#pragma once
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

} // namespace lpt
