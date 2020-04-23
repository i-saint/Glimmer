#include "pch.h"
#include "lptEntity.h"

namespace lpt {

void GlobalSettings::enableDebugFlag(DebugFlag v)
{
    debug_flags = debug_flags | (uint32_t)v;
}

void GlobalSettings::disableDebugFlag(DebugFlag v)
{
    debug_flags = debug_flags & (~(uint32_t)v);
}

bool GlobalSettings::hasDebugFlag(DebugFlag v) const
{
    return (debug_flags & (uint32_t)v) != 0;
}

bool GlobalSettings::hasFlag(GlobalFlag v) const
{
    return (flags & (uint32_t)v) != 0;
}

GlobalSettings& GetGlobals()
{
    static GlobalSettings s_globals;
    return s_globals;
}




} // namespace lpt 
