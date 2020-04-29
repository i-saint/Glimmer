#pragma once
#include <vector>
#include <map>
#include <algorithm>

namespace mu {

#define muEnableIf(...) std::enable_if_t<__VA_ARGS__, bool> = true

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


template<class A1, class Body>
inline void each(A1& a1, const Body& body)
{
    for (auto& v : a1)
        body(v);
}
template<class A1, class A2, class Body>
inline void each(A1& a1, A2& a2, const Body& body)
{
    size_t n = a1.size();
    if (n != a2.size())
        return;
    for (size_t i = 0; i < n; ++i)
        body(a1[i], a2[i]);
}
template<class A1, class A2, class A3, class Body>
inline void each(A1& a1, A2& a2, A3& a3, const Body& body)
{
    size_t n = a1.size();
    if (n != a2.size() || n != a3.size())
        return;
    for (size_t i = 0; i < n; ++i)
        body(a1[i], a2[i], a3[i]);
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

// VERY SLOW. the number of input elements should be up to 10 or so.
template<class Iter>
inline Iter unique_unsorted(Iter begin, Iter end)
{
    for (auto i = begin; i + 1 < end; ++i) {
        for (;;) {
            auto p = std::find(i + 1, end, *i);
            if (p != end) {
                // shift elements
                auto q = p + 1;
                while (q != end)
                    *p++ = std::move(*q++);
                --end;
            }
            else
                break;
        }
    }
    return end;
}

} // namespace mu
