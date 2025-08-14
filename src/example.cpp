#include "la/la.hpp"
#include "la/gl.hpp"

namespace gl = la::gl;

struct Window : public la::IWindowEvents {
    la::Window win;
    la::Out out;
    uint32_t counter = 0;

    Window() noexcept : win{ *this } {
        win.set_renderer(la::RendererApi::Software);
    }

    void on_render_software() noexcept override {
        int w = win.width();
        int h = win.height();
        win.fb().clear(counter, w, h); // Blue color
        win.swap_buffer_software();
    }

    void on_render_opengl() noexcept override {
        gl::clear_color(1.f, 0.f, 1.f, 1.f); // Pink color
        gl::clear(gl::BUFFER_BIT.COLOR);
        win.swap_buffer_opengl();
    }

    void on_key_up(la::Key key) noexcept override {
        out << "Released key: " << la::get_key_name(key) << la::endl;
    }

    void on_key_down(la::Key key) noexcept override {
        out << "Pressed key: " << la::get_key_name(key) << la::endl;
    }
};

struct Application : public Window {
    inline Application() noexcept {
        win.set_title("LA Example / 東京 / القاهرة / Αθήνα / Київ / Zürich");
        win.show(true);
        out << "CPU Info / プロセッサ情報 / معلومات المعالج / ЦП інфо / Πληροφορίες CPU\n"
            << "\t avx: "   << la::Simd::has_avx()  << '\n'
            << "\t avx2: "  << la::Simd::has_avx2() << '\n'
            << "\t sse: "   << la::Simd::has_sse()  << '\n'
            << "\t sse2: "  << la::Simd::has_sse2() << '\n'
            << "\t avx512bw: " << la::Simd::has_avx512bw() << '\n';
        out.flush();
    }
}; // struct Application

static inline float get_fps(double dt_sec) noexcept {
    if (dt_sec <= 0.0)
        return 0.0f;
    return static_cast<float>(1.0 / dt_sec);
}

static inline float get_fps_smooth(double dt_sec) noexcept {
    static double fps_smooth = 0.0;
    const double alpha = 0.1;
    float fps = get_fps(dt_sec);
    fps_smooth = alpha * fps + (1.0 - alpha) * fps_smooth;
    return static_cast<float>(fps_smooth);
}

inline void run() noexcept {
    Application app;

    // Timer things
    double last_time = la::get_monotonic_secs();
    double dt;
    

    // Main loop
    while (app.win.poll_events()) {
        // Calculate delta time in seconds
        double current_time = la::get_monotonic_secs();
        dt = current_time - last_time;
        last_time = current_time;

        // app.out << "fps: " << get_fps_smooth(dt) << la::endl;


        app.win.render();
        app.counter++;

        // Wait
        __LA_CONSTEXPR_VAR float TARGET_FRAME = 66.6f;
        if (dt < TARGET_FRAME)
            la::sleep((TARGET_FRAME - dt) / 1000.f);
    } // while


    // app.win.set_renderer(la::RendererApi::None); // ERROR. Shows message to the user (DEBUG only)
} // run

int main() {
    la::GlobalInitializer::init();
    run();
    la::exit_process(0);
    return 0;
} // main()