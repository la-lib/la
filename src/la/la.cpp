#ifdef _WIN32

// Uncomment macro below, to see destuctor messages in command prompt.
// Also, define `LA_CONSOLE` if not defined.
// #define LA_DEBUG_DESTRUCTORS

// I FUCKING HATE MICROSOFT PRODUCTS
#if defined(_MSC_VER) && defined(LA_NOSTD)
extern "C" void* __cdecl memset(void* dest, int ch, size_t count) {
    unsigned char* p = static_cast<unsigned char*>(dest);
    while (count--) *p++ = static_cast<unsigned char>(ch);
    return dest;
}
#endif


#include "gl.hpp"
#include "la.hpp"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef CreateWindow

#include <Psapi.h> // psapi.lib
#include <gl/GL.h>

// ------------------------------ Intrin --------------------------------------
#if defined(_MSC_VER)
#   include <intrin.h>
#else
static inline void cpuid(int info[4], int eax, int ecx) {
    __asm__ __volatile__(
        "cpuid"
        : "=a"(info[0]), "=b"(info[1]), "=c"(info[2]), "=d"(info[3])
        : "a"(eax), "c"(ecx)
    );
}
#endif // Intrin


// ------------------------------ Global Variables ----------------------------
LA_CONSTEXPR_VAR wchar_t WINDOW_CLASSNAME[] { L"_" };
LA_CONSTEXPR_VAR LPCSTR DUMMY_CLASS_NAME{ "d" };
LA_CONSTEXPR_VAR unsigned DRAW_COLOR_MODE = 4; // ARGB

LA_CONSTEXPR_VAR unsigned char KEY_ARRAY_SIZE = static_cast<unsigned char>(::la::Key::__LAST__);
unsigned char key_array[KEY_ARRAY_SIZE]{ 0 };

namespace la {
    Simd::AddFloat  add_float = nullptr;
    Simd::AddInt32  add_int32 = nullptr;
    Simd::FillFloat fill_float = nullptr;
    Simd::FillInt32 fill_int32 = nullptr;

    void GlobalInitializer::init() noexcept {
        volatile GlobalInitializer g {
            Simd::has_sse(),
            Simd::has_sse2(),
            Simd::has_avx(),
            Simd::has_avx2()
        };
    }

    
// --------------------------------- SIMD -------------------------------------

    namespace detail {
        template<typename T>
        inline size_t align_prologue(T*& out, size_t& count, size_t align_bytes, const T value) noexcept {
            // Align "out" upward to align_bytes by writing a small scalar prologue.
            const uintptr_t addr = reinterpret_cast<uintptr_t>(out);
            const size_t mis = (size_t)((-(intptr_t)addr) & (align_bytes - 1));
            const size_t elems = count < mis / sizeof(T) ? 
                                 count : mis / sizeof(T);
            for (size_t k = 0; k < elems; ++k) out[k] = value;
            out += elems;
            count -= elems;
            return elems; // number of elements written in prologue
        } // align_prologue

        template<typename T>
        inline void scalar_tail_unrolled(T* out, const T value, size_t rem) noexcept {
            switch (rem) {
            case 15: out[14] = value; LA_FALLTHROUGH;
            case 14: out[13] = value; LA_FALLTHROUGH;
            case 13: out[12] = value; LA_FALLTHROUGH;
            case 12: out[11] = value; LA_FALLTHROUGH;
            case 11: out[10] = value; LA_FALLTHROUGH;
            case 10: out[9] = value; LA_FALLTHROUGH;
            case 9:  out[8] = value; LA_FALLTHROUGH;
            case 8:  out[7] = value; LA_FALLTHROUGH;
            case 7:  out[6] = value; LA_FALLTHROUGH;
            case 6:  out[5] = value; LA_FALLTHROUGH;
            case 5:  out[4] = value; LA_FALLTHROUGH;
            case 4:  out[3] = value; LA_FALLTHROUGH;
            case 3:  out[2] = value; LA_FALLTHROUGH;
            case 2:  out[1] = value; LA_FALLTHROUGH;
            case 1:  out[0] = value; LA_FALLTHROUGH;
            default: break;
            }
        } // scalar_tail_unrolled
    } // namespace detail

    // ----------------------------- Add --------------------------------------

    // Specialization for float + SSE
    void Simd::add_t<float, 4>::apply(const float* LA_RESTRICT a,
                                     const float* LA_RESTRICT b, 
                             float* LA_RESTRICT out, size_t count) noexcept {
        using reg = __m128;
        for (size_t i = 0; i < count; i += 4) {
            reg av = _mm_loadu_ps(a + i);
            reg bv = _mm_loadu_ps(b + i);
            reg r = _mm_add_ps(av, bv);
            _mm_storeu_ps(out + i, r);
        }
    } // float + SSE

    // Specialization for float + AVX
    void Simd::add_t<float, 8>::apply(const float* LA_RESTRICT a,
                                      const float* LA_RESTRICT b,
                             float* LA_RESTRICT out, size_t count) noexcept {
        using reg = __m256;
        for (size_t i = 0; i < count; i += 8) {
            reg av = _mm256_loadu_ps(a + i);
            reg bv = _mm256_loadu_ps(b + i);
            reg r = _mm256_add_ps(av, bv);
            _mm256_storeu_ps(out + i, r);
        }
    } // float + AVX

    // Specialization for int32 + AVX2
    void Simd::add_t<int32_t, 8>::apply(const int32_t* LA_RESTRICT a,
                                        const int32_t* LA_RESTRICT b,
                           int32_t* LA_RESTRICT out, size_t count) noexcept {
        using reg = __m256i;
        for (size_t i = 0; i < count; i += 8) {
            reg av = _mm256_loadu_si256((__m256i*)(a + i));
            reg bv = _mm256_loadu_si256((__m256i*)(b + i));
            reg r = _mm256_add_epi32(av, bv);
            _mm256_storeu_si256((__m256i*)(out + i), r);
        }
    } // int32 + AVX2

    // ----------------------------- Fill -------------------------------------

    // SSE2: int32_t, 4
    void Simd::fill_t<int32_t, 4>::apply(int32_t* out, int32_t value, size_t count) noexcept {
        if (count == 0) return;

        using reg = __m128i;
        reg v = _mm_set1_epi32(value);

        // Try to use aligned stores by aligning the pointer with a small scalar prologue.
        size_t wrote = detail::align_prologue(out, count, /*align_bytes=*/16, value);
        (void)wrote;

        // Unrolled SIMD loop: 4 * 4 = 16 ints per iteration
        const size_t V = 4;      // elements per SSE register for int32
        const size_t U = 4;      // unroll factor
        const size_t BLOCK = V * U; // 16

        size_t i = 0;
        // Prefetch distance (in elements). ~512 bytes ahead as a starting point.
        const size_t PF = 512 / sizeof(int32_t);

        // Use aligned stores now that out is 16B-aligned
        for (; i + BLOCK <= count; i += BLOCK) {
            _mm_prefetch((const char*)(out + i + PF), _MM_HINT_T0);
            _mm_store_si128((__m128i*)(out + i + 0 * V), v);
            _mm_store_si128((__m128i*)(out + i + 1 * V), v);
            _mm_store_si128((__m128i*)(out + i + 2 * V), v);
            _mm_store_si128((__m128i*)(out + i + 3 * V), v);
        }

        // Vector cleanup (still aligned stores), then scalar tail-unrolled
        for (; i + V <= count; i += V)
            _mm_store_si128((__m128i*)(out + i), v);

        detail::scalar_tail_unrolled(out + i, value, count - i);
    } // SSE2: int32_t, 4

    // SSE: float, 4
    void Simd::fill_t<float, 4>::apply(float* out, float value, size_t count) noexcept {
        if (count == 0) return;

        using reg = __m128;
        reg v = _mm_set1_ps(value);

        size_t wrote = detail::align_prologue(out, count, /*align_bytes=*/16, value);
        (void)wrote;

        const size_t V = 4;      // floats per SSE reg
        const size_t U = 4;      // unroll
        const size_t BLOCK = V * U; // 16
        size_t i = 0;
        const size_t PF = 512 / sizeof(float);

        for (; i + BLOCK <= count; i += BLOCK) {
            _mm_prefetch((const char*)(out + i + PF), _MM_HINT_T0);
            _mm_store_ps(out + i + 0 * V, v);
            _mm_store_ps(out + i + 1 * V, v);
            _mm_store_ps(out + i + 2 * V, v);
            _mm_store_ps(out + i + 3 * V, v);
        }
        for (; i + V <= count; i += V) {
            _mm_store_ps(out + i, v);
        }
        detail::scalar_tail_unrolled(out + i, value, count - i);
    }; // SSE: float, 4

    // AVX: float, 8
    void Simd::fill_t<float, 8>::apply(float* out, float value, size_t count) noexcept {
        if (count == 0) return;

        using reg = __m256;
        reg v = _mm256_set1_ps(value);

        // Align to 32 bytes for AVX aligned stores
        size_t wrote = detail::align_prologue(out, count, /*align_bytes=*/32, value);
        (void)wrote;

        const size_t V = 8;      // floats per AVX reg
        const size_t U = 4;      // unroll
        const size_t BLOCK = V * U; // 32
        size_t i = 0;
        const size_t PF = 512 / sizeof(float);

        for (; i + BLOCK <= count; i += BLOCK) {
            _mm_prefetch((const char*)(out + i + PF), _MM_HINT_T0);
            _mm256_store_ps(out + i + 0 * V, v);
            _mm256_store_ps(out + i + 1 * V, v);
            _mm256_store_ps(out + i + 2 * V, v);
            _mm256_store_ps(out + i + 3 * V, v);
        }
        for (; i + V <= count; i += V) {
            _mm256_store_ps(out + i, v);
        }
        detail::scalar_tail_unrolled(out + i, value, count - i);
    } // AVX: float, 8

    // AVX2: int, 8
    void Simd::fill_t<int32_t, 8>::apply(int32_t* out, int32_t value, size_t count) noexcept {
        if (count == 0) return;

        using reg = __m256i;
        reg v = _mm256_set1_epi32(value);

        size_t wrote = detail::align_prologue(out, count, /*align_bytes=*/32, value);
        (void)wrote;

        const size_t V = 8;      // ints per AVX2 reg
        const size_t U = 4;      // unroll
        const size_t BLOCK = V * U; // 32
        size_t i = 0;
        const size_t PF = 512 / sizeof(int32_t);

        for (; i + BLOCK <= count; i += BLOCK) {
            _mm_prefetch((const char*)(out + i + PF), _MM_HINT_T0);
            _mm256_store_si256((__m256i*)(out + i + 0 * V), v);
            _mm256_store_si256((__m256i*)(out + i + 1 * V), v);
            _mm256_store_si256((__m256i*)(out + i + 2 * V), v);
            _mm256_store_si256((__m256i*)(out + i + 3 * V), v);
        }
        for (; i + V <= count; i += V) {
            _mm256_store_si256((__m256i*)(out + i), v);
        }
        detail::scalar_tail_unrolled(out + i, value, count - i);
    } // AVX2: int, 8
} // namespace la

// ---------------------------- Native Declarations ---------------------------

    LA_NO_DISCARD ::la::AboutError
create_gl_context(HDC main_dc) noexcept;

    static LRESULT CALLBACK
win_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) noexcept;

// ------------------------ Hacks for freestanding mode -----------------------

#if defined(LA_NOSTD) && defined(_MSC_VER) // Hacks for MSVC
extern "C" int _fltused = 0;
void __cdecl operator delete(void* ptr, unsigned int size) {
    ::la::panic_process(::la::what(::la::AboutError::Win32_Freestanding_DeleteOperatorCalled), -1);
}

#if defined(_M_X64) || defined(_M_ARM64) // Exclusive for 64-bit targets
void __cdecl operator delete(void* ptr, size_t size) {
    ::la::panic_process(::la::what(::la::AboutError::Win32_Freestanding_DeleteOperatorCalled), -1);
}
#endif // 64-bit
#endif // LA_NOSTD && _MSC_VER

// ---------------------------- OpenGL Loader ---------------------------------

namespace la {
namespace gl {
// Helper function
LA_NO_DISCARD
static void* opengl_loader(const char* name) noexcept {
    void* p = (void*)wglGetProcAddress(name);
    if (!p || p == (void*)0x1 || p == (void*)0x2 || p == (void*)0x3 ||
        p == (void*)-1) {
        static HMODULE module = LoadLibraryA("opengl32.dll");
        p = (void*)GetProcAddress(module, name);
    }
    return p;
} // opengl_loader

LoadProc loader = opengl_loader;
} // namespace gl
} // namespace la

// Import WGL functions
extern "C" {
    __declspec(dllimport) HGLRC WINAPI wglCreateContext(HDC hdc);
    __declspec(dllimport) BOOL  WINAPI wglMakeCurrent(HDC hdc, HGLRC hglrc);
    __declspec(dllimport) BOOL  WINAPI wglDeleteContext(HGLRC hglrc);
    __declspec(dllimport) PROC  WINAPI wglGetProcAddress(LPCSTR lpszProc);
    __declspec(dllimport) HDC   WINAPI wglGetCurrentDC(void);
    __declspec(dllimport) HGLRC WINAPI wglGetCurrentContext(void);
}

namespace wingl {
// minimal ARB
LA_CONSTEXPR_VAR struct /* arb */ {
    LA_CONSTEXPR_VAR static int DRAW_TO_WINDOW = 0x2001;
    LA_CONSTEXPR_VAR static int SUPPORT_OPENGL = 0x2010;
    LA_CONSTEXPR_VAR static int DOUBLE_BUFFER = 0x2011;
    LA_CONSTEXPR_VAR static int PIXEL_TYPE = 0x2013;
    LA_CONSTEXPR_VAR static int TYPE_RGBA = 0x202B;
    LA_CONSTEXPR_VAR static int COLOR_BITS = 0x2014;
    LA_CONSTEXPR_VAR static int DEPTH_BITS = 0x2022;
    LA_CONSTEXPR_VAR static int STENCIL_BITS = 0x2023;

    LA_CONSTEXPR_VAR static int CONTEXT_MAJOR_VERSION = 0x2091;
    LA_CONSTEXPR_VAR static int CONTEXT_MINOR_VERSION = 0x2092;
    LA_CONSTEXPR_VAR static int CONTEXT_PROFILE_MASK = 0x9126;
    LA_CONSTEXPR_VAR static int CONTEXT_CORE_PROFILE_BIT = 0x00000001;
    
} arb;

// Create Context
typedef HGLRC(WINAPI* PFNWGLCREATECONTEXTATTRIBSARBPROC)(
                                                       HDC        hdc, 
                                                       HGLRC      shareContext, 
                                                       const int* attribList);
// Choose pixel format
typedef BOOL(WINAPI* PFNWGLCHOOSEPIXELFORMATARBPROC)(
                                                    HDC          hdc, 
                                                    const int*   piAttribIList, 
                                                    const FLOAT* pfAttribFList, 
                                                    UINT         nMaxFormats, 
                                                    int*         piFormats, 
                                                    UINT*        nNumFormats);
// Swap interval (V-Sync)
typedef BOOL(WINAPI* PFNWGLSWAPINTERVALEXTPROC)(int interval);

LA_CONSTEXPR_VAR int PIXEL_ATTRS [] {
                        arb.DRAW_TO_WINDOW, GL_TRUE,
                        arb.SUPPORT_OPENGL, GL_TRUE,
                        arb.DOUBLE_BUFFER,  GL_TRUE,
                        arb.PIXEL_TYPE,     arb.TYPE_RGBA,
                        arb.COLOR_BITS,     32,
                        arb.DEPTH_BITS,     24,
                        arb.STENCIL_BITS,   8,
                        0 /* The end of the array */ };

LA_CONSTEXPR_VAR int CONTEXT_ATTRS [] {
                        arb.CONTEXT_MAJOR_VERSION, 3,
                        arb.CONTEXT_MINOR_VERSION, 3,
                        arb.CONTEXT_PROFILE_MASK,  arb.CONTEXT_CORE_PROFILE_BIT,
                        0 /* The end of the array */ };

LA_CONSTEXPR_VAR PIXELFORMATDESCRIPTOR PF_DESCRIPTOR{
                        /* nSize    */ sizeof(PF_DESCRIPTOR),
                        /* nVersion */ 1,
                        /* dwFlags  */ PFD_DRAW_TO_WINDOW
                                     | PFD_SUPPORT_OPENGL
                                     | PFD_DOUBLEBUFFER,
                        /* iPixelType   */ PFD_TYPE_RGBA,
                        /* cColorBits   */ 32,
                        /* cDepthBits   */ 24,
                        /* cStencilBits */ 8,
                        /* iLayerType   */ PFD_MAIN_PLANE };
} // namespace wingl


// --------------------------- Dummy Window ---------------------------

struct DummyWindow {
private:
    HINSTANCE m_histance;
    HWND m_hwnd;
    HDC m_hdc;
    HGLRC m_ctx;
public:

    // Constructor
    inline DummyWindow() noexcept :
        m_histance { GetModuleHandleA(nullptr) },
        m_hwnd{ nullptr }, m_hdc{ nullptr }, m_ctx{ nullptr } {

        WNDCLASSA win_class{ /*         style */ CS_OWNDC,
                             /*   lpfnWndProc */ DefWindowProcA,
                             /*    cbClsExtra */ 0,
                             /*    cbWndExtra */ 0,
                             /*     hInstance */ m_histance,
                             /*         hIcon */ nullptr,
                             /*       hCursor */ nullptr,
                             /* hbrBackground */ nullptr,
                             /*  lpszMenuName */ nullptr,
                             /* lpszClassName */ DUMMY_CLASS_NAME };
        if (!RegisterClassA(&win_class))
            return;

        m_hwnd = CreateWindowExA( /*    dwExStyle */ 0,
                                  /*  lpClassName */ DUMMY_CLASS_NAME,
                                  /* lpWindowName */ " ",
                                  /*      dwStyle */ WS_OVERLAPPEDWINDOW,
                                  /*            X */ CW_USEDEFAULT,
                                  /*            Y */ CW_USEDEFAULT,
                                  /*       nWidth */ 1,
                                  /*      nHeight */ 1,
                                  /*   hWndParent */ nullptr,
                                  /*        hMenu */ nullptr,
                                  /*    hInstance */ m_histance,
                                  /*      lpParam */ nullptr);
        if (!m_hwnd)
            return;

        m_hdc = GetDC(m_hwnd);
    } // constructor 

    // Getters

    LA_NO_DISCARD inline HINSTANCE histance() const noexcept { return m_histance; }
    LA_NO_DISCARD inline HWND hwnd() const noexcept { return m_hwnd; }
    LA_NO_DISCARD inline HDC hdc() const noexcept { return m_hdc; }
    LA_NO_DISCARD inline HGLRC ctx() const noexcept { return m_ctx; }

    // Setters

    LA_NO_DISCARD inline void set_ctx(HGLRC ctx) noexcept { m_ctx = ctx; }
}; // struct DummyWindow

namespace la {
// ---------------------- OS-dependent Functions ------------------------

bool Simd::
has_sse() noexcept {
    int info[4] = {};
#if defined(_MSC_VER)
    __cpuid(info, 1);
#else
    cpuid(info, 1, 0);
#endif
    return (info[3] & (1 << 25)) != 0; // SSE
} // cpu_supports_sse

bool Simd::has_sse2() noexcept {
    int info[4] = {};
#if defined(_MSC_VER)
    __cpuid(info, 1);
#else
    cpuid(info, 1, 0);
#endif
    return (info[3] & (1 << 26)) != 0; // SSE2
} // cpu_supports_sse2

bool Simd::
has_avx() noexcept {
    int info[4] = {};
#if defined(_MSC_VER)
    __cpuid(info, 1);
#else
    cpuid(info, 1, 0);
#endif
    bool os_uses_xsave_xrstore = (info[2] & (1 << 27)) != 0;
    bool avx_supported = (info[2] & (1 << 28)) != 0;

    if (!os_uses_xsave_xrstore || !avx_supported)
        return false;

    // Check OS has enabled AVX state via XGETBV
    uint32_t eax = 0, edx = 0;
#if defined(_MSC_VER)
    uint64_t xcr = _xgetbv(0);
#else
    __asm__ volatile (".byte 0x0f, 0x01, 0xd0" : "=a"(eax), "=d"(edx) : "c"(0));
    uint64_t xcr = ((uint64_t)edx << 32) | eax;
#endif
    return (xcr & 0x6) == 0x6; // XMM and YMM state enabled
} // check_avx

bool Simd::
has_avx2() noexcept {
    int info[4] = {};

    // Check if AVX is supported
#if defined(_MSC_VER)
    __cpuid(info, 0);
#else
    cpuid(info, 0, 0);
#endif
    if (info[0] < 7) return false; // AVX2 in leaf 7

#if defined(_MSC_VER)
    __cpuidex(info, 7, 0);
#else
    cpuid(info, 7, 0);
#endif
    return (info[1] & (1 << 5)) != 0; // Bit 5 of EBX = AVX2
}

bool Simd::
has_avx512bw() noexcept {
    int info[4] = {};

#if defined(_MSC_VER)
    __cpuid(info, 0);
#else
    cpuid(info, 0, 0);
#endif
    if (info[0] < 7) return false;

#if defined(_MSC_VER)
    __cpuidex(info, 7, 0);
#else
    cpuid(info, 7, 0);
#endif
    return (info[1] & (1 << 30)) != 0; // Bit 30 of EBX = AVX-512BW
}

// --------------------------- Allocate/Free ---------------------------
void*
alloc(size_t size) noexcept {
    return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void
free(void* ptr, size_t /* size */) noexcept { VirtualFree(ptr, 0, MEM_RELEASE); }

// --------------------------- Processing ---------------------------

void
exit_process(int error_code) noexcept { ExitProcess(error_code); }

void
panic_process(const char* explain_msg, int error_code) noexcept {
    ::la::Out out;
    out << explain_msg << ::la::endl;
    MessageBoxA(nullptr, explain_msg, "Error", MB_OK | MB_ICONERROR);
    exit_process(error_code);
} // panic_process_explained

// --------------------------- Time ---------------------------

void
sleep(unsigned ms) noexcept { Sleep(ms); }

double get_monotonic_secs() noexcept {
    static LARGE_INTEGER frequency = [] {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        return freq;
    }();

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);

    return static_cast<double>(counter.QuadPart) / static_cast<double>(frequency.QuadPart);
}

// --------------------------- Misc Functions ---------------------------

bool
is_battery_in_use() noexcept {
    SYSTEM_POWER_STATUS status;
    if (GetSystemPowerStatus(&status)) {
        // ACLineStatus == 0 means battery power supply currently in use
        // BatteryFlag == 128 means "No system battery"
        if (status.ACLineStatus == 0 && status.BatteryFlag != 128)
            return true;
    }
    return false;
} // is_battery_in_use

void
reset_workset() noexcept {
#ifdef _PSAPI_H_ // Clear resources
    SetProcessWorkingSetSize(GetCurrentProcess(), -1, -1);
    EmptyWorkingSet(GetCurrentProcess());
#endif
} // reset_workset

// --------------------------- Input/Output ---------------------------

// TODO: I don't like this mess
void print(const char* msg, size_t msg_length) noexcept {
#ifdef LA_CONSOLE
    HANDLE h_console = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h_console == INVALID_HANDLE_VALUE)
        return; // No fallback

    // UTF-8 to UTF-16
    wchar_t wbuf[Out::BUFFER_SIZE];
    int wlen = MultiByteToWideChar(
        CP_UTF8,
        0,
        msg,
        (int)msg_length,
        wbuf,
        sizeof(wbuf) / sizeof(wchar_t)
    );

    if (wlen > 0) {
        DWORD written;
        WriteConsoleW(h_console, wbuf, (DWORD)wlen, &written, nullptr);
    }
#endif // LA_CONSOLE
} // print

// --------------------------- Native Window ---------------------------

native::Window::
~Window() noexcept {
    if (hdc)  ReleaseDC(reinterpret_cast<HWND>(hwnd), 
                        reinterpret_cast<HDC>(hdc));
    if (hwnd) DestroyWindow(reinterpret_cast<HWND>(hwnd));

#ifdef LA_DEBUG_DESTRUCTORS
    out << "~Window" << endl;
#endif
}

// --------------------------- Native Framebuffer ---------------------------
void native::Framebuffer::
clear(uint32_t color, int win_width, int win_height) const noexcept {
    if (!pixels) return;

    const size_t pixel_count = win_width * win_height;
    int32_t* out = reinterpret_cast<int32_t*>(pixels);
    int32_t fill = static_cast<int32_t>(color);

    ::la::fill_int32(out, fill, pixel_count); // Using the best found SIMD
} // clear

native::Framebuffer::
~Framebuffer() noexcept {
    if (bmp) DeleteObject(reinterpret_cast<HBITMAP>(bmp));
    if (hdc) DeleteDC(reinterpret_cast<HDC>(hdc));
#ifdef LA_DEBUG_DESTRUCTORS
    out << "~Framebuffer" << endl;
#endif
} // ~Framebuffer

LA_NO_DISCARD ::la::AboutError native::Framebuffer::
recreate(
    const ::la::Window& win, int width_, int height_) noexcept {

    // Free old resources
    if (bmp) {
        DeleteObject(reinterpret_cast<HBITMAP>(bmp));
        bmp = nullptr;
    }
    if (hdc) {
        DeleteDC(reinterpret_cast<HDC>(hdc));
        hdc = nullptr;
    }

    // Reject invalid dimensions
    if (width_ <= 0 || height_ <= 0)
        return ::la::AboutError::None; // Exit without recreating

    // Create compatible DC
    HDC device = GetDC(reinterpret_cast<HWND>(win.native().hwnd));
    if (!device)
        return ::la::AboutError::Win32_GetCurrentDC;

    hdc = CreateCompatibleDC(device);
    ReleaseDC(reinterpret_cast<HWND>(win.native().hwnd), device);

    if (!hdc)
        return ::la::AboutError::Win32_CreateCompatibleDc;

    // Prepare bitmap info (top-down DIB)
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width_;
    bmi.bmiHeader.biHeight = -height_; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* ppv_bits{ nullptr };
    bmp = CreateDIBSection(reinterpret_cast<HDC>(hdc),
                           &bmi, DIB_RGB_COLORS, &ppv_bits, nullptr, 0);
    // Check `bitmap` && `received pixel buffer`
    if (!bmp || !ppv_bits)
        return ::la::AboutError::Win32_CreateDibSection;

    // Check selected object
    if (!SelectObject(reinterpret_cast<HDC>(hdc), 
                      reinterpret_cast<HBITMAP>(bmp)))
        return ::la::AboutError::Win32_SelectObject;

    pixels = ppv_bits;
    return ::la::AboutError::None;
} // recreate

void native::Framebuffer::
draw_pixel(int x, int y, int width, int height, uint32_t color) const noexcept {
    // Defensive: don't draw out of bounds
    if (x < 0 || x >= width || y < 0 || y >= height || !pixels)
        return;

    // Each pixel is 4 bytes (BGRA32)
    const int pitch = width * 4;
    uint8_t* dst = static_cast<uint8_t*>(pixels);
    uint32_t* pixel_ptr = reinterpret_cast<uint32_t*>(dst + y * pitch + x * 4);

    *pixel_ptr = color;
}

// ------------------------- Native Opengl Context ---------------------------

native::OpenglContext::
~OpenglContext() noexcept {
    HGLRC ctx = reinterpret_cast<HGLRC>(hglrc);

    // Unbind context from any HDC (just in case it's current)
    if (wglGetCurrentContext() == ctx)
        wglMakeCurrent(nullptr, nullptr);

    // Delete OpenGL rendering context
    if (ctx)
        wglDeleteContext(ctx);

#ifdef LA_DEBUG_DESTRUCTORS
    out << "~OpenglContext" << endl;
#endif
}

// --------------------------- Window Specialized ---------------------------

void native::
render_software(const ::la::Window& win) noexcept {
    HWND hwnd = reinterpret_cast<HWND>(win.native().hwnd);
    PAINTSTRUCT ps;
    BeginPaint(hwnd, &ps);
    BitBlt(/* HDC hdc   */ ps.hdc,
        /*    int x  */ 0,
        /*    int y  */ 0,
        /*    int cx */ win.width(),
        /*    int cy */ win.height(),
        /* HDC src   */ reinterpret_cast<HDC>(win.fb().hdc),
        /*    int x1 */ 0,
        /*    int x2 */ 0,
        /* DWORD rop */ SRCCOPY);
    EndPaint(hwnd, &ps);
} // native::render_software

    void native::
render_opengl(const ::la::Window& win) noexcept {
    HWND hwnd = reinterpret_cast<HWND>(win.native().hwnd);
    PAINTSTRUCT ps;
    BeginPaint(hwnd, &ps);
    EndPaint(hwnd, &ps);
} // native::render_opengl

    void native::
on_geometry_change(::la::Window& win, int w, int h) noexcept {
    win.m_width = w;
    win.m_height = h;

    Error      error = Error::None;
    AboutError about = AboutError::None;

    // Switch between APIs
    switch (win.renderer_api())
    {
    case RendererApi::Software: {
        error = Error::CreateFramebuffer;
        about = win.fb().recreate(win, w, h);
        break;
    } // software

    case RendererApi::Opengl: {
        ::la::sleep(7); // hack
        error = Error::CreateOpenglContext;
        about = create_gl_context(reinterpret_cast<HDC>(win.native().hdc));

        if (about == AboutError::None)
            gl::viewport(0, 0, w, h);
        break;
    } // opengl

    default: {
        error = Error::RendererApiNotSet;
        about = AboutError::RendererApiNotSet;
    } // default
    } // switch renderer

    if (about != AboutError::None)
        win.handler().on_error(error, about);

    win.handler().on_resize(w, h);
    win.render();
} // native::on_geometry_change

// --------------------------- Window ---------------------------

Window::
Window(IWindowEvents& handler, int w, int h,
    RendererApi api,
    bool shown,
    bool bordless) noexcept
    : m_handler{ handler }, m_width{ w }, m_height{ h },
      m_fb{} {
#ifdef LA_NOSTD
    static bool is_registered = false;
    if (!is_registered) {
        WNDCLASS wc{};
        wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = win_proc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = WINDOW_CLASSNAME;

        if (!RegisterClassW(&wc))
            m_handler.on_error(Error::CreateWindow, AboutError::Win32_WindowClass);
        is_registered = true;
    }
#else // has STD
    static thread_local
    struct ThreadInstance {
        HMODULE hinstance;

        ThreadInstance() noexcept : hinstance{ GetModuleHandleW(nullptr) } {
            WNDCLASS wc{};
            wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc = win_proc;
            wc.hInstance = hinstance;
            wc.lpszClassName = WINDOW_CLASSNAME;

            if (!RegisterClassW(&wc))
                hinstance = nullptr;
        }
    } thread_ctx;

    if (!thread_ctx.hinstance) {
        m_handler.on_error(Error::CreateWindow, AboutError::Win32_WindowClass);
        return;
    }
#endif // LA_NOSTD

    // Window
    m_native.hwnd = reinterpret_cast<void *>(
        CreateWindowExW(
        /* dwExStyle    */ 0,
        /* lpClassName  */ WINDOW_CLASSNAME,
        /* lpWindowName */ WINDOW_CLASSNAME,
        /* dwStyle      */ bordless ? WS_POPUP : WS_OVERLAPPED  |
                                                 WS_CAPTION     |
                                                 WS_SYSMENU     |
                                                 WS_THICKFRAME  |
                                                 WS_MINIMIZEBOX |
                                                 WS_MAXIMIZEBOX |
                                                 (shown ? WS_VISIBLE : 0),
        /* x            */ CW_USEDEFAULT,
        /* y            */ CW_USEDEFAULT,
        /* width        */ w,
        /* height       */ h,
        /* hWndParent   */ nullptr,
        /* hMenu        */ nullptr,
#ifdef LA_NOSTD
        /* hInstance    */ GetModuleHandleW(nullptr),
#else // NOT freestanding
        /* hInstance    */ thread_ctx.hinstance,
#endif
        /* lpParam      */ this));
    if (!native().hwnd) {
        m_handler.on_error(Error::CreateWindow, AboutError::Win32_Window);
        return;
    }

    // DC
    m_native.hdc = reinterpret_cast<void *>(
        GetDC(reinterpret_cast<HWND>((native().hwnd))));

    if (!native().hdc) {
        m_handler.on_error(Error::CreateWindow, AboutError::Win32_WindowDc);
        return;
    }
} // Window

// --------------------------- Window Procedures ---------------------------

// Private procedures

void Window::
swap_buffer_software() const noexcept {
    InvalidateRect(reinterpret_cast<HWND>(m_native.hwnd), nullptr, FALSE);
    UpdateWindow(reinterpret_cast<HWND>(m_native.hwnd));
}
void Window::
swap_buffer_opengl() const noexcept {
#ifdef __LA_CXX17 // Wrap it in macro: Cannot access PF_DESCRIPTOR at runtime
    static_assert(wingl::PF_DESCRIPTOR.dwFlags & PFD_DOUBLEBUFFER,
        "OpenGL renderer requires double buffering");
#endif
    SwapBuffers(reinterpret_cast<HDC>(m_native.hdc));
}

// Public procedures

void Window::
show(bool show) const noexcept {
    ShowWindow(reinterpret_cast<HWND>(m_native.hwnd), show ? SW_SHOW : SW_HIDE);
}

void Window::
quit() const noexcept {
    PostMessageW(reinterpret_cast<HWND>(m_native.hwnd), WM_QUIT, 0, 0);
}

// --------------------------- Window Getters ---------------------------

bool Window::
poll_events() const noexcept {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT)
            return false;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return true;
} // poll_events

// --------------------------- Window Setters ---------------------------

void Window::
set_renderer(RendererApi api) noexcept {
    if (api == m_renderer_api) return;

    // Destroy old renderer
    switch (m_renderer_api) {
    case RendererApi::Software:
        fb().~Framebuffer();
        break;
    case RendererApi::Opengl:
        gl().~OpenglContext();
        break;
    }

    m_renderer_api = api;
    native::on_geometry_change((*this), width(), height());
} // set_renderer

void Window::
set_title(const char* title) const noexcept {
    constexpr size_t BUF_SIZE = 512;
    wchar_t wide_buf[BUF_SIZE] = {};

    int len = MultiByteToWideChar(/*       CodePage */ CP_UTF8,
                                  /*        dwFlags */ 0,
                                  /* lpMultiByteStr */ title,
                                  /*    cbMultiByte */ -1,
                                  /*  lpWideCharStr */ wide_buf,
                                  /*    cchWideChar */ BUF_SIZE);

    if (len > 0) {
        SetWindowTextW(reinterpret_cast<HWND>(m_native.hwnd), wide_buf);
    }
    else {
        // Fallback: set empty title
        SetWindowTextW(reinterpret_cast<HWND>(m_native.hwnd), L"");
    }
} // set title

void Window::
set_cursor_visible(bool value) const noexcept {
    int shown = ShowCursor(value);
    while ((value && shown < 0) || (!value && shown >= 0)) {
        shown = ShowCursor(value);
    }
}

void Window::
set_fullscreen(bool value) const noexcept {
    HWND hwnd = reinterpret_cast<HWND>(m_native.hwnd);

    if (value) {
        // Remove window borders and make fullscreen
        SetWindowLongPtr(hwnd, GWL_STYLE,
            GetWindowLongPtr(hwnd, GWL_STYLE) & ~WS_OVERLAPPEDWINDOW);

        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(MONITORINFO) };
        if (GetMonitorInfoW(monitor, &mi)) {
            SetWindowPos(/*   hWnd */ hwnd, 
                         /* hWndInsertAfter */ HWND_TOP,
                         /*      X */ mi.rcMonitor.left, 
                         /*      Y */ mi.rcMonitor.top,
                         /*     cx */ mi.rcMonitor.right - mi.rcMonitor.left,
                         /*     cy */ mi.rcMonitor.bottom - mi.rcMonitor.top,
                         /* uFlags */ SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else {
        // Restore fixed windowed style (standard overlapped window)
        SetWindowLongW(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);

        // Optional: set default size and center
        SetWindowPos(hwnd, HWND_NOTOPMOST,
            100, 100, 1280, 720,
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
} // set_fullscreen

// ------------------------------- Key ----------------------------------------

LA_NO_DISCARD bool is_pressed(Key k) noexcept { return key_array[static_cast<unsigned int>(k)]; }

LA_NO_DISCARD const char* get_key_name(Key k) noexcept {
    using K = Key;
    switch (k)
    {
    case K::F1:  return "f1";
    case K::F2:  return "f2";
    case K::F3:  return "f3";
    case K::F4:  return "f4";
    case K::F5:  return "f5";
    case K::F6:  return "f6";
    case K::F7:  return "f7";
    case K::F8:  return "f8";
    case K::F9:  return "f9";
    case K::F10: return "f10";
    case K::F11: return "f11";
    case K::F12: return "f12";
    case K::Shift:      return "shift";
    case K::Control:    return "control";
    case K::Alt:        return "alt";
    case K::Super:      return "super";
    case K::Escape:     return "escape";
    case K::Insert:     return "insert";
    case K::Delete:     return "delete";
    case K::Backspace:  return "backspace";
    case K::Tab:        return "tab";
    case K::Return:     return "return";
    case K::ScrollLock: return "scroll lock";
    case K::NumLock:    return "num lock";
    case K::CapsLock:   return "caps lock";
    case K::Home:       return "home";
    case K::End:        return "end";
    case K::PageUp:     return "page up";
    case K::PageDown:   return "page down";
    case K::Left:       return "left";
    case K::Up:         return "up";
    case K::Right:      return "right";
    case K::Down:       return "down";
    case K::MouseLeft:  return "LMB";
    case K::MouseRight: return "RMB";
    case K::MouseMiddle: return "middle mouse";
    case K::MouseX1:    return "x1 MB";
    case K::MouseX2:    return "x2 MB";
    case K::_0: return "0";
    case K::_1: return "1";
    case K::_2: return "2";
    case K::_3: return "3";
    case K::_4: return "4";
    case K::_5: return "5";
    case K::_6: return "6";
    case K::_7: return "7";
    case K::_8: return "8";
    case K::_9: return "9";
    case K::A: return "a";
    case K::B: return "b";
    case K::C: return "c";
    case K::D: return "d";
    case K::E: return "e";
    case K::F: return "f";
    case K::G: return "g";
    case K::H: return "h";
    case K::I: return "i";
    case K::J: return "j";
    case K::K: return "k";
    case K::L: return "l";
    case K::M: return "m";
    case K::N: return "n";
    case K::O: return "o";
    case K::P: return "p";
    case K::Q: return "q";
    case K::R: return "r";
    case K::S: return "s";
    case K::T: return "t";
    case K::U: return "u";
    case K::V: return "v";
    case K::W: return "w";
    case K::X: return "x";
    case K::Y: return "y";
    case K::Z: return "z";
    case K::Grave:      return "`";
    case K::Hyphen:     return "-";
    case K::Equal:      return "=";
    case K::BracketLeft:    return "[";
    case K::BracketRight:   return "]";
    case K::Comma:  return ",";
    case K::Period: return ".";
    case K::Slash:  return "/";
    case K::Backslash:  return "\\";
    case K::Semicolon:  return ";";
    case K::Apostrophe: return "'";
    default:            return "unknown";
    }
}

} // namespace la



// --------------------------- Create Opengl Context --------------------------

LA_NO_DISCARD ::la::AboutError
create_gl_context(HDC main_dc) noexcept {
    wingl::PFNWGLCHOOSEPIXELFORMATARBPROC    
        wglChoosePixelFormatARB = nullptr;

    wingl::PFNWGLCREATECONTEXTATTRIBSARBPROC 
        wglCreateContextAttribsARB = nullptr;

    // 1. Create a dummy window for loading WGL extensions
#if !defined(LA_NOSTD)
    static thread_local
#endif
    DummyWindow dummy{};
    if (!dummy.ctx()) {

        // 1.1 Dummy window
        if (!dummy.histance()) return ::la::AboutError::Win32_Dummy_WindowClass;
        if (!dummy.hwnd())     return ::la::AboutError::Win32_Dummy_Window;
        if (!dummy.hdc())      return ::la::AboutError::Win32_Dummy_WindowDc;

        // 1.2. Dummy context for loading WGL extensions
        { // Choose and set pixel format
            int format = ChoosePixelFormat(dummy.hdc(), &wingl::PF_DESCRIPTOR);
            if (format == 0)
                return ::la::AboutError::Win32_Dummy_ChoosePixelFormat;

            if (!SetPixelFormat(dummy.hdc(), format, &wingl::PF_DESCRIPTOR))
                return ::la::AboutError::Win32_Dummy_SetPixelFormat;
        }

        // 1.3. Load WGL extensions
        // Dummy Context
        dummy.set_ctx(wglCreateContext(dummy.hdc()));

        if (!wglMakeCurrent(dummy.hdc(), dummy.ctx()))
            return ::la::AboutError::Win32_Dummy_CreateContext;

        // wgl functions
        wglChoosePixelFormatARB =
            (wingl::PFNWGLCHOOSEPIXELFORMATARBPROC)
            (void*)wglGetProcAddress("wglChoosePixelFormatARB");

        wglCreateContextAttribsARB =
            (wingl::PFNWGLCREATECONTEXTATTRIBSARBPROC)
            (void*)wglGetProcAddress("wglCreateContextAttribsARB");
        // Loaded wgl functions
    } // if (!dummy.ctx())

    if (!wglChoosePixelFormatARB) return ::la::AboutError::Win32_Missing_ChoosePixelFormatARB;
    if (!wglCreateContextAttribsARB) return ::la::AboutError::Win32_Missing_CreateContextAttribsARB;

    // 3. Set modern pixel format for the main window
    { // Choose and set pixel format using ARB
        int format;
        UINT num_format;
        BOOL success = wglChoosePixelFormatARB(
            main_dc, wingl::PIXEL_ATTRS, nullptr, 1, &format, &num_format);
        
        if (!success || num_format == 0 || format == 0)              return ::la::AboutError::Win32_ChoosePixelFormatARB;
        if (!SetPixelFormat(main_dc, format, &wingl::PF_DESCRIPTOR)) return ::la::AboutError::Win32_SetPixelFormat;
    } // Choosed pixel format for main DC

    // 4. Create OpenGL 3.3 Core Profile context
    HGLRC gl_context = wglCreateContextAttribsARB(main_dc, nullptr, wingl::CONTEXT_ATTRS);
    if (!gl_context)                          return ::la::AboutError::Win32_CreateContextAttribsARB;
    if (!wglMakeCurrent(main_dc, gl_context)) return ::la::AboutError::Win32_CreateModernContext;

    // Enable VSync
    /*typedef BOOL(APIENTRY* PFNWGLSWAPINTERVALEXTPROC)(int interval);
    PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT =
        (PFNWGLSWAPINTERVALEXTPROC)(void*)wglGetProcAddress(
            "wglSwapIntervalEXT");
    if (!wglSwapIntervalEXT)
        return AboutError::Win32_Missing_SwapIntervalEXT;

    wglSwapIntervalEXT(1);*/

    // Check out current context
    if (!wglGetCurrentContext()) return ::la::AboutError::Win32_GetCurrentContext;
    if (!wglGetCurrentDC())      return ::la::AboutError::Win32_GetCurrentDC;
    return ::la::AboutError::None;
} // create_gl_context

// --------------------------- Key Mapper ---------------------------

static ::la::Key map_key(WPARAM wparam) noexcept {
    using K = ::la::Key;

#ifdef LA_CXX_17
    static_assert(1 == static_cast<int>(K::F1));
#endif // LA_CXX_17

    // F-keys F1-F12
    if (wparam >= VK_F1 && wparam <= VK_F12)
        return static_cast<K>(wparam - VK_F1 + 1);

    // Digits 0-9 (VK_0 = 0x30)
    if (wparam >= '0' && wparam <= '9') // _0.._9 = 0..9
        return static_cast<K>(wparam - '0' + static_cast<int>(K::_0));

    // Numpad digits
    if (wparam >= VK_NUMPAD0 && wparam <= VK_NUMPAD9)
        return static_cast<K>(wparam - VK_NUMPAD0 + static_cast<int>(K::_0));

    // Letters A-Z (VK_A = 0x41)
    if (wparam >= 'A' && wparam <= 'Z') // A..Z = 10..35
        return static_cast<K>(wparam - 'A' + static_cast<int>(K::A));

    switch (wparam)
    {
    case VK_PRIOR:      return K::PageUp;
    case VK_NEXT:       return K::PageDown;
    case VK_SHIFT:      return K::Shift;
    case VK_CONTROL:    return K::Control;
    case VK_MENU /* it isn't key `super` */: return K::Alt;
    case VK_LWIN:       return K::Super;
    case VK_ESCAPE:     return K::Escape;
    case VK_INSERT:     return K::Insert;
    case VK_DELETE:     return K::Delete;
    case VK_BACK:       return K::Backspace;
    case VK_TAB:        return K::Tab;
    case VK_RETURN:     return K::Return;
    case VK_SCROLL:     return K::ScrollLock;
    case VK_NUMLOCK:    return K::NumLock;
    case VK_CAPITAL:    return K::CapsLock;
    case VK_HOME:       return K::Home;
    case VK_END:        return K::End;
    case VK_LEFT:       return K::Left;
    case VK_UP:         return K::Up;
    case VK_RIGHT:      return K::Right;
    case VK_DOWN:       return K::Down;
    case VK_LBUTTON:    return K::MouseLeft;
    case VK_RBUTTON:    return K::MouseRight;
    case VK_MBUTTON:    return K::MouseMiddle;
    case VK_XBUTTON1:   return K::MouseX1;
    case VK_XBUTTON2:   return K::MouseX2;
    
    case VK_SPACE:      return K::Space;
    case VK_OEM_3:      return K::Grave; // Grave (`)
    case VK_OEM_MINUS:  return K::Hyphen; // Hyphen (-)
    case VK_OEM_PLUS:   return K::Equal; // Equal (=)


    case VK_OEM_4:      return K::BracketLeft;  // [
    case VK_OEM_6:      return K::BracketRight; // ]
    case VK_OEM_COMMA:  return K::Comma;        // ,
    case VK_OEM_PERIOD: return K::Period;       // .
    case VK_OEM_2:      return K::Slash;
    case VK_OEM_5:      return K::Backslash;
    case VK_OEM_1:      return K::Semicolon;    // ;
    case VK_OEM_7:      return K::Apostrophe;   // '

    default:            return K::__NONE__;
    } // switch
} // map_key

static inline ::la::Key handle_key(WPARAM wparam, bool pressed) noexcept {
    ::la::Key k;

#ifdef LA_CXX_17
    static_assert(sizeof(::la::Key) == sizeof(int));
    static_assert(sizeof(k) == sizeof(int));
#endif
    // Try to search for general key, otherwise returns `__NONE__` (0) as flag.
    k = map_key(wparam);

    // No needs to check out for `__NONE__`, we simply write the value.
    key_array[static_cast<int>(k)] = pressed ? 1 : 0;
    return k;
} // handle_key


// --------------------------- Win Proc ---------------------------

static LRESULT CALLBACK
win_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) noexcept {
    ::la::Window* win = reinterpret_cast<::la::Window*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_PAINT: {
        win->render();
        return 0;
    } // WM_PAINT

    case WM_SIZE: {
        if (win->renderer_api() == ::la::RendererApi::None)
            return 0;

        // Window Minimized
        if (wparam == SIZE_MINIMIZED) {
            win->handler().on_focus_change(false);
            return 0; // Handled
        }
        // Window Resized
        RECT rect;
        GetClientRect(reinterpret_cast<HWND>(win->native().hwnd), &rect);
        ::la::native::on_geometry_change(*win, static_cast<int>(rect.right - rect.left),
                                               static_cast<int>(rect.bottom - rect.top));
        return 0;
    } // WM_SIZE

    case WM_MOUSEMOVE:
        win->handler().on_mouse_move(/* X pos */ LOWORD(lparam),
                                     /* Y pos */ HIWORD(lparam));
        return 0;

    case WM_SETFOCUS:
        win->handler().on_focus_change(true);
        return 0;

    case WM_KILLFOCUS:
        win->handler().on_focus_change(false);
        return 0;

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wparam); // +120 or -120
        win->handler().on_scroll_vertical(delta / 120.f);
        return 0;
    }

    case WM_KEYDOWN: {
        ::la::Key k = handle_key(wparam, true);
        win->handler().on_key_down(k);
        return 0;
    }
    case WM_KEYUP: {
        ::la::Key k = handle_key(wparam, false);
        win->handler().on_key_up(k);
        return 0;
    }

    case WM_SYSKEYDOWN:
        if (wparam == VK_F10)       win->handler().on_key_down(::la::Key::F10);
        else if (wparam == VK_MENU) win->handler().on_key_down(::la::Key::Alt);
        return 0;

    case WM_SYSKEYUP:
        if (wparam == VK_F10)       win->handler().on_key_up(::la::Key::F10);
        else if (wparam == VK_MENU) win->handler().on_key_up(::la::Key::Alt);
        return 0;

    case WM_NCCREATE: {
        auto create_struct = reinterpret_cast<CREATESTRUCT*>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)(create_struct->lpCreateParams));
        return TRUE;
    } // WM_NCCREATE

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    } // switch (msg)

    return DefWindowProc(hwnd, msg, wparam, lparam);
} // win_proc

#endif // _WIN32