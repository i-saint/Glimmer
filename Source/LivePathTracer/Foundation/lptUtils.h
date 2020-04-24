#pragma once

namespace lpt {

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


template<class Body>
bool Map(ID3D12Resource* res, UINT subresource, const D3D12_RANGE* range, const Body& body)
{
    using arg_t = lambda_traits<Body>::arg<0>::type;
    arg_t dst;
    if (SUCCEEDED(res->Map(subresource, range, (void**)&dst))) {
        body(dst);
        res->Unmap(subresource, nullptr);
        return true;
    }
    else {
        return false;
    }
}
template<class Body>
bool Map(ID3D12Resource* res, const Body& body)
{
    return Map(res, 0, nullptr, body);
}

} // namespace lpt
