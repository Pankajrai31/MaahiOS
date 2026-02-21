/**
 * MaahiOS - Button Implementation
 */

#include "button.h"
#include "../../core/executive/cellexecutive/cellexecutive.h"

int maahi_create_button(int window_id, int x, int y, int width, int height, const char *text) {
    /* Use Cell Executive to queue button creation request */
    return cellexec_create_button(x, y, width, height, text);
}

int maahi_button_create(int window_id, int x, int y, int width, int height,
                        const char *text, int type, int size) {
    /* Use Cell Executive to queue button creation request */
    return cellexec_create_button(x, y, width, height, text);
}
