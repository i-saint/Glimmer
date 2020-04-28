#include "pch.h"
#include "lptUtils.h"

namespace lpt {

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
} // namespace lpt
