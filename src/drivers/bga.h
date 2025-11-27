/**
 * BGA (Bochs Graphics Adapter) Driver Header
 * Supports QEMU, Bochs, VirtualBox
 */

#ifndef BGA_H
#define BGA_H

#include <stdint.h>

/* BGA I/O Ports */
#define VBE_DISPI_IOPORT_INDEX   0x01CE
#define VBE_DISPI_IOPORT_DATA    0x01CF

/* BGA Register Indices */
#define VBE_DISPI_INDEX_ID              0x0
#define VBE_DISPI_INDEX_XRES            0x1
#define VBE_DISPI_INDEX_YRES            0x2
#define VBE_DISPI_INDEX_BPP             0x3
#define VBE_DISPI_INDEX_ENABLE          0x4
#define VBE_DISPI_INDEX_BANK            0x5
#define VBE_DISPI_INDEX_VIRT_WIDTH      0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT     0x7
#define VBE_DISPI_INDEX_X_OFFSET        0x8
#define VBE_DISPI_INDEX_Y_OFFSET        0x9

/* BGA Version IDs */
#define VBE_DISPI_ID0                   0xB0C0
#define VBE_DISPI_ID1                   0xB0C1
#define VBE_DISPI_ID2                   0xB0C2
#define VBE_DISPI_ID3                   0xB0C3
#define VBE_DISPI_ID4                   0xB0C4
#define VBE_DISPI_ID5                   0xB0C5

/* BGA Enable Flags */
#define VBE_DISPI_DISABLED              0x00
#define VBE_DISPI_ENABLED               0x01
#define VBE_DISPI_LFB_ENABLED           0x40
#define VBE_DISPI_NOCLEARMEM            0x80

/* BGA Bit Depths */
#define VBE_DISPI_BPP_4                 0x04
#define VBE_DISPI_BPP_8                 0x08
#define VBE_DISPI_BPP_15                0x0F
#define VBE_DISPI_BPP_16                0x10
#define VBE_DISPI_BPP_24                0x18
#define VBE_DISPI_BPP_32                0x20

/* PCI IDs for BGA device */
#define BGA_PCI_VENDOR_ID               0x1234
#define BGA_PCI_DEVICE_ID               0x1111

/* BGA Functions */
int bga_is_available(void);
int bga_init(uint16_t width, uint16_t height, uint16_t bpp);
void bga_set_video_mode(uint16_t width, uint16_t height, uint16_t bpp);
uint32_t bga_get_framebuffer_addr(void);
uint32_t bga_get_framebuffer_size(void);
uint16_t bga_get_width(void);
uint16_t bga_get_height(void);

/* Drawing primitives */
void bga_clear(uint32_t color);
void bga_putpixel(int x, int y, uint32_t color);
void bga_fill_rect(int x, int y, int width, int height, uint32_t color);
void bga_draw_rect(int x, int y, int width, int height, uint32_t color);
void bga_draw_bmp(int x, int y, const uint8_t *bmp_data);

/* Text output - kernel manages cursor position */
void bga_print(const char *str, uint32_t fg, uint32_t bg);
void bga_print_at(int x, int y, const char *str, uint32_t fg, uint32_t bg);
void bga_set_cursor(int x, int y);
void bga_get_cursor(int *x, int *y);

/* Internal helper functions */
void bga_write_register(uint16_t index, uint16_t value);
uint16_t bga_read_register(uint16_t index);

#endif /* BGA_H */
