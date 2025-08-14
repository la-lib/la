#ifndef __LA_HEADER_GUARD
#define __LA_HEADER_GUARD

/*
    Use `LA_NOSTD` macro for freestanding mode builds: no std libraries, pure bare metal.
    Use `LA_CONSOLE` macro to enable terminal output.
*/

#ifdef _CONSOLE
#   define LA_CONSOLE
#endif

// --------------------------- Macros -----------------------------------------

#ifndef _NDEBUG
#   define LA_ASSERT(x, msg) if (!(x)) ::la::panic_process(msg, __LINE__)
#else
#   define LA_ASSERT(x, msg)
#endif

// I FUCKING HATE MICROSOFT PRODUCTS.
#if defined(_MSC_VER) && defined(LA_NOSTD)
extern "C" void* __cdecl memset(void* dest, int ch, size_t count);
#endif

// -------------------- C++17 feature detection -------------------------------
// MSVC
#if defined(_MSC_VER)
#   define COMPILER_MSVC _MSC_VER

#   if defined(_MSVC_LANG)
#       define CPP_VERSION _MSVC_LANG
#   else
#       define CPP_VERSION __cplusplus
#   endif

#   if CPP_VERSION >= 201703L
#       define LA_CXX_17
#   endif 

// CLang
#elif defined(__clang__)
#   if __cplusplus >= 201703L
#       define LA_CXX_17
#   endif

// GNU
#elif defined(__GNUC__)
#   if __cplusplus >= 201703L
#       define LA_CXX_17
#   endif
#endif // C++17 feature detection

// Replace `main` with `WinMain`.
#if defined(_MSC_VER) && !defined(_DEBUG)
/* Detected Windows GUI application without console.
   The main() function was quietly changed to WinMain()
   main()'s arguments are NOT AVAILABLE. */
#define main() __stdcall WinMain( la::native::HINSTANCE hInstance,            \
                                  la::native::HINSTANCE hPrevInstance,        \
                                  la::native::LPSTR lpCmdLine,                \
                                  int nCmdShow)
#endif //_MSC_VER && !_DEBUG && !_CONSOLE

// ------------------------ `restrict` keyword --------------------------------

// `LA_RESTRICT` keyword
#if CPP_VERSION < 202302L // `restrict` was added in C++23
#  ifndef LA_RESTRICT
#    ifdef __GNUC__         // GNU
#      define LA_RESTRICT __restrict__
#    elif defined(_MSC_VER) // MSVC
#      define LA_RESTRICT __restrict
#    else                   // Fallback
#      define LA_RESTRICT
#    endif
#  endif // LA_RESTRICT
#endif // __cplusplus < 202302

#if defined(LA_CXX_17)
#   define LA_FALLTHROUGH [[fallthrough]]
#elif defined(__GNUC__) && __GNUC__ >= 7
#   define LA_FALLTHROUGH __attribute__((fallthrough))
#else
#   define LA_FALLTHROUGH ((void)0)
#endif

// ------------------- Freestanding-friendly Includes -------------------------

#include <stddef.h> // size_t
#include <stdint.h> // uint32_t

// -------------------- Attribute/constexpr defines ---------------------------

#ifdef LA_CXX_17 // Define `LA_CXX_17` macro if C++17 or above used
#   define LA_NO_DISCARD [[nodiscard]]
#   define LA_CONSTEXPR constexpr
#   define LA_CONSTEXPR_VAR constexpr
#else // fallback
#   define LA_NO_DISCARD
#   define LA_CONSTEXPR inline
#   define LA_CONSTEXPR_VAR const
#endif // // Attribute/constexpr defines

#ifdef _CONSOLE
#   define LA_CONSOLE
#endif

// -------------------------- Namespace `la` ----------------------------------
// -------------------------- OS-dependent ------------------------------------
namespace la {
    struct Window;

    namespace native {
        // ---------------- Win32 Forward Declarations ------------------------
#if defined(_WIN32)
    // Forward declarations to avoid <Windows.h>
        typedef struct HINSTANCE__* HINSTANCE;
        typedef char* LPSTR;
#endif
        enum class WindowBackend : int {
            Unknown,
            X11,
            WindowsApi,
            Cocoa,
            AndroidNdk,
        }; // enum class WindowBackend

        LA_CONSTEXPR_VAR static WindowBackend
            WINDOW_BACKEND =
            // Choose via macros
#if defined(__linux__)     // Linux
            WindowBackend::X11;
#elif defined(_WIN32)      // Windows
            WindowBackend::WindowsApi;
#elif defined(__APPLE__)   // Apple
            WindowBackend::Cocoa;
#elif defined(__ANDROID__) // Android
            WindowBackend::AndroidNdk;
#else                      // Unknown
            WindowBackend::Unknown;
#endif

        // Check if `Window Backend` is known at compile-time in C++17 or above
#ifdef LA_CXX_17
        static_assert(WINDOW_BACKEND != WindowBackend::Unknown && "Unknown window backend");
#endif

        // Forward declarations
        struct Window;
        struct Framebuffer;
        struct OpenglContext;

        // Window specialized functions

        void render_software(const ::la::Window&) noexcept;
        void render_opengl(const ::la::Window&) noexcept;
        void on_geometry_change(::la::Window&, int w, int h) noexcept;

    } // namespace native






    // --------------------------- Allocate/Free ------------------------------

    LA_NO_DISCARD void* alloc(size_t size) noexcept;
    void free(void* ptr, size_t size) noexcept;


    // --------------------------- Processing ---------------------------------

    void exit_process(int error_code) noexcept; // Exit current process
    void panic_process(const char* explain_msg, int error_code) noexcept;


    // --------------------------- Time ---------------------------------------

    void sleep(unsigned ms) noexcept;
    LA_NO_DISCARD double get_monotonic_secs() noexcept;


    // --------------------------- Misc Functions -----------------------------

    LA_NO_DISCARD bool is_battery_in_use() noexcept;
    void reset_workset() noexcept;

    // --------------------------- Input/Output -------------------------------

    void print(const char* msg, size_t msg_length) noexcept;

    // ---------------------- SIMD system -------------------------------------

struct Simd {
public:
    using AddFloat = void (*)(const float* LA_RESTRICT a, 
                              const float* LA_RESTRICT b,
                              float* LA_RESTRICT out, 
                              size_t count);
    using AddInt32 = void (*)(const int32_t* LA_RESTRICT a, 
                              const int32_t* LA_RESTRICT b,
                              int32_t* LA_RESTRICT out,
                              size_t count);

    using FillFloat = void (*)(float* out, float value, size_t count);
    using FillInt32 = void (*)(int32_t* out, int32_t value, size_t count);

    Simd() = delete;

    // ======= Hardware dependent =======
    LA_NO_DISCARD static bool has_sse() noexcept;
    LA_NO_DISCARD static bool has_sse2() noexcept;
    LA_NO_DISCARD static bool has_avx() noexcept;
    LA_NO_DISCARD static bool has_avx2() noexcept;
    LA_NO_DISCARD static bool has_avx512bw() noexcept;

    AddFloat static inline choose_add_float(bool sse, bool avx) noexcept {
        if (avx) return &add_t<float, 8>::apply;
        if (sse) return &add_t<float, 4>::apply;
                 return &add_t<float, 1>::apply;
    }

    AddInt32 static inline choose_add_int32(bool sse2, bool avx2) noexcept {
        if (avx2) return &add_t<int32_t, 8>::apply;
        if (sse2) return &add_t<int32_t, 4>::apply;
                  return &add_t<int32_t, 1>::apply;
    }

    FillFloat static inline choose_fill_float(bool sse, bool avx) noexcept {
        if (avx)  return &fill_t<float, 8>::apply;
        if (sse)  return &fill_t<float, 4>::apply;
                  return &fill_t<float, 1>::apply;
    }

    FillInt32 static inline choose_fill_int32(bool sse2, bool avx2) noexcept {
        if (avx2) return &fill_t<int32_t, 8>::apply;
        if (sse2) return &fill_t<int32_t, 4>::apply;
                  return &fill_t<int32_t, 1>::apply;
    }

    // ----------------------------- Add --------------------------------------

    // Base template: fallback scalar
    template<typename T, size_t Width> struct add_t {
        static inline void apply(const T* LA_RESTRICT a, const T* LA_RESTRICT b, T* LA_RESTRICT out, size_t count) noexcept {
            for (size_t i = 0; i < count; ++i) out[i] = a[i] + b[i];
        } // apply
    }; // struct add_t

    // Specialization for float + SSE
    template<> struct add_t<float, 4> {
        static void apply(const float* LA_RESTRICT a, const float* LA_RESTRICT b, float* LA_RESTRICT out, size_t count) noexcept;
    };

    // Specialization for float + AVX
    template<> struct add_t<float, 8> {
    static void apply(const float* LA_RESTRICT a, const float* LA_RESTRICT b, float* LA_RESTRICT out, size_t count) noexcept;
    };

    // Specialization for int32 + AVX2
    template<> struct add_t<int32_t, 8> {
        static void apply(const int32_t* LA_RESTRICT a, const int32_t* LA_RESTRICT b, int32_t* LA_RESTRICT out, size_t count) noexcept;
    };

    // ----------------------------- Fill -------------------------------------

    // None: T, Width
    template<typename T, size_t Width> struct fill_t {
        static inline void apply(T* out, T value, size_t count) noexcept {
            for (size_t i = 0; i < count; ++i) out[i] = value;
        }
    };

    // SSE2: int32_t, 4
    template<> struct fill_t<int32_t, 4> {
        static void apply(int32_t* out, int32_t value, size_t count) noexcept;
    };

    // SSE: float, 4
    template<> struct fill_t<float, 4> {
        static void apply(float* out, float value, size_t count) noexcept;
    };

    // AVX: float, 8
    template<> struct fill_t<float, 8> {
        static void apply(float* out, float value, size_t count) noexcept;
    };

    // AVX2: int, 8
    template<>
    struct fill_t<int32_t, 8> {
        static void apply(int32_t* out, int32_t value, size_t count) noexcept;
    };
    }; // struct Simd

struct
Out {
        static constexpr size_t BUFFER_SIZE = 256 - sizeof(size_t);

    private:
#ifdef LA_CONSOLE
        char m_buffer[BUFFER_SIZE]{};
        size_t m_length = 0;
#endif
    public:

        explicit inline Out() noexcept = default;

        // -------------------------- Procedures ----------------------------------

        // Flush the internal buffer to stdout/stderr/console
        void 
flush() noexcept {
#ifdef LA_CONSOLE
            m_buffer[m_length] = '\0';
            print(m_buffer, m_length);
            m_length = 0;
#endif
        } // flush

        // Append a single character to buffer
        inline void
put(char c) noexcept {
#ifdef LA_CONSOLE
            if (m_length < BUFFER_SIZE - 1) {
                m_buffer[m_length++] = c;
            }
            else {
                // Flush and reset buffer if full
                flush();
                m_buffer[m_length++] = c;
            }
#endif
        } // put

        // ---------------------------- Helpers -----------------------------------

        void
write_str(const char* str, size_t count) {
#ifdef LA_CONSOLE
            while (count-- && m_length < BUFFER_SIZE - 1) {
                m_buffer[m_length++] = *str++;
            }
#endif
        } // write_str

        // Append a null-terminated string to buffer
        inline void
write_str(const char* str) noexcept {
#ifdef LA_CONSOLE
            while (*str && m_length < BUFFER_SIZE - 1)
                m_buffer[m_length++] = *str++;
#endif
        } // write_str

        // Convert unsigned int to string and write to buffer
        inline void
write_unsigned(uint64_t value) noexcept {
#ifdef LA_CONSOLE
            char temp[20];
            int index = 0;

            do {
                temp[index++] = '0' + (value % 10);
                value /= 10;
            } while (value);

            while (--index >= 0)
                put(temp[index]);
#endif
        } // write_unsigned
        inline void write_unsigned(uint8_t value) noexcept {
#ifdef LA_CONSOLE
            write_unsigned(static_cast<uint64_t>(value));
#endif
        }
        inline void write_unsigned(uint16_t value) noexcept {
#ifdef LA_CONSOLE
            write_unsigned(static_cast<uint64_t>(value));
#endif
        }
        inline void write_unsigned(uint32_t value) noexcept {
#ifdef LA_CONSOLE
            write_unsigned(static_cast<uint64_t>(value));
#endif
        }

// Signed ints
        // Convert signed int to string and write
        inline void write_signed(int64_t value) noexcept {
#ifdef LA_CONSOLE
            if (value < 0) {
                put('-');
                write_unsigned(static_cast<uint64_t>(-value));
            }
            else {
                write_unsigned(static_cast<uint64_t>(value));
            }
#endif
        } // write_signed
        inline void write_signed(int8_t value) noexcept {
#ifdef LA_CONSOLE
            write_signed(static_cast<int64_t>(value));
#endif
        }
        inline void write_signed(int16_t value) noexcept { 
#ifdef LA_CONSOLE
            write_signed(static_cast<int64_t>(value));
#endif
        }
        inline void write_signed(int32_t value) noexcept { 
#ifdef LA_CONSOLE
            write_signed(static_cast<int64_t>(value));
#endif
        }

        // Convert floating-point number to string with fixed precision
        inline void write_float(double value, int precision = 6) noexcept;

        // Helper for power of 10
static inline unsigned pow10(int n) noexcept {
            unsigned result = 1;
            while (n-- > 0) result *= 10;
            return result;
        } // pow10

        // ---------------------------- Operators ---------------------------------

// Byte strings
        inline Out& operator<<(const unsigned char* s) noexcept {
#ifdef LA_CONSOLE
            write_str(reinterpret_cast<const char*>(s));
#endif
            return *this;
        }

// C-style Strings
        inline Out& operator<<(const char* s) noexcept {
#ifdef LA_CONSOLE
            write_str(s);
#endif
            return *this;
        }

// Single char
        inline Out& operator<<(char c) noexcept {
#ifdef LA_CONSOLE
            put(c);
#endif
            return *this;
}

// Signed ints
        inline Out& operator<<(int8_t v) noexcept {
#ifdef LA_CONSOLE
            write_signed(v);
#endif
            return *this;
        }
        inline Out& operator<<(int16_t v) noexcept {
#ifdef LA_CONSOLE
            write_signed(v);
#endif
            return *this;
        }
        inline Out& operator<<(int32_t v) noexcept {
#ifdef LA_CONSOLE
            write_signed(v);
#endif
            return *this;
        }
        inline Out& operator<<(int64_t v) noexcept { 
#ifdef LA_CONSOLE
            write_signed(v);
#endif
            return *this;
        }

// Unsigned ints
        inline Out& operator<<(uint8_t v) noexcept { 
#ifdef LA_CONSOLE
            write_unsigned(v);
#endif
            return *this;
        }
        inline Out& operator<<(uint16_t v) noexcept { 
#ifdef LA_CONSOLE
            write_unsigned(v); return *this; 
#endif
            return *this;
        }
        inline Out& operator<<(uint32_t v) noexcept { 
#ifdef LA_CONSOLE
            write_unsigned(v); return *this; 
#endif
            return *this;
        }
        inline Out& operator<<(uint64_t v) noexcept { 
#ifdef LA_CONSOLE
            write_unsigned(v); return *this; 
#endif
            return *this;
        }

// float/double
        inline Out& operator<<(double v) noexcept {
#ifdef LA_CONSOLE
            write_float(v); return *this; 
#endif
            return *this;
        }

// bool
        inline Out& operator<<(bool b) noexcept {
#ifdef LA_CONSOLE
            write_str(b ? "true" : "false"); return *this;
#endif
            return *this;
        }

        // Support manipulators like endl
        inline Out& operator<<(Out& (*manip)(Out&)) noexcept { return manip(*this); }
    }; // struct Out


    // ---------------------------- Write Float -------------------------------

    // Convert floating-point number to string with fixed precision
    inline void Out::
write_float(double value, int precision) noexcept {
#ifdef LA_CONSOLE
        if (value < 0) {
            put('-');
            value = -value;
        }

        const int int_part = static_cast<int>(value);
        double frac_part = value - int_part;

        write_unsigned(static_cast<uint64_t>(int_part));
        put('.');

        frac_part *= pow10(precision);
        write_unsigned(static_cast<uint64_t>(frac_part + 0.5));
#endif
    } // write_float

    // ---------------------------- Write `endl` ------------------------------

    // Custom manipulator: endl (flushes after newline)
    inline Out&
endl(Out& o) noexcept {
#ifdef LA_CONSOLE
        o.put('\n');
        o.flush();
#endif
        return o;
    } // endl

// ------------------------------- Error Handling -----------------------------

enum class Error : int {
    None = 0,

    RendererApiNotSet,
    CreateWindow,
    CreateFramebuffer,
    CreateOpenglContext,
}; // enum class Error

enum class AboutError : int {
    None = 0,
    Unknown,
    RendererApiNotSet,

    // Main Window
    Win32_WindowClass,
    Win32_Window,
    Win32_WindowDc,
    // Opengl Window
    Win32_ChoosePixelFormatARB,
    Win32_SetPixelFormat,
    Win32_CreateContextAttribsARB,
    Win32_CreateModernContext,
    Win32_GetCurrentContext,
    Win32_GetCurrentDC,
    // Dummy Window
    Win32_Dummy_WindowClass,
    Win32_Dummy_Window,
    Win32_Dummy_WindowDc,
    Win32_Dummy_ChoosePixelFormat,
    Win32_Dummy_SetPixelFormat,
    Win32_Dummy_CreateContext,
    // Missing functions
    Win32_Missing_ChoosePixelFormatARB,
    Win32_Missing_CreateContextAttribsARB,
    Win32_Missing_SwapIntervalEXT,
    // Framebuffer (Software renderer)
    Win32_CreateCompatibleDc,
    Win32_CreateDibSection,
    Win32_SelectObject,

    // MSVC issues in freestanding mode
#ifdef LA_NOSTD
    Win32_Freestanding_DeleteOperatorCalled,
#endif
}; // enum class AboutError

LA_NO_DISCARD LA_CONSTEXPR
static const char* what(AboutError error) noexcept {
    using AE = AboutError;
    switch (error)
    {
    case AE::None:               return "No error";
    case AE::RendererApiNotSet:    return "Renderer API not set";

    case AE::Win32_WindowClass:  return "Couldn't create window class";
    case AE::Win32_Window:       return "Couldn't create window object";
    case AE::Win32_WindowDc:     return "Couldn't create window DC";
    case AE::Win32_ChoosePixelFormatARB:    return "Couldn't choose pixel format (ARB)";
    case AE::Win32_SetPixelFormat:          return "Couldn't set pixel format";
    case AE::Win32_CreateContextAttribsARB: return "Couldn't create context attribs (ARB)";
    case AE::Win32_CreateModernContext:     return "Couldn't create modern context";
    case AE::Win32_GetCurrentContext:       return "Couldn't get current context";
    case AE::Win32_GetCurrentDC:            return "Couldn't get current DC";
    case AE::Win32_Dummy_WindowClass:       return "Couldn't create dummy window class";
    case AE::Win32_Dummy_Window:            return "Couldn't create dummy window object";
    case AE::Win32_Dummy_WindowDc:          return "Couldn't create dummy window DC";
    case AE::Win32_Dummy_ChoosePixelFormat: return "Couldn't choose dummy pixel format";
    case AE::Win32_Dummy_SetPixelFormat:    return "Couldn't set dummy pixel format";
    case AE::Win32_Dummy_CreateContext:     return "Couldn't create dummy context";
    case AE::Win32_Missing_ChoosePixelFormatARB:    return "Missing wglChoosePixelFormatARB";
    case AE::Win32_Missing_CreateContextAttribsARB: return "Missing wgCreateContextAttribsARB";
    case AE::Win32_Missing_SwapIntervalEXT:         return "Missing wglSwapIntervalEXT";
    case AE::Win32_CreateCompatibleDc: return "Couldn't create compatibale DC";
    case AE::Win32_CreateDibSection:   return "Couldn't create DIB section";
    case AE::Win32_SelectObject:       return "Couldn't select object";
#ifdef LA_NOSTD
    case AE::Win32_Freestanding_DeleteOperatorCalled: return "Operator delete called in freestanding mode";
#endif
    default: return "unknown error";
    }
} // what

// ------------------------------- Key ----------------------------------------

enum class Key {
    __NONE__,
    // --------------------------- FUNCTIONAL ---------------------------
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    // --------------------------- MODIFIERS ---------------------------
    Shift,
    Control,
    Alt,
    Super,
    // --------------------------- TTY ---------------------------
    Escape,
    Insert,
    Delete,
    Backspace,
    Tab,
    Return,
    ScrollLock,
    NumLock,
    CapsLock,
    // --------------------------- MOTION ---------------------------
    Home,
    End,
    PageUp,
    PageDown,
    Left,
    Up,
    Right,
    Down,
    // --------------------------- MOUSE ---------------------------
    MouseLeft,
    MouseRight,
    MouseMiddle,
    MouseX1,
    MouseX2,
    
    // --------------------------- BASIC ---------------------------
    Space,
    _0, _1, _2, _3, _4, _5, _6, _7, _8, _9,
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // --------------------------- ASCII ---------------------------
    Grave,       // `
    Hyphen,      // -
    Equal,       // =
    BracketLeft, // [
    BracketRight,// ]
    Comma,       // ,
    Period,      // .
    Slash,
    Backslash,
    Semicolon,   // ;
    Apostrophe,  // '  

    __LAST__
}; // enum class Key

LA_NO_DISCARD bool is_key_pressed(la::Key) noexcept;
LA_NO_DISCARD const char* get_key_name(la::Key) noexcept;

// --------------------------- RendererApi ---------------------------

enum class RendererApi {
    None = 0,
    Software,
    Opengl,
};

// --------------------------- Event Interface ---------------------------

/// Interface for window event callbacks
struct IWindowEvents {
    virtual void on_scroll_vertical(float delta) noexcept {}
    virtual void on_resize(int width, int height) noexcept {}
    virtual void on_mouse_move(int x, int y) noexcept {}
    virtual void on_key_down(Key) noexcept {}
    virtual void on_key_up(Key) noexcept {}
    virtual void on_focus_change(bool gained) noexcept {}
    virtual void on_render_software() noexcept {}
    virtual void on_render_opengl() noexcept {}
    virtual void on_error(Error e, AboutError about) noexcept {
        // default: panic on error
        panic_process(what(about), static_cast<int>(about));
    }
};

// --------------------------- Native Window ---------------------------
struct native::Window {
#if defined(__linux__)

#elif defined(_WIN32)
    void* hdc{ nullptr };
    void* hwnd{ nullptr };
#endif

    explicit inline Window() noexcept = default;
    ~Window() noexcept;

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;
}; // struct Window

// --------------------------- Native Framebuffer ---------------------------
struct native::Framebuffer {
#if defined(__linux__)

#elif defined(_WIN32)
    void* hdc{ nullptr };
    void* bmp{ nullptr };
    void* pixels{ nullptr };
#endif // Platform

    explicit inline Framebuffer() noexcept = default;
    ~Framebuffer() noexcept;

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    Framebuffer(Framebuffer&&) = delete;
    Framebuffer& operator=(Framebuffer&&) = delete;

    LA_NO_DISCARD la::AboutError recreate(
        const la::Window& win, int width, int height) noexcept;

    void clear(uint32_t color, int width, int height) const noexcept;
    void draw_pixel(int x, int y, int width, int height, uint32_t color) const noexcept;
}; // struct Framebuffer

// --------------------------- Opengl Context ---------------------------
struct native::OpenglContext {
#if defined(__linux__)

#elif defined(_WIN32)
    void* hglrc{ nullptr };
#endif

    explicit inline OpenglContext() noexcept = default;
    ~OpenglContext() noexcept;

    OpenglContext(const OpenglContext&) = delete;
    OpenglContext& operator=(const OpenglContext&) = delete;
    OpenglContext(OpenglContext&&) = delete;
    OpenglContext& operator=(OpenglContext&&) = delete;
}; // struct OpenglContext

// --------------------------- Window ---------------------------
struct Window {
    using Native = ::la::native::Window;
    using Framebuffer = ::la::native::Framebuffer;
    using GlContext = ::la::native::OpenglContext;
    
    explicit Window(IWindowEvents& handler, int w = 640, int h = 480,
                    RendererApi api = RendererApi::Software, 
                    bool shown = false,
                    bool bordless = false) noexcept;
    inline ~Window() noexcept;
    
    // Non-copyable, non-movable
    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;
    Window(Window &&) = delete;
    Window &operator=(Window &&) = delete;

    // Procedures

    inline void render() noexcept;
    void show(bool) const noexcept;
    void quit() const noexcept;

    void swap_buffer_software() const noexcept;
    void swap_buffer_opengl() const noexcept;

    // Getters: render

    LA_NO_DISCARD RendererApi renderer_api() const noexcept { return m_renderer_api; }    
    LA_NO_DISCARD       Framebuffer& fb()       noexcept { return m_fb; }
    LA_NO_DISCARD const Framebuffer& fb() const noexcept { return m_fb; }
    LA_NO_DISCARD       GlContext& gl()       noexcept { return m_gl; }
    LA_NO_DISCARD const GlContext& gl() const noexcept { return m_gl; }

    // Getters: window properties

    LA_NO_DISCARD bool poll_events() const noexcept;
    LA_NO_DISCARD IWindowEvents& handler() const noexcept { return m_handler; }
    LA_NO_DISCARD const Native& native() const noexcept { return m_native; }
    
    LA_NO_DISCARD int width() const noexcept { return m_width; }
    LA_NO_DISCARD int height() const noexcept { return m_height; }

    // Setters

    void set_handler(IWindowEvents& handler) noexcept { m_handler = handler; }
    void set_renderer(RendererApi api) noexcept;
    void set_title(const char *title) const noexcept;
    void set_fullscreen(bool) const noexcept;
    void set_cursor_visible(bool) const noexcept;



private:
    IWindowEvents& m_handler;
    Native m_native;

    RendererApi m_renderer_api{ RendererApi::None };
    union {
        Framebuffer m_fb;
        GlContext m_gl;
    };
    
    int m_width;
    int m_height;

    friend void native::on_geometry_change(::la::Window&, int w, int h) noexcept;
}; // struct Window

inline Window::~Window() noexcept {
    switch (renderer_api()) {
    case RendererApi::Software: fb().~Framebuffer(); break;
    case RendererApi::Opengl:   gl().~GlContext();   break;
    default: LA_ASSERT(0, "Do NOT set RendererApi to `None` before window destructor called");
    }
};

inline void Window::render() noexcept {
    switch (renderer_api())
    {
    case RendererApi::Software:
        m_handler.on_render_software();
        native::render_software(*this);
        break;
    case RendererApi::Opengl:
        m_handler.on_render_opengl();
        native::render_opengl(*this);
        break;
    } // switch
} // render


extern Simd::AddFloat  add_float;
extern Simd::AddInt32  add_int32;
extern Simd::FillFloat fill_float;
extern Simd::FillInt32 fill_int32;

struct GlobalInitializer {
    GlobalInitializer(bool sse, bool sse2, bool avx, bool avx2) noexcept {
        add_float = Simd::choose_add_float(sse, avx);
        add_int32 = Simd::choose_add_int32(sse2, avx2);
        fill_float = Simd::choose_fill_float(sse, avx);
        fill_int32 = Simd::choose_fill_int32(sse2, avx2);
    }

    static void init() noexcept;
}; // struct GlobalInitializer

} // namespace la

#endif // __LA_HEADER_GUARD