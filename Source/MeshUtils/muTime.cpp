#include "pch.h"
#include "muMisc.h"
#include "muTime.h"

namespace mu {

nanosec Now()
{
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

Timer::Timer()
{
    reset();
}

void Timer::reset()
{
    m_begin = Now();
}

float Timer::elapsed() const
{
    return NS2S(Now() - m_begin);
}


ProfileTimer::ProfileTimer(const char *mes, ...)
{
    va_list args;
    va_start(args, mes);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), mes, args);
    va_end(args);

    m_message = buf;
}

ProfileTimer::~ProfileTimer()
{
    float t = elapsed() * 1000.0f;
    Print("%s - %.2fms\n", m_message.c_str(), t);
}

} // namespace mu
