#pragma once
#include "gptEntity.h"

namespace gpt {

class Window : public RefCount<IWindow>
{
public:
    Window(int width, int height, WindowFlag flags);
    ~Window();

    bool open(int width, int height, WindowFlag flags);
    void close() override;
    void processMessages() override;

    void addCallback(IWindowCallback* cb) override;
    void removeCallback(IWindowCallback* cb) override;

    bool isClosed() const override;
    int getWidth() const override;
    int getHeight() const override;
    void* getHandle() override;
    void onRefCountZero() override;

    void onDestroy();
    void onClose();
    void onResize(int w, int h);
    void onMinimize();
    void onMaximize();
    void onKeyDown(int key);
    void onKeyUp(int key);
    void onMouseMove(int x, int y);
    void onMouseDown(int button);
    void onMouseUp(int button);


    template<class Body>
    void eachCallback(const Body& body)
    {
        for (auto* cb : m_callbacks)
            body(cb);
    }

public:
    int m_width = 0;
    int m_height = 0;
    std::vector<IWindowCallback*> m_callbacks;

#ifdef _WIN32
    HWND m_hwnd = nullptr;
#endif
};
gptDefRefPtr(Window);
gptDefBaseT(Window, IWindow)

} // namespace gpt
