#include "pch.h"
#include "gptUtils.h"

namespace gpt {

RawVector<char> GetDummyBuffer_(size_t size)
{
    static RawVector<char> s_buffer;
    static std::mutex s_mutex;
    {
        std::unique_lock<std::mutex> lock(s_mutex);
        if (s_buffer.size() < size)
            s_buffer.resize_zeroclear(size);
    }
    return s_buffer;
}

int IndexAllocator::allocate()
{
    int ret;
    if (!m_vacants.empty()) {
        ret = m_vacants.back();
        m_vacants.pop_back();
    }
    else {
        ret = m_capacity++;
    }
    return ret;
}

void IndexAllocator::free(int v)
{
    if (v >= 0)
        m_vacants.push_back(v);
}

} // namespace gpt
