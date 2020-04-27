#include "pch.h"
#include "lptWindow.h"
#include "lptEntity.h"

namespace lpt {

static std::vector<IWindow*> g_windows;

static IWindow* FindWindow(void* handle)
{
    auto it = std::find_if(g_windows.begin(), g_windows.end(),
        [handle](IWindow* w) { return w->getHandle() == handle; });
    if (it != g_windows.end())
        return *it;
    else
        return nullptr;
}

#ifdef _WIN32

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

    static LRESULT CALLBACK MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HWND m_hwnd = nullptr;
};


LRESULT CALLBACK Window::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CLOSE:
        if (auto *w = FindWindow(hwnd)) {
            w->close();
        }
        return 0;

    default:
        return ::DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

Window::Window(int width, int height)
{
    g_windows.push_back(this);
    open(width, height);
}

Window::~Window()
{
    close();
    g_windows.erase(std::find(g_windows.begin(), g_windows.end(), this));
}

bool Window::open(int width, int height)
{
    if (m_hwnd)
        return true; // already opened

    const CHAR* title = m_name.c_str();
    const CHAR* class_name = "lpt";
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;

    WNDCLASS wc{};
    wc.lpfnWndProc = &Window::MsgProc;
    wc.hInstance = ::GetModuleHandle(nullptr);
    wc.lpszClassName = class_name;
    //wc.hIcon = (HICON)::LoadImageA(nullptr, "lpt.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE | LR_SHARED);

    if (RegisterClass(&wc) != 0) {
        RECT r{ 0, 0, (LONG)width, (LONG)height };
        ::AdjustWindowRect(&r, style, false);

        int window_width = r.right - r.left;
        int window_height = r.bottom - r.top;
        m_hwnd = ::CreateWindowEx(0, class_name, title, style, CW_USEDEFAULT, CW_USEDEFAULT, window_width, window_height, nullptr, nullptr, wc.hInstance, nullptr);
        if (m_hwnd) {
            ::ShowWindow(m_hwnd, SW_SHOWNORMAL);
        }
    }
    return m_hwnd != nullptr;
}

void Window::close()
{
    if (m_hwnd) {
        ::DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void Window::processMessages()
{
    MSG msg;
    while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }
}

bool Window::isClosed()
{
    return m_hwnd == nullptr;
}

void* Window::getHandle()
{
    return m_hwnd;
}

void Window::onRefCountZero()
{
    delete this;
}

#endif // _WIN32

} // namespace lpt

lptAPI lpt::IWindow* lptCreateWindow_(int width, int height)
{
    return new lpt::Window(width, height);
}

