#pragma once
#include "lptEntity.h"

namespace lpt {

class Window : public RefCount<IWindow>
{
public:
    Window(int width, int height);
    ~Window();

    bool open(int width, int height) override;
    void close() override;
    void processMessages() override;

    bool isClosed() override;
    void* getHandle() override;

    void onRefCountZero() override;

public:
#ifdef _WIN32
    HWND m_hwnd = nullptr;
#endif
};

} // namespace lpt
