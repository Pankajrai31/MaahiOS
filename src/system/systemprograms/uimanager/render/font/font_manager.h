#ifndef FONT_MANAGER_H
#define FONT_MANAGER_H
#include <stdint.h>
#define FONT_SIZE_14  14
void font_init(void);
int font_get_line_height(int size);
int font_draw_char(int x, int y, char c, uint32_t color, int size, uint32_t *fb, int sw);
int font_draw_string(int x, int y, const char* text, uint32_t color, int size, uint32_t *fb, int sw);
int font_get_char_width(char c, int size);
int font_get_string_width(const char* text, int size);
#endif
