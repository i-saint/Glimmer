#pragma once

namespace lpt {

std::string GetErrorLog();
void SetErrorLog(const char *format, ...);
void SetErrorLog(const std::string& str);
void ClearErrorLog();
void DebugPrintImpl(const char *fmt, ...);

#ifdef lptDebug
    #define lptDebugPrint(...) DebugPrintImpl(__VA_ARGS__)
#else
    #define lptDebugPrint(...)
#endif

} // namespace lpt
