#include "la/la.hpp"
#include "la/gl.hpp"

namespace gl = la::gl;

struct Window : public la::IWindowEvents {
    la::Window win;
    la::Simd simd;

    Window() noexcept : win{ *this }  {
        win.set_renderer(la::RendererApi::Software);
    }

    void on_render_software() noexcept override {
        const uint32_t BLUE = 0xFF0000FF;
        int w = win.width();
        int h = win.height();
        win.fb().clear(simd, BLUE, w, h); // Blue color
    }

    void on_render_opengl() noexcept override {
        gl::clear_color(1.f, 0.f, 1.f, 1.f); // Pink color
        gl::clear(gl::BUFFER_BIT.COLOR);
    }
};

struct Application : public Window {
    inline Application() noexcept {
        win.set_title("LA Example - 東京 – القاهرة – Αθήνα – Київ – Zürich");
        win.show(true);

        la::Out out;
        out << "CPU Info / プロセッサ情報 / معلومات المعالج / ЦП інфо / Πληροφορίες CPU\n"
            << "\t avx: " << simd.has_avx() << "\n"
            << "\t avx2: " << simd.has_avx2() << "\n"
            << "\t sse: " << simd.has_sse() << "\n"
            << "\t sse2: " << simd.has_sse2() << "\n"
            << "\t avx512bw: " << simd.has_avx512bw() << "\n";
        out.flush();

    }
}; // struct Application

inline void run() noexcept {
    Application app;

    // Timer things
    uint32_t last_time = la::get_monotonic_ms();
    uint32_t dt;

    // Main loop
    while (app.win.poll_events()) {
        // Calculate delta time
        uint32_t current_time = la::get_monotonic_ms();
        dt = current_time - last_time;
        last_time = current_time;

        app.win.render();

        // Wait
        __LA_CONSTEXPR_VAR uint32_t TARGET_FRAME_MS = 16;
        if (dt < TARGET_FRAME_MS)
            la::sleep(TARGET_FRAME_MS - dt);
    } // while


    // app.win.set_renderer(la::RendererApi::None); // ERROR. Shows message to the user (DEBUG only)
} // run

int main() {
    run();
    la::exit_process(0);
    return 0;
} // main()