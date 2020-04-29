#pragma once

namespace gpt {

std::string GetErrorLog();
void SetErrorLog(const char *format, ...);
void SetErrorLog(const std::string& str);
void ClearErrorLog();
void DebugPrintImpl(const char *fmt, ...);

#ifdef gptDebug
    #define gptDebugPrint(...) DebugPrintImpl(__VA_ARGS__)
#else
    #define gptDebugPrint(...)
#endif

} // namespace gpt
