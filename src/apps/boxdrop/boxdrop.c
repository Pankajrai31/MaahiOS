/**
 * boxdrop.mex - MaahiOS BoxDrop Game
 *
 * A fun little arcade game for MaahiOS!
 *
 * Rules:
 *   - Red boxes fall from the top of the play area
 *   - Move your green paddle left/right with arrow keys
 *   - Boxes that reach the floor: +1 point
 *   - Boxes that hit your paddle: -1 point
 *   - Press ESC to quit
 *
 * Uses: libgui (drawing + keyboard), libprocess (timing)
 */

#include "../../system/libraries/liblog/liblog.h"
#include "../../system/libraries/libgui/libgui.h"
#include "../../system/libraries/libprocess/libprocess.h"

/*=============================================================================
 * GAME CONFIGURATION
 *===========================================================================*/

/* Play area dimensions (fixed game area, position computed at runtime) */
#define AREA_W      600
#define AREA_H      650

/* Computed at startup to center on screen */
static int AREA_X;
static int AREA_Y;

/* Inner play region (inside border) */
#define BORDER      2
static int PLAY_X;
static int PLAY_Y;
#define PLAY_W      (AREA_W - 2 * BORDER)
static int PLAY_H;

/* Player paddle */
#define PADDLE_W    60
#define PADDLE_H    14
static int PADDLE_Y;
#define PADDLE_SPEED 8

/* Falling boxes */
#define BOX_W       20
#define BOX_H       20
#define MAX_BOXES   12
#define DROP_SPEED  3
#define SPAWN_INTERVAL 12   /* ticks between spawns */

/* Colors */
#define COL_BACKGROUND  0x001A1A2E
#define COL_AREA_BG     0x002D2D44
#define COL_BORDER      0x00555577
#define COL_HEADER_BG   0x00222240
#define COL_BOX         0x00FF3333
#define COL_BOX_HIT     0x00FF8800
#define COL_PADDLE      0x0033FF66
#define COL_SCORE_FG    0x00FFFFFF
#define COL_TITLE_FG    0x0000CCFF
#define COL_FLOOR       0x00444466

/*=============================================================================
 * SIMPLE PRNG (Linear Congruential Generator)
 *===========================================================================*/

static unsigned int rng_state = 12345;

static void rng_seed(unsigned int s) {
    rng_state = s ? s : 1;
}

static unsigned int rng_next(void) {
    rng_state = rng_state * 1103515245 + 12345;
    return (rng_state >> 16) & 0x7FFF;
}

static int rng_range(int lo, int hi) {
    if (lo >= hi) return lo;
    return lo + (int)(rng_next() % (unsigned int)(hi - lo));
}

/*=============================================================================
 * GAME STATE
 *===========================================================================*/

typedef struct {
    int x, y;
    int active;
    int hit;       /* 1 = was caught by paddle (flash orange) */
    int flash;     /* countdown for hit flash */
} box_t;

static box_t boxes[MAX_BOXES];
static int paddle_x;
static int score;
static int tick;
static int spawn_timer;
static int game_over;

/*=============================================================================
 * HELPERS
 *===========================================================================*/

static void itoa_simple(int val, char *buf) {
    char tmp[16];
    int i = 0;
    int neg = 0;

    if (val < 0) {
        neg = 1;
        val = -val;
    }
    if (val == 0) {
        tmp[i++] = '0';
    } else {
        while (val > 0) {
            tmp[i++] = '0' + (val % 10);
            val /= 10;
        }
    }
    int p = 0;
    if (neg) buf[p++] = '-';
    while (i > 0) buf[p++] = tmp[--i];
    buf[p] = '\0';
}

/*=============================================================================
 * DRAWING
 *===========================================================================*/

static void draw_background(void) {
    /* Fill entire screen background */
    gui_fill_rect(0, 0, gui_get_screen_width(), gui_get_screen_height(), COL_BACKGROUND);

    /* Border */
    gui_fill_rect(AREA_X, AREA_Y, AREA_W, AREA_H, COL_BORDER);

    /* Header area */
    gui_fill_rect(AREA_X + BORDER, AREA_Y + BORDER,
                  AREA_W - 2 * BORDER, 40 - BORDER, COL_HEADER_BG);

    /* Title */
    gui_draw_string(AREA_X + 12, AREA_Y + 12,
                    "B O X   D R O P", COL_TITLE_FG, COL_HEADER_BG);

    /* Play area background */
    gui_fill_rect(PLAY_X, PLAY_Y, PLAY_W, PLAY_H, COL_AREA_BG);

    /* Floor line */
    gui_fill_rect(PLAY_X, PLAY_Y + PLAY_H - 2, PLAY_W, 2, COL_FLOOR);
}

static void draw_score(void) {
    char buf[32];
    /* Clear score area */
    gui_fill_rect(AREA_X + AREA_W - 180, AREA_Y + 10,
                  170, 20, COL_HEADER_BG);

    char score_str[48] = "Score: ";
    itoa_simple(score, buf);
    /* Append score number */
    int i = 7;
    int j = 0;
    while (buf[j]) score_str[i++] = buf[j++];
    score_str[i] = '\0';

    gui_draw_string(AREA_X + AREA_W - 170, AREA_Y + 12,
                    score_str, COL_SCORE_FG, COL_HEADER_BG);
}

static void draw_paddle(void) {
    gui_fill_rect(paddle_x, PADDLE_Y, PADDLE_W, PADDLE_H, COL_PADDLE);
}

static void erase_paddle(void) {
    gui_fill_rect(paddle_x, PADDLE_Y, PADDLE_W, PADDLE_H, COL_AREA_BG);
}

static void draw_box(box_t *b) {
    if (!b->active) return;
    uint32_t color = b->flash > 0 ? COL_BOX_HIT : COL_BOX;
    gui_fill_rect(b->x, b->y, BOX_W, BOX_H, color);
}

static void erase_box(box_t *b) {
    if (!b->active) return;
    gui_fill_rect(b->x, b->y, BOX_W, BOX_H, COL_AREA_BG);
}

/*=============================================================================
 * GAME LOGIC
 *===========================================================================*/

static void spawn_box(void) {
    for (int i = 0; i < MAX_BOXES; i++) {
        if (!boxes[i].active) {
            boxes[i].active = 1;
            boxes[i].hit = 0;
            boxes[i].flash = 0;
            boxes[i].x = rng_range(PLAY_X + 2, PLAY_X + PLAY_W - BOX_W - 2);
            boxes[i].y = PLAY_Y + 2;
            return;
        }
    }
    /* No free slot — skip this spawn */
}

static int rects_overlap(int ax, int ay, int aw, int ah,
                         int bx, int by, int bw, int bh) {
    return (ax < bx + bw) && (ax + aw > bx) &&
           (ay < by + bh) && (ay + ah > by);
}

static void update_boxes(void) {
    for (int i = 0; i < MAX_BOXES; i++) {
        box_t *b = &boxes[i];
        if (!b->active) continue;

        /* Erase at old position */
        erase_box(b);

        /* Flash countdown */
        if (b->flash > 0) {
            b->flash--;
            if (b->flash == 0) {
                b->active = 0;
                continue;
            }
            draw_box(b);
            continue;
        }

        /* Move down */
        b->y += DROP_SPEED;

        /* Check collision with paddle */
        if (rects_overlap(b->x, b->y, BOX_W, BOX_H,
                          paddle_x, PADDLE_Y, PADDLE_W, PADDLE_H)) {
            b->hit = 1;
            b->flash = 4;  /* flash orange for a few frames */
            score--;
            draw_score();
            draw_box(b);
            continue;
        }

        /* Check if reached floor */
        if (b->y + BOX_H >= PLAY_Y + PLAY_H - 2) {
            b->active = 0;
            score++;
            draw_score();
            continue;
        }

        draw_box(b);
    }
}

/*=============================================================================
 * MAIN GAME LOOP
 *===========================================================================*/

void mex_main(void) {
    /* Initialize GUI */
    if (gui_init() != 0) {
        return;  /* No display available */
    }

    /* Compute play area position: centered on actual screen */
    int scr_w = (int)gui_get_screen_width();
    int scr_h = (int)gui_get_screen_height();
    AREA_X  = (scr_w - AREA_W) / 2;
    AREA_Y  = (scr_h - AREA_H) / 2;
    PLAY_X  = AREA_X + BORDER;
    PLAY_Y  = AREA_Y + 40;
    PLAY_H  = AREA_H - 40 - BORDER;
    PADDLE_Y = PLAY_Y + PLAY_H - PADDLE_H - 4;

    /* Seed RNG with our PID (different each launch for variety) */
    rng_seed((unsigned int)libprocess_get_pid() * 7919 + 42);

    /* Initialize state */
    score = 0;
    tick = 0;
    spawn_timer = 0;
    game_over = 0;
    paddle_x = PLAY_X + (PLAY_W - PADDLE_W) / 2;

    for (int i = 0; i < MAX_BOXES; i++) {
        boxes[i].active = 0;
    }

    /* Draw initial scene */
    draw_background();
    draw_score();
    draw_paddle();

    /* === GAME LOOP === */
    key_event_t evt;
    int left_held = 0;
    int right_held = 0;

    while (!game_over) {
        /* --- Input --- */
        while (kbd_read_event(&evt) > 0) {
            if (evt.type == KEY_PRESSED) {
                switch (evt.scancode) {
                    case SC_ESCAPE:
                        game_over = 1;
                        break;
                    case SC_LEFT:
                        left_held = 1;
                        break;
                    case SC_RIGHT:
                        right_held = 1;
                        break;
                }
            } else if (evt.type == KEY_RELEASED) {
                switch (evt.scancode) {
                    case SC_LEFT:
                        left_held = 0;
                        break;
                    case SC_RIGHT:
                        right_held = 0;
                        break;
                }
            }
        }

        if (game_over) break;

        /* --- Move paddle --- */
        if (left_held || right_held) {
            erase_paddle();
            if (left_held)  paddle_x -= PADDLE_SPEED;
            if (right_held) paddle_x += PADDLE_SPEED;

            /* Clamp to play area */
            if (paddle_x < PLAY_X + 2)
                paddle_x = PLAY_X + 2;
            if (paddle_x + PADDLE_W > PLAY_X + PLAY_W - 2)
                paddle_x = PLAY_X + PLAY_W - PADDLE_W - 2;

            draw_paddle();
        }

        /* --- Spawn boxes --- */
        spawn_timer++;
        if (spawn_timer >= SPAWN_INTERVAL) {
            spawn_timer = 0;
            spawn_box();
        }

        /* --- Update boxes --- */
        update_boxes();

        /* --- Frame delay --- */
        tick++;
        libprocess_sleep(2);  /* ~2 PIT ticks ≈ ~20ms per frame */
    }

    /* Returning from mex_main causes mex_entry.s to issue exit syscall */
}
