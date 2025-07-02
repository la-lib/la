#ifndef __lalib
#define __lalib 202507L

#include <stddef.h> // size_t
#include <stdint.h> // uint32_t

// Replace `main` with `WinMain`.
#if defined(_MSC_VER) && !defined(_DEBUG) && !defined(_CONSOLE)
// Forward declarations to avoid <Windows.h>
typedef struct HINSTANCE__ *HINSTANCE;
typedef char *LPSTR;

/* Detected Windows GUI application without console.
   The main() function was quietly changed to WinMain()
   main()'s arguments are NOT available. */
#define main() __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
#endif

// Actual version of C++
#if defined(_MSC_VER)
#  define COMPILER_MSVC _MSC_VER
#endif // _MSC_VER

#if defined(_MSVC_LANG)
#  define CPP_VERSION _MSVC_LANG
#else
#  define CPP_VERSION __cplusplus
#endif // MSVC_LANG

// `restrict` keyword
#if __cplusplus < 202302L // `restrict` was added in C++23
#  ifndef restrict
#    ifdef __GNUC__ // gnu compiler
#      define restrict __restrict__
#    elif defined(_MSC_VER) // msvc
#      define restrict __restrict
#    else // fallback
#      define restrict
#    endif
#  endif // restrict
#endif // __cplusplus < 202302

namespace la
{
// ========================== traits =====================================

namespace traits
{
template <typename T> [[nodiscard]] consteval bool is_trivially_copyable()
{
#if defined(__clang__) || defined(_MSC_VER) // clang || msvc
    return __is_trivially_copyable(T);
#elif defined(__GNUC__) && __GNUC__ >= 5 // gnu >= 5 version
    return __is_trivially_copyable(T);
#elif defined(__GNUC__)                  // gnu < 5 version
    return __has_trivial_copy(T) && __has_trivial_assign(T) && __has_trivial_destructor(T);
#else                                    // no fallbacks
    static_assert(false, "Compiler doesn't support trivially copyable detection");
    return false;
#endif
} // is_trivially_copyable

// === Remove reference ===
template <typename T> struct remove_reference
{
    using type = T;
};
template <typename T> struct remove_reference<T &>
{
    using type = T;
};
template <typename T> struct remove_reference<T &&>
{
    using type = T;
};
template <typename T> using remove_reference_t = typename remove_reference<T>::type;

// === Remove cv (const/volatile) ===
template <typename T> struct remove_cv
{
    using type = T;
};
template <typename T> struct remove_cv<const T>
{
    using type = T;
};
template <typename T> struct remove_cv<volatile T>
{
    using type = T;
};
template <typename T> struct remove_cv<const volatile T>
{
    using type = T;
};
template <typename T> using remove_cv_t = typename remove_cv<T>::type;

// === Remove both cv and reference ===
template <typename T> using remove_cvref_t = remove_cv_t<remove_reference_t<T>>;

template <typename T> [[nodiscard]] constexpr T &&forward(traits::remove_cvref_t<T> &t) noexcept
{
    return static_cast<T &&>(t);
}
} // namespace traits

namespace mem
{
// === Simple memcpy for freestanding ===
static inline void *copy(void *restrict dest, const void *restrict src, size_t n) noexcept {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    for (size_t i = 0; i < n; ++i)
        d[i] = s[i];
    return dest;
} // copy

// === Simple memmove for freestanding ===
static inline void *move(void *restrict dest, const void *restrict src, size_t n) noexcept {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    if (d == s || n == 0) return dest;
    if (d < s || d >= s + n) {
        // non-overlapping or destination before source: forward copy
        for (size_t i = 0; i < n; ++i)
            d[i] = s[i];
    } else {
        // overlapping regions, copy backwards
        for (size_t i = n; i-- > 0;)
            d[i] = s[i];
    }
    return dest;
} // move

// === Generic byte swap ===
template <typename U> [[nodiscard]] constexpr U byteswap(U v) noexcept {
    U r = 0;
    unsigned char       *pr =       (unsigned char *)&r;
    const unsigned char *pv = (const unsigned char *)&v;
    for (size_t i = 0; i < sizeof(U); ++i)
        pr[sizeof(U) - 1 - i] = pv[i];
    return r;
} // byteswap

// === Endian conversion ===
[[nodiscard]] static inline const bool is_little_endian() noexcept {
    union {
        uint16_t u16;
        uint8_t u8[2];
    } test{0x0100};
    return test.u8[1] == 1;
} // is_little_endian

template <typename U> [[nodiscard]] constexpr U to_little_endian(U v) noexcept {
    if (is_little_endian())
        return v;
    else
        return byteswap(v);
} // to_little_endian

template <typename U> [[nodiscard]] constexpr U from_little_endian(U v) noexcept {
    if (is_little_endian())
        return v;
    else
        return byteswap(v);
} // from_little_endian

// ===================== Tuple of pointers ===============================
// Apply a lambda to each field in tuple

template <auto... Ptrs> struct Tuple
{
};

template <typename T, typename F, auto... Ptrs>
static inline void [[nodiscard]] apply(const T &obj, F &&f, Tuple<Ptrs...>) noexcept
{
    (f(obj, Ptrs), ...);
}

template <typename T, typename F, auto... Ptrs>
static inline void [[nodiscard]] apply(T &obj, F &&f, Tuple<Ptrs...>) noexcept
{
    (f(obj, Ptrs), ...);
}

// ==================== Serializer logic =================================

template <typename T> struct Serializer; // Serializer core

template <typename... Ts> [[nodiscard]] consteval size_t expected_size_seq(); // Expected size calculation

template <typename T>
    requires(traits::is_trivially_copyable<T>())
consteval void static_assert_blittable(); // Compile-time assert blittable
} // namespace byte

// =========================== math ======================================

namespace math
{
[[nodiscard]] constexpr float sin(float x) noexcept;
[[nodiscard]] constexpr float cos(float x) noexcept;
[[nodiscard]] constexpr float tan(float x) noexcept;

// Only one iteration. ~0.175 ulp.
[[nodiscard]] constexpr float sqrt(float x) noexcept;

[[nodiscard]] constexpr float floor(float x) noexcept;
[[nodiscard]] constexpr float radians(float degrees) noexcept;
} // namespace math

// =========================== vec =======================================

// Generic T-typed N-dimensional vector
template <typename T, size_t N> struct vec
{
  private:
    T v[N]{};

  public:
    constexpr vec() noexcept = default;

    vec(const T (&arr)[N]) noexcept
    {
        for (size_t i = 0; i < N; ++i)
            v[i] = arr[i];
    }

    // Element access

    [[nodiscard]] constexpr
    T& operator[](size_t i) noexcept { return v[i]; }

    [[nodiscard]] constexpr
    T operator[](size_t i) const noexcept { return v[i]; }

    // Math operations
    [[nodiscard]] constexpr
    vec<T, N> operator+(const vec &other) const noexcept
    {
        vec r;
        for (size_t i = 0; i < N; ++i)
            r[i] = v[i] + other[i];
        return r;
    }

    [[nodiscard]] constexpr
    vec<T, N> operator-(const vec &other) const noexcept
    {
        vec r;
        for (size_t i = 0; i < N; ++i)
            r[i] = v[i] - other[i];
        return r;
    }

    [[nodiscard]] constexpr
    vec<T, N> operator*(T s) const noexcept
    {
        vec r;
        for (size_t i = 0; i < N; ++i)
            r[i] = v[i] * s;
        return r;
    }

    [[nodiscard]] constexpr static
    T dot(const vec &a, const vec &b) noexcept
    {
        T sum{};
        for (size_t i = 0; i < N; ++i)
            sum += a[i] * b[i];
        return sum;
    }

    [[nodiscard]] constexpr
    T dot() noexcept
    {
        T sum{};
        for (size_t i = 0; i < N; ++i)
            sum += (*this)[i] * (*this)[i];
        return sum;
    }

    // Vector length (magnitude)
    [[nodiscard]]
    T length() const noexcept
    {
        return math::sqrt(dot(*this, *this));
    }

    // Normalized vector (unit length)
    [[nodiscard]]
    vec<T, N> normalized() const noexcept
    {
        T len = length();
        return (len > T(0)) ? (*this) * (T(1) / len) : vec{};
    }

    // Component-wise minimum
    [[nodiscard]] constexpr static
    vec<T, N> min(const vec &a, const vec &b) noexcept
    {
        vec r;
        for (size_t i = 0; i < N; ++i)          // for every element
            r[i] = (a[i] < b[i]) ? a[i] : b[i]; // a < b
        return r;
    }

    // Component-wise maximum
    [[nodiscard]] constexpr static
    vec<T, N> max(const vec &a, const vec &b) noexcept
    {
        vec r;
        for (size_t i = 0; i < N; ++i)          // for every element
            r[i] = (a[i] > b[i]) ? a[i] : b[i]; // a > b
        return r;
    }
}; // struct vec
} // namespace la

// ========================== OS-dependent ================================
namespace la::os {
// =============================== Keys ===================================
enum class Key : unsigned char {
    Unknown = 0x00,
    // ========== FUNCTIONAL ==========
    F1 = 0x01,F2,F3,F4,F5,F6,F7,F8,F9,F10,F11,F12,
    // ========== MODIFIERS ==========
    Shift,
    Control,
    CapsLock,
    Alt,
    Super,
    // ========== TTY ==========
    Escape,
    Insert,
    Delete,
    Backspace,
    Tab,
    Return,
    ScrollLock,
    // ========== MOTION ==========
    Home,
    End,
    PageUp,
    PageDown,
    Left,
    Up,
    Right,
    Down,
    // ========== Mouse ==========
    MouseLeft,
    MouseRight,
    MouseMiddle,
    MouseScrollUp,
    MouseScrollDown,
    MouseX1,
    MouseX2,
    // ========== ASCII ==========
    Grave,  // grave,  shift -> Tilde (~)
    Hyphen, // hyphen, shift -> Underscore (_)
    Equal,  // equal,  shift -> Plus (+)
    
    digit_0,digit_1,digit_2,digit_3,digit_4,digit_5,digit_6,digit_7,digit_8,digit_9,
    
    A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z,
    __MAX__ // do not use. do not remove. internal stuff.
};          // enum class Key

[[nodiscard]] void *alloc(size_t size) noexcept;
void free(void *ptr, size_t size) noexcept;

// Exit current process (thread)
void exit_process(int error_code) noexcept;
void sleep(unsigned ms) noexcept;
[[nodiscard]] double monotonic_seconds() noexcept;

// =========================== Window ====================================

struct Window
{
    enum class Backend : size_t
    {
        Unknown,
        X11,
        WindowsApi,
        Cocoa,
        AndroidNDK,
    }; // enum class Backend
    constexpr static Backend BACKEND =
#if defined(__linux__)     // Linux
        Backend::X11;
#elif defined(_WIN32)      // Windows
        Backend::WindowsApi;
#elif defined(__APPLE__)   // Apple
        Backend::Cocoa;
#elif defined(__ANDROID__) // Android
        Backend::AndroidNDK;
#else                      // Unknown
        Backend::Unknown;
#endif
    static_assert(Window::BACKEND != Backend::Unknown && "Unknown backend");

    struct /* native */
    {
#if defined(__linux__)
        void *display;
        unsigned window;
#elif defined(_WIN32)
        void *hwnd;
        void *hdc;
#endif
    } native; // OS-dependent struct

    struct {
#if defined(__linux__)
#elif defined(_WIN32)
        void *hdc;
        void *bmp;
        void *pixels;
        int width;
        int height;
#endif
    } framebuffer;

    struct /* event */
    {
        using ResizeCallback = void (*)(Window &, int, int);
        using MouseMoveCallback = void (*)(Window &, int, int);
        using KeyUpCallback = void (*)(Window &, Key);
        using FocusChangeCallback = void (*)(Window &, bool);

        struct /* noop */ {
            static inline void on_resize(Window &, int, int) {}
            static inline void on_mouse_move(Window &, int, int) {}
            static inline void on_key_up(Window &, Key) {}
            static inline void on_focus_change(Window &, bool) {}
        } noop;

        ResizeCallback      on_resize       = noop.on_resize;
        MouseMoveCallback   on_mouse_move   = noop.on_mouse_move;
        KeyUpCallback       on_key_up       = noop.on_key_up;
        FocusChangeCallback on_focus_change = noop.on_focus_change;
    } event;

    // is_valid
#if defined(__linux__)
    [[nodiscard]] inline bool is_valid() const noexcept { return native.display != nullptr && native.window != 0; }
#elif defined(_WIN32)
    [[nodiscard]] inline bool is_valid() const noexcept { 
        return native.hwnd != nullptr 
            && native.hdc != nullptr
            && framebuffer.hdc != nullptr
            && framebuffer.bmp != nullptr
            && framebuffer.pixels != nullptr;
    }
#endif // is_valid

    explicit Window(int width, int height) noexcept;
    ~Window() noexcept;

    Window() = delete;
    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;
    Window(Window &&) = delete;
    Window &operator=(Window &&) = delete;

    // Use with `while`
    
    [[nodiscard]] bool is_poll_events() const noexcept;
    void proc_swap_buffers() const noexcept;

    // Procedures
    
    void proc_resize(int, int) const noexcept;
    void proc_mouse_move(int, int) const noexcept;
    void proc_key_up(Key) const noexcept;
    void proc_focus_change(bool) const noexcept;

    void proc_recreate_framebuffer(int width, int height) noexcept;
    void proc_show(bool) const noexcept;
    void proc_center_cursor(int x, int y) const noexcept;
    void proc_quit() const noexcept;
    
    // Getters
    
    [[nodiscard]] vec<int, 2> get_center_point() const noexcept;
    
    // Setters
    
    void set_pixel(int x, int y, uint32_t color) noexcept;
    void set_title(const char *title) const noexcept;
    void set_fullscreen(bool enabled) const noexcept;
    void set_cursor_visible(bool enabled) const noexcept;
}; // struct Window
} // namespace la::os

// ==========================================================================
//                         IMPLEMENTATION
// ==========================================================================

namespace la::mem {
// ======================= serializer logic ==============================

// Expected size calculation
template <typename... Ts> [[nodiscard]] consteval static size_t expected_size_seq()
{
    size_t offset = 0;
    size_t max_align = 1;
    (
        (max_align = alignof(Ts), 
        offset = ((offset + max_align - 1) / max_align) 
            * max_align + sizeof(Ts)),
    ...);
    // final struct align
    offset = ((offset + max_align - 1) / max_align) * max_align;
    return offset;
} // expected_size_seq

// === Serializer core ===
template <typename T> struct Serializer
{
    consteval static auto fields(); // must be specialized

    [[nodiscard]] static char *serialize(char *out, const T &value) noexcept
    {
        apply(
            value,
            [&](auto &obj, auto ptr) {
                using Field = decltype(obj.*ptr);
                Field tmp = to_little_endian(obj.*ptr);
                copy(out, &tmp, sizeof(Field));
                out += sizeof(Field);
            },
            fields());
        return out;
    }

    [[nodiscard]] static const char *deserialize(const char *in, T &value) noexcept
    {
        apply(
            value,
            [&](auto &obj, auto ptr) {
                using Field = decltype(obj.*ptr);
                Field tmp;
                mem::copy(&tmp, in, sizeof(Field));
                value.*ptr = from_little_endian(tmp);
                in += sizeof(Field);
            },
            fields());
        return in;
    }
}; // struct Serializer

// Compile-time assert blittable without std
template <typename T>
    requires(traits::is_trivially_copyable<T>())
consteval void static_assert_blittable()
{
    constexpr auto members = Serializer<T>::fields();
    using TupleType = decltype(members);

    constexpr size_t expected = expected_size_seq<T, TupleType>();
    constexpr size_t actual = sizeof(T);

    // Ensure layout matches fields()
    static_assert(expected == actual, "Padding or missing fields in structure.");
} // static_assert_blittable
} // namespace la::mem

// =========================== math ======================================

namespace la::math
{
constexpr float PI = 3.14159265359f;
constexpr float PI_HALF = 1.57079632679f;
constexpr float PI2 = 6.28318530718f;

[[nodiscard]] constexpr float sin(float x) noexcept
{
    bool flip = false;
    const float x2 = x * x;
    float result;

    // x to [0, 2π)
    while (x >= PI2)
        x -= PI2;
    while (x < 0.0f)
        x += PI2;

    // [−π/2, π/2]
    if (x > PI) {
        x -= PI;
        flip = true;
    }
    if (x > PI_HALF) x = PI - x;

    // [−π/2, π/2]
    result = x * (1.0f - x2 / 6.0f + (x2 * x2) / 120.0f);

    return flip ? -result : result;
}
[[nodiscard]] constexpr float cos(float x) noexcept
{
    return sin(x + PI_HALF);
}
[[nodiscard]] constexpr float tan(float x) noexcept
{
    const float x2 = x * x;
    // [−π, π)
    while (x > PI)
        x -= PI2;
    while (x < -PI)
        x += PI2;

    // tan(x) ≈ x + x^3/3 + 2x^5/15 + 17x^7/315
    return x + x * x2 * (1.0f / 3.0f + x2 * (2.0f / 15.0f + x2 * (17.0f / 315.0f)));
}
[[nodiscard]] constexpr float sqrt(float x) noexcept
{
    float x_half;
    int i;
    float y;
    if (x <= 0.0f) return 0.0f;

    x_half = 0.5f * x;
    i = *(int *)&x;            // reinterpret as int
    i = 0x5f3759df - (i >> 1); // magic number
    y = *(float *)&i;
    y = y * (1.5f - x_half * y * y); // 1st Newton-Raphson iteration
    return x * y;
}
[[nodiscard]] constexpr float floor(float x) noexcept
{
    int i = (int)x;
    if (x < 0.0f && x != static_cast<float>(i)) --i;
    return static_cast<float>(i);
}
[[nodiscard]] constexpr float radians(float degrees) noexcept
{
    return degrees * PI / 180.0f;
}
} // namespace la::math

#endif // __lalib