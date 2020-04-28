#pragma once
#include "lptSettings.h"
#include "lptInterface.h"

namespace lpt {

#define lptEnableIf(...) std::enable_if_t<__VA_ARGS__, bool> = true

template<class R, class... Args>
struct lambda_traits_impl
{
    using return_type = R;
    enum { arity = sizeof...(Args) };

    template<size_t i> struct arg
    {
        using type = typename std::tuple_element<i, std::tuple<Args...>>::type;
    };
};

template<class L>
struct lambda_traits : lambda_traits<decltype(&L::operator())>
{};

template<class R, class T, class... Args>
struct lambda_traits<R(T::*)(Args...)> : lambda_traits_impl<R, Args...>
{};

template<class R, class T, class... Args>
struct lambda_traits<R(T::*)(Args...) const> : lambda_traits_impl<R, Args...>
{};


template<class Container, class Body>
inline void each(Container& dst, const Body& body)
{
    for (auto& v : dst)
        body(v);
}

template<class Container, class Body>
inline void each_ref(Container& dst, const Body& body)
{
    for (auto& v : dst)
        body(*v);
}

template<class Container, class Body>
inline void each_with_index(Container& dst, const Body& body)
{
    int i = 0;
    for (auto& v : dst)
        body(v, i++);
}

template<class Container, class Condition>
inline bool erase_if(Container& dst, const Condition& cond)
{
    size_t size_prev = dst.size();
    dst.erase(
        std::remove_if(dst.begin(), dst.end(), cond),
        dst.end());
    return dst.size() != size_prev;
}

template<class K, class V, class Condition>
inline void erase_if(std::map<K, V>& dst, const Condition& cond)
{
    std::vector<K> keys_to_erase;
    for (auto& kvp : dst) {
        if (cond(kvp.second))
            keys_to_erase.push_back(kvp.first);
    }
    for (auto& k : keys_to_erase)
        dst.erase(k);
    return !keys_to_erase.empty();
}

template<class Container, class T>
inline bool erase(Container& dst, T v)
{
    auto it = std::find_if(dst.begin(), dst.end(), [&](const auto& e) { return e == v; });
    if (it != dst.end()) {
        dst.erase(it);
        return true;
    }
    else {
        return false;
    }
}


template<class Container, class Condition>
inline bool find_any(const Container& src, const Condition& cond)
{
    for (auto& v : src) {
        if (cond(v))
            return true;
    }
    return false;
}



template<class T, class U, lptEnableIf(std::is_base_of<U, T>::value)>
ref_ptr<T>& cast(ref_ptr<U>& v)
{
    return (ref_ptr<T>&)v;
}
template<class T, class U, lptEnableIf(std::is_base_of<U, T>::value)>
const ref_ptr<T>& cast(const ref_ptr<U>& v)
{
    return (const ref_ptr<T>&)v;
}


template<class T, class R> int GetID(ref_ptr<T, R>& p) { return p ? p->m_id : -1; }
template<class T> int GetID(T* p) { return p ? p->m_id : -1; }

template<class T>
class InternalReleaser
{
public:
    static void addRef(T* v) { v->addRefInternal(); }
    static void release(T* v) { v->releaseInternal(); }
};

template<class T>
class EntityList
{
public:
    using pointer = ref_ptr<T, InternalReleaser<T>>;

    size_t size() const { return m_active.size(); }
    size_t capacity() const { return m_entities.size(); }
    T* operator[](size_t i) { return m_active[i]; }
    auto begin() { return m_active.begin(); }
    auto end() { return m_active.end(); }

    void clear()
    {
        m_entities.clear();
        m_active.clear();
        m_vacants.clear();
    }

    int insert(T* p)
    {
        int id;
        if (!m_vacants.empty()) {
            id = m_vacants.back();
            m_vacants.pop_back();
            m_entities[id] = p;
        }
        else {
            id = (int)m_entities.size();
            m_entities.push_back(p);
        }
        p->m_id = id;
        m_active.push_back(p);
        return id;
    }

    void eraseUnreferenced()
    {
        auto is_zero_ref = [](T* p) {
            return p->getRef() <= 0 && p->getRefInternal() <= 1;
        };
        erase_if(m_active, is_zero_ref);

        each_with_index(m_entities, [&](auto& p, int i) {
            if (is_zero_ref(p)) {
                p = nullptr;
                m_vacants.push_back((int)i);
            }
            });
    }

    void clearDirty()
    {
        for (auto p : m_active)
            p->clearDirty();
    }

private:
    std::vector<pointer> m_entities;
    std::vector<T*> m_active;
    std::vector<int> m_vacants;
};

class IndexAllocator
{
public:
    int allocate();
    void free(int v);

private:
    std::vector<int> m_vacants;
    int m_capacity = 0;
};

} // namespace lpt
