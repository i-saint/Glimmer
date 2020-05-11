#pragma once
#include <string>

namespace mu {

using nanosec = uint64_t;
nanosec Now();
inline float NS2MS(nanosec ns) { return (float)((double)ns / 1000000.0); }
inline float NS2S(nanosec ns) { return (float)((double)ns / 1000000000.0); }
inline double NS2Sd(nanosec ns) { return ((double)ns / 1000000000.0); }
inline nanosec MS2NS(float ms) { return (nanosec)((double)ms * 1000000.0); }
inline nanosec S2NS(float s) { return (nanosec)((double)s * 1000000000.0); }
inline nanosec S2NS(double s) { return (nanosec)(s * 1000000000.0); }


class Timer
{
public:
    Timer();
    void reset();
    float elapsed() const; // in sec

private:
    nanosec m_begin = 0;
};

class ProfileTimer : public Timer
{
public:
    ProfileTimer(const char *mes, ...);
    ~ProfileTimer();

private:
    std::string m_message;
};

} // namespace mu

