#ifndef ICONS_H
#define ICONS_H

#include "libgui.h"

/**
 * Simple Icon API for MaahiOS
 * Draw desktop icons by name
 * (IconType enum is defined in libgui.h)
 */

/**
 * Draw an icon at the specified position
 * For now, draws colored 64x64 squares (simpler than BMP)
 */
void icon_draw(IconType type, int x, int y);

/**
 * Draw an icon with label
 */
void icon_draw_with_label(IconType type, int x, int y, const char *label);

#endif // ICONS_H
