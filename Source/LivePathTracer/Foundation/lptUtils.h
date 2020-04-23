#pragma once

namespace lpt {

template<class Container, class Condition>
inline void erase_if(Container& dst, const Condition& cond)
{
    dst.erase(
        std::remove_if(dst.begin(), dst.end(), cond),
        dst.end());
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

} // namespace lpt
