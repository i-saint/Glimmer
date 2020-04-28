#include "pch.h"
#include "lptWindow.h"
#include "lptEntity.h"

namespace lpt {

static std::vector<Window*> g_windows;

static Window* FindWindow(void* handle)
{
    auto it = std::find_if(g_windows.begin(), g_windows.end(),
        [handle](Window* w) { return w->getHandle() == handle; });
    if (it != g_windows.end())
        return *it;
    else
        return nullptr;
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

#ifdef _WIN32

static LRESULT CALLBACK lptMsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    bool handled = false;
    auto handle = [&](auto& body) {
        if (auto* w = FindWindow(hwnd)) {
            body(w);
            handled = true;
        }
    };

    switch (msg)
    {
    case WM_CLOSE:
        ::DestroyWindow(hwnd);
        handle([&](Window* w) {
            w->onClose();
        });
        break;
    case WM_DESTROY:
        handle([&](Window* w) {
            w->onDestroy();
        });
        break;
    case WM_SIZE:
        handle([&](Window* w) {
            w->onResize(LOWORD(lParam), HIWORD(lParam));
            if (wParam == SIZE_MAXIMIZED)
                w->onMaximize();
            else if (wParam == SIZE_MINIMIZED)
                w->onMinimize();
        });
        break;
    case WM_KEYDOWN:
        handle([&](Window* w) { w->onKeyDown((int)wParam); });
        break;
    case WM_KEYUP:
        handle([&](Window* w) { w->onKeyUp((int)wParam); });
        break;

    case WM_MOUSEMOVE:
        handle([&](Window* w) { w->onMouseMove(LOWORD(lParam), HIWORD(lParam)); });
        break;
    case WM_LBUTTONDOWN:
        handle([&](Window* w) { w->onMouseDown(1); });
        break;
    case WM_RBUTTONDOWN:
        handle([&](Window* w) { w->onMouseDown(2); });
        break;
    case WM_MBUTTONDOWN:
        handle([&](Window* w) { w->onMouseDown(3); });
        break;
    case WM_LBUTTONUP:
        handle([&](Window* w) { w->onMouseUp(1); });
        break;
    case WM_RBUTTONUP:
        handle([&](Window* w) { w->onMouseUp(2); });
        break;
    case WM_MBUTTONUP:
        handle([&](Window* w) { w->onMouseUp(3); });
        break;

    default:
        break;
    }

    if (!handled)
        return ::DefWindowProc(hwnd, msg, wParam, lParam);
    else
        return 0;
}

bool Window::open(int width, int height)
{
    if (m_hwnd)
        return true; // already opened

    const CHAR* title = m_name.c_str();
    const CHAR* class_name = "lpt";
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;

    WNDCLASS wc{};
    wc.lpfnWndProc = &lptMsgProc;
    wc.hInstance = ::GetModuleHandle(nullptr);
    wc.lpszClassName = class_name;
    //wc.hIcon = (HICON)::LoadImageA(nullptr, "lpt.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE | LR_SHARED);

    if (::RegisterClass(&wc) != 0) {
        RECT r{ 0, 0, (LONG)width, (LONG)height };
        ::AdjustWindowRect(&r, style, false);

        int window_width = r.right - r.left;
        int window_height = r.bottom - r.top;
        m_hwnd = ::CreateWindowEx(0, class_name, title, style, CW_USEDEFAULT, CW_USEDEFAULT, window_width, window_height, nullptr, nullptr, wc.hInstance, nullptr);
        if (m_hwnd) {
            ::ShowWindow(m_hwnd, SW_SHOWNORMAL);
            m_width = width;
            m_height = height;
        }
    }
    return m_hwnd != nullptr;
}

void Window::close()
{
    if (m_hwnd) {
        ::DestroyWindow(m_hwnd);
        onClose();
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

void* Window::getHandle()
{
    return m_hwnd;
}

#endif // _WIN32

void Window::addCallback(IWindowCallback* cb)
{
    m_callbacks.push_back(cb);
}

void Window::removeCallback(IWindowCallback* cb)
{
    erase(m_callbacks, cb);
}

bool Window::isClosed()
{
    return m_hwnd == nullptr;
}

void Window::onRefCountZero()
{
    delete this;
}

void Window::onDestroy()
{
    m_hwnd = nullptr;

    eachCallback([&](IWindowCallback* cb) {
        cb->onDestroy();
    });
}

void Window::onClose()
{
    eachCallback([&](IWindowCallback* cb) {
        cb->onClose();
    });
}

void Window::onResize(int w, int h)
{
    m_width = w;
    m_height = h;

    eachCallback([&](IWindowCallback* cb) {
        cb->onResize(w, h);
    });
}
void Window::onMinimize()
{
    eachCallback([&](IWindowCallback* cb) {
        cb->onMinimize();
    });
}
void Window::onMaximize()
{
    eachCallback([&](IWindowCallback* cb) {
        cb->onMaximize();
    });
}

void Window::onKeyDown(int key)
{
    eachCallback([&](IWindowCallback* cb) {
        cb->onKeyDown(key);
    });
}
void Window::onKeyUp(int key)
{
    eachCallback([&](IWindowCallback* cb) {
        cb->onKeyDown(key);
    });
}

void Window::onMouseMove(int x, int y)
{
    eachCallback([&](IWindowCallback* cb) {
        cb->onMouseMove(x, y);
    });
}
void Window::onMouseDown(int button)
{
    eachCallback([&](IWindowCallback* cb) {
        cb->onMouseDown(button);
    });
}
void Window::onMouseUp(int button)
{
    eachCallback([&](IWindowCallback* cb) {
        cb->onMouseUp(button);
    });
}


} // namespace lpt

lptAPI lpt::IWindow* lptCreateWindow_(int width, int height)
{
    return new lpt::Window(width, height);
}

