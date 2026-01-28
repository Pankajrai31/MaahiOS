#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <stdint.h>

// Forward declarations
typedef struct UIControl UIControl;
typedef struct UIWindow UIWindow;

/**
 * Input Handling Module
 * Processes mouse and keyboard events
 */

typedef struct {
    int hover_control;
    int last_click_time;
    int last_click_control;
    unsigned int last_buttons;
} InputState;

void input_init(InputState* state);
int input_process_mouse(InputState* state, int mx, int my, unsigned int buttons, 
                        int frame_count, UIControl* controls, int max_controls,
                        UIWindow* windows, int max_windows, int screen_width, int screen_height,
                        int* needs_redraw);

#endif // INPUT_HANDLER_H
