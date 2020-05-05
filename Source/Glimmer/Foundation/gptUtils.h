#pragma once
#include "gptSettings.h"
#include "gptInterface.h"

namespace gpt {

using mu::lambda_traits;
using mu::each;
using mu::each_ref;
using mu::each_with_index;
using mu::erase;
using mu::erase_if;

template<class T, muEnableIf(std::is_enum<T>::value)>
inline void set_flag(uint32_t& dst, T flag, bool v)
{
    if (v)
        (uint32_t&)dst |= (uint32_t)flag;
    else
        (uint32_t&)dst &= ~(uint32_t)flag;
}
template<class T, muEnableIf(std::is_enum<T>::value)>
inline void set_flag(int& dst, T flag, bool v)
{
    set_flag((uint32_t&)dst, flag, v);
}

template<class T, muEnableIf(std::is_enum<T>::value)>
inline bool get_flag(uint32_t src, T flag)
{
    return (src & (uint32_t)flag) != 0;
}
template<class T, muEnableIf(std::is_enum<T>::value)>
inline bool get_flag(int src, T flag)
{
    return get_flag((uint32_t)src, flag);
}
template<class T, muEnableIf(std::is_enum<T>::value)>
inline bool get_flag(T src, T flag)
{
    return get_flag((uint32_t)src, flag);
}


template<class T, class U, muEnableIf(std::is_base_of<U, T>::value)>
ref_ptr<T>& cast(ref_ptr<U>& v)
{
    return (ref_ptr<T>&)v;
}
template<class T, class U, muEnableIf(std::is_base_of<U, T>::value)>
const ref_ptr<T>& cast(const ref_ptr<U>& v)
{
    return (const ref_ptr<T>&)v;
}


RawVector<char> GetDummyBuffer_(size_t n);
template<class T>
inline T* GetDummyBuffer(size_t n)
{
    auto& buf = GetDummyBuffer_(sizeof(T) * n);
    return (T*)buf.data();
}


template<class T, class R> int GetID(ref_ptr<T, R>& p) { return p ? p->getID() : -1; }
template<class T> int GetID(T* p) { return p ? p->getID() : -1; }

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
        p->setID(id);
        m_active.push_back(p);
        return id;
    }

    void eraseUnreferenced()
    {
        auto is_zero_ref = [](T* p) {
            return p && p->getRef() <= 0 && p->getRefInternal() <= 1;
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

} // namespace gpt
