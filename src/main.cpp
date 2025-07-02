#include "la.hpp"

int main() {
    // Create window
    la::os::Window win{ 600, 480 };
    if (!win.is_valid())
        return -1;

    // Show window
    win.proc_show(true);

    // Main loop
    while (win.is_poll_events()) {
        // Fill window with red colour
        for (int i = 0; i < win.framebuffer.width; ++i) {
            for (int j = 0; j < win.framebuffer.height; ++j) {
                win.set_pixel(i, j, 0xFFFF0000);
            }
        }
        // Update window buffer
        win.proc_swap_buffers();
    }
    
    // Exit the main process
    la::os::exit_process(0);
    return 0;
}