#ifndef MYOS_GUI_DESKTOP_HPP
#define MYOS_GUI_DESKTOP_HPP

#include "../core/gui_types.hpp"

extern "C" {
#include "graphics.h"
}

namespace myos::gui {

class Desktop {
public:
    static Rect start_menu_rect(const Metrics &metrics);
    static void draw(graphics_surface *surface, const Metrics &metrics, bool start_menu_open,
                     bool terminal_open, bool terminal_minimized,
                     bool files_open, bool files_minimized,
                     bool monitor_open, bool monitor_minimized);
};

} // namespace myos::gui

#endif
