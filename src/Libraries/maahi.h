/**
 * MaahiOS Application Library
 * 
 * This is the ONLY header apps need to include.
 * Provides simple, clean functions for:
 *   - Creating windows and controls
 *   - Handling events (clicks, keyboard)
 *   - Drawing graphics
 *   - Process management
 *   - Filesystem access
 *   - Debug output
 * 
 * Example usage:
 * 
 *   #include <maahi.h>
 *   
 *   void app_main() {
 *       int win = maahi_create_window(100, 100, 400, 300, "My App");
 *       int btn = maahi_create_button(win, 10, 50, 100, 30, "Click Me");
 *       
 *       while (1) {
 *           MaahiEvent e;
 *           if (maahi_poll_event(&e)) {
 *               if (e.type == MAAHI_EVENT_CLICK && e.control_id == btn) {
 *                   // Button was clicked!
 *               }
 *           }
 *           maahi_yield();  // Let other apps run
 *       }
 *   }
 */

#ifndef MAAHI_H
#define MAAHI_H

/* Include all component headers */
#include "gui/event/event.h"
#include "gui/window/window.h"
#include "gui/button/button.h"
#include "gui/label/label.h"
#include "gui/icon/icon.h"
#include "gui/list/list.h"
#include "gui/graphics/graphics.h"
#include "gui/text/text.h"
#include "gui/menu/menu.h"
#include "process/process.h"
#include "filesystem/filesystem.h"
#include "debug/debug.h"

#endif /* MAAHI_H */
