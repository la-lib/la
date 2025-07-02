#include "la.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef CreateWindow

#include <Psapi.h> // psapi.lib

#if !defined(__clang__)
extern "C" int _fltused = 0;
#endif


namespace la::os {
constexpr wchar_t WINDOW_CLASSNAME[]{L"_"};
constexpr unsigned DRAW_COLOR_MODE = 4; // ARGB

constexpr unsigned char KEY_MAX = static_cast<unsigned char>(Key::__MAX__);
unsigned char key_array[KEY_MAX]{0};

static LRESULT CALLBACK win_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    Window *win = reinterpret_cast<Window *>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_MOUSEMOVE:
        win->event.on_resize(
            /* Window & */ *win, 
            /* X pos    */ LOWORD(lparam),
            /* Y pos    */ HIWORD(lparam));
        return 0;

    case WM_KEYDOWN:
        if (wparam < KEY_MAX) {
            key_array[wparam] = 1;
            return 0;
        }
        break;

    case WM_KEYUP:
        if (wparam < KEY_MAX) {
            key_array[wparam] = 0;
            win->event.on_key_up(*win, static_cast<Key>(wparam));
        }
        return 0;


    case WM_SIZE: {
        RECT rect;
        GetClientRect(hwnd, &rect);
        int w = rect.right - rect.left;
        int h = rect.bottom - rect.top;

        win->proc_recreate_framebuffer(w, h);
        InvalidateRect(hwnd, nullptr, FALSE);
        UpdateWindow(hwnd); // Force draw rn

        return TRUE;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        BitBlt(ps.hdc, 0, 0, win->framebuffer.width, win->framebuffer.height,
               reinterpret_cast<HDC>(win->framebuffer.hdc), 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SETFOCUS:
        win->event.on_focus_change(*win, true);
        return 0;

    case WM_KILLFOCUS:
        win->event.on_focus_change(*win, false);
        return 0;

    case WM_NCCREATE: {
#ifdef _PSAPI_H_ // Clear resources
        SetProcessWorkingSetSize(GetCurrentProcess(), -1, -1);
        EmptyWorkingSet(GetCurrentProcess());
#endif
        auto create_struct = reinterpret_cast<CREATESTRUCT *>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)(create_struct->lpCreateParams));
        return TRUE;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    } // switch

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

void *alloc(size_t size) noexcept {
    return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
} // alloc

void free(void *ptr, size_t /* size */) noexcept {
    VirtualFree(ptr, 0, MEM_RELEASE);
} // free

void exit_process(int error_code) noexcept {
    ExitProcess(error_code);
} // exit_process

void sleep(unsigned ms) noexcept {
    Sleep(ms);
} // sleep

double monotonic_seconds() noexcept {
    static LARGE_INTEGER freq = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return f;
    }();
    LARGE_INTEGER counter;
    if (!QueryPerformanceCounter(&counter)) return 0.0;
    return static_cast<double>(counter.QuadPart) / freq.QuadPart;
} // monotonic_seconds


Window::Window(int width, int height) noexcept
    : native{nullptr, nullptr}, 
    framebuffer{
        .hdc{nullptr}, 
        .bmp{nullptr}, 
        .pixels{nullptr},
        .width{0}, 
        .height{0}}, 
    event{}
    {
    // Note: Creating windows outside the main thread causes a crash
    // because we would need to replace the `static` keyword with `thread_local`
    // to allow window creation on other threads. However, using `thread_local`
    // requires linking against the runtime library (msvcrt.lib), which is
    // incompatible with keeping the `la` library freestanding.
    static struct ThreadInstance {
      private:
        HMODULE m_hinstance;

      public:
          explicit inline ThreadInstance() noexcept 
              : m_hinstance{GetModuleHandleW(nullptr)}
          {
            // register window class once per thread
            WNDCLASS wc{
                .style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
                .lpfnWndProc = win_proc,
                .hInstance = get_hinstance(),
                .lpszClassName = WINDOW_CLASSNAME,
            };
            if (!RegisterClassW(&wc))
                m_hinstance = nullptr;
        }
        const HMODULE get_hinstance() const noexcept { return m_hinstance; }
    } thread_ctx;

    if (!thread_ctx.get_hinstance())
        return;

    // Window
    native.hwnd = reinterpret_cast<void *>(
        CreateWindowExW(
        /* dwExStyle    */ 0,
        /* lpClassName  */ WINDOW_CLASSNAME,
        /* lpWindowName */ WINDOW_CLASSNAME,
        /* dwStyle      */ WS_OVERLAPPEDWINDOW,
        /* x            */ CW_USEDEFAULT,
        /* y            */ CW_USEDEFAULT,
        /* width        */ width,
        /* height       */ height,
        /* hWndParent   */ nullptr,
        /* hMenu        */ nullptr,
        /* hInstance    */ thread_ctx.get_hinstance(),
        /* lpParam      */ this));
    native.hdc = reinterpret_cast<void *>(GetDC(reinterpret_cast<HWND>(native.hwnd)));
    if (!native.hdc) 
        return;
    proc_recreate_framebuffer(width, height);
} // Window

Window::~Window() noexcept {
    // Framebuffer
    DeleteObject(reinterpret_cast<HBITMAP>(framebuffer.bmp));
    DeleteDC(reinterpret_cast<HDC>(framebuffer.hdc));
    // Window
    ReleaseDC(reinterpret_cast<HWND>(native.hwnd), reinterpret_cast<HDC>(native.hdc));
    DestroyWindow(reinterpret_cast<HWND>(native.hwnd));
} // ~Window

bool Window::is_poll_events() const noexcept {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) return false;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return true;
} // is_poll_events

void Window::proc_show(bool show) const noexcept {
    ShowWindow(reinterpret_cast<HWND>(native.hwnd), 
               show ? SW_SHOW : SW_HIDE);
} // proc_show

void Window::proc_swap_buffers() const noexcept {
    InvalidateRect(reinterpret_cast<HWND>(native.hwnd), nullptr, FALSE);
    UpdateWindow(reinterpret_cast<HWND>(native.hwnd));
} // proc_swap_buffers

void Window::set_pixel(int x, int y, uint32_t color) noexcept {
    if (!framebuffer.pixels) return;

    if (x >= 0 && y >= 0 && x < framebuffer.width && y < framebuffer.height) {
        auto ptr = static_cast<uint32_t *>(framebuffer.pixels);
        ptr[y * framebuffer.width + x] = color;
    }
} // set_pixel

void Window::proc_recreate_framebuffer(int width, int height) noexcept {
    // free
    if (framebuffer.bmp) DeleteObject(reinterpret_cast<HBITMAP>(framebuffer.bmp));
    if (framebuffer.hdc) DeleteDC(reinterpret_cast<HDC>(framebuffer.hdc));

    // Bitmap
    BITMAPINFO bmi{.bmiHeader{.biSize = sizeof(BITMAPINFOHEADER),
                              .biWidth = width,
                              .biHeight = -height, // top-down
                              .biPlanes = 1,
                              .biBitCount = 32,
                              .biCompression = BI_RGB}};
    // Framebuffer
    framebuffer.width = width;
    framebuffer.height = height;

    // DC
    framebuffer.hdc = reinterpret_cast<void *>(CreateCompatibleDC(reinterpret_cast<HDC>(native.hdc)));

    // Bitmap
    framebuffer.bmp = reinterpret_cast<void *>(CreateDIBSection(
        /* hdc      */ reinterpret_cast<HDC>(framebuffer.hdc),
        /* pbmi     */ &bmi,
        /* usage    */ DIB_RGB_COLORS,
        /* ppvBits  */ &framebuffer.pixels,
        /* hSection */ NULL,
        /* offset   */ 0));

    if (!framebuffer.bmp) return;

    SelectObject(reinterpret_cast<HDC>(framebuffer.hdc), reinterpret_cast<HBITMAP>(framebuffer.bmp));
} // proc_recreate_framebuffer

} // namespace la::os
#endif // _WIN32