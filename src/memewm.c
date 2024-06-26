#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "memewm.h"
#include "memewm_glue.h"

typedef struct window_t {
    int id;
    char *title;
    int x;
    int y;
    int x_size;
    int y_size;
    bool is_drawable;
    uint32_t *framebuffer;
    struct window_t *next;
} window_t;

typedef struct {
    int64_t bitmap[16 * 16];
} cursor_t;

#define X 0x00ffffff
#define B 0x00000000
#define o (-1)

static cursor_t cursor = {
    {
        X, X, X, X, X, X, X, X, X, X, X, X, o, o, o, o,
        X, B, B, B, B, B, B, B, B, B, X, o, o, o, o, o,
        X, B, B, B, B, B, B, B, B, X, o, o, o, o, o, o,
        X, B, B, B, B, B, B, B, X, o, o, o, o, o, o, o,
        X, B, B, B, B, B, B, X, o, o, o, o, o, o, o, o,
        X, B, B, B, B, B, B, B, X, o, o, o, o, o, o, o,
        X, B, B, B, B, B, B, B, B, X, o, o, o, o, o, o,
        X, B, B, B, X, B, B, B, B, B, X, o, o, o, o, o,
        X, B, B, X, o, X, B, B, B, B, B, X, o, o, o, o,
        X, B, X, o, o, o, X, B, B, B, B, B, X, o, o, o,
        X, X, o, o, o, o, o, X, B, B, B, B, B, X, o, o,
        X, o, o, o, o, o, o, o, X, B, B, B, B, B, X, o,
        o, o, o, o, o, o, o, o, o, X, B, B, B, B, B, X,
        o, o, o, o, o, o, o, o, o, o, X, B, B, B, X, o,
        o, o, o, o, o, o, o, o, o, o, o, X, B, X, o, o,
        o, o, o, o, o, o, o, o, o, o, o, o, X, o, o, o,
    }
};

#undef X
#undef B
#undef o

static int TITLE_BAR_THICKNESS = 18;
static uint32_t BACKGROUND_COLOUR = 0x00008080;
static uint32_t WINDOW_BORDERS = 0x00ffffff;
static uint32_t TITLE_BAR_BACKG = 0x00003377;
static uint32_t TITLE_BAR_FOREG = 0x00ffffff;

static int memewm_needs_refresh = 0;
static int memewm_current_window = -1;

static uint32_t *memewm_framebuffer;
static int memewm_screen_width;
static int memewm_screen_height;
static int memewm_screen_pitch;

static uint8_t *memewm_font_bitmap;
static int memewm_font_width;
static int memewm_font_height;

static size_t memewm_fb_size;

static int memewm_mouse_x = 0;
static int memewm_mouse_y = 0;

static int last_mouse_x = 0;
static int last_mouse_y = 0;

static uint32_t *antibuffer;
static uint32_t *prevbuffer;

static window_t *windows = 0;

static size_t memewm_strlen(const char *str) {
    size_t len;

    for (len = 0; str[len]; len++);

    return len;
}

static char *memewm_strcpy(char *dest, const char *src) {
    size_t i;

    for (i = 0; src[i]; i++)
        dest[i] = src[i];
    dest[i] = 0;

    return dest;
}

static void *memewm_alloc(size_t size) {
    uint8_t *ptr = memewm_malloc(size);

    if (!ptr)
        return (void *)0;

    for (size_t i = 0; i < size; i++)
        ptr[i] = 0;

    return (void *)ptr;
}

static void plot_px(int x, int y, uint32_t hex) {
    if (x >= memewm_screen_width || y >= memewm_screen_height || x < 0 || y < 0)
        return;

    size_t fb_i = x + (memewm_screen_pitch / sizeof(uint32_t)) * y;

    antibuffer[fb_i] = hex;

    return;
}

static void plot_px_direct(int x, int y, uint32_t hex) {
    if (x >= memewm_screen_width || y >= memewm_screen_height || x < 0 || y < 0)
        return;

    size_t fb_i = x + (memewm_screen_pitch / sizeof(uint32_t)) * y;

    memewm_framebuffer[fb_i] = hex;

    return;
}

static uint32_t get_px(int x, int y) {
    if (x >= memewm_screen_width || y >= memewm_screen_height || x < 0 || y < 0)
        return 0;

    size_t fb_i = x + (memewm_screen_pitch / sizeof(uint32_t)) * y;

    return antibuffer[fb_i];
}

static void plot_char(char c, int x, int y, uint32_t hex_fg, uint32_t hex_bg) {
    int orig_x = x;

    for (int i = 0; i < memewm_font_height; i++) {
        for (int j = 0; j < memewm_font_width; j++) {
            if ((memewm_font_bitmap[c * memewm_font_height + i] >> ((memewm_font_width - 1) - j)) & 1)
                plot_px(x++, y, hex_fg);
            else
                plot_px(x++, y, hex_bg);
        }
        y++;
        x = orig_x;
    }

    return;
}

static void memewm_update_cursor(void) {
    for (size_t x = 0; x < 16; x++) {
        for (size_t y = 0; y < 16; y++) {
            if (cursor.bitmap[x * 16 + y] != -1) {
                uint32_t px = get_px(last_mouse_x + x, last_mouse_y + y);
                plot_px_direct(last_mouse_x + x, last_mouse_y + y, px);
            }
        }
    }
    for (size_t x = 0; x < 16; x++) {
        for (size_t y = 0; y < 16; y++) {
            if (cursor.bitmap[x * 16 + y] != -1) {
                plot_px_direct(memewm_mouse_x + x, memewm_mouse_y + y, cursor.bitmap[x * 16 + y]);
            }
        }
    }
    last_mouse_x = memewm_mouse_x;
    last_mouse_y = memewm_mouse_y;
    return;
}

static window_t *get_window_ptr(int id) {
    if (!windows) {
        return (window_t *)0;
    } else {
        window_t *wptr = windows;
        for (;;) {
            if (wptr->id == id) {
                return wptr;
            }
            if (!wptr->next) {
                /* not found */
                return (window_t *)0;
            } else {
                wptr = wptr->next;
            }
        }
    }
}

/* creates a new window with a title, size */
/* returns window id */
int memewm_window_create(char *title, size_t x, size_t y, size_t x_size, size_t y_size) {
    window_t *wptr;
    int id = 0;

    uint32_t *fb = memewm_alloc(x_size * y_size * sizeof(uint32_t));
    if (!fb)
        return -1;

    char *wtitle = memewm_alloc(memewm_strlen(title) + 1);
    if (!title)
        return -1;

    /* check if no windows were allocated */
    if (!windows) {
        /* allocate root window */
        windows = memewm_alloc(sizeof(window_t));
        if (!windows)
            return -1;
        wptr = windows;
    } else {
        /* else crawl the linked list to the last entry */
        wptr = windows;
        for (;;) {
            if (wptr->id == id)
                id++;
            if (wptr->next) {
                wptr = wptr->next;
                continue;
            } else {
                wptr->next = memewm_alloc(sizeof(window_t));
                if (!wptr->next)
                    return -1;
                wptr = wptr->next;
                break;
            }
        }
    }

    wptr->id = id;
    memewm_strcpy(wtitle, title);
    wptr->title = wtitle;
    wptr->x = x;
    wptr->y = y;
    wptr->x_size = x_size;
    wptr->y_size = y_size;
    wptr->framebuffer = fb;
    wptr->next = 0;

    memewm_current_window = id;

    memewm_needs_refresh = 1;

    return id;
}

void memewm_window_focus(int window) {
    /* moves the requested window to the foreground */
    window_t *last_wptr;
    window_t *req_wptr = get_window_ptr(window);
    window_t *prev_wptr;

    if (!windows)
        return;

    if (!req_wptr)
        return;

    window_t *next_wptr = req_wptr->next;

    if (!(req_wptr == windows))
        for (prev_wptr = windows; prev_wptr->next != req_wptr; prev_wptr = prev_wptr->next);
    else
        prev_wptr = 0;

    for (last_wptr = windows; last_wptr->next; last_wptr = last_wptr->next);

    if (last_wptr == req_wptr)
        return;

    /* all necessary variables acquired */

    /* the prev should point to the next */
    if (prev_wptr)
        prev_wptr->next = next_wptr;
    else
        windows = next_wptr;
    /* the requested one should point to NULL */
    req_wptr->next = 0;
    /* the last should point to the requested one */
    last_wptr->next = req_wptr;

    memewm_current_window = window;

    memewm_needs_refresh = 1;

    return;
}

void memewm_window_move(int x, int y, int window) {
    window_t *wptr = get_window_ptr(window);

    wptr->x += x;
    wptr->y += y;

    memewm_needs_refresh = 1;

    return;
}

static void quick_plot_px(int x, int y, int x_size, int y_size, uint32_t *fb, uint32_t hex) {
    if (x >= x_size || y >= y_size || x < 0 || y < 0)
        return;

    size_t fb_i = x + x_size * y;

    fb[fb_i] = hex;

    return;
}

static uint32_t quick_get_px(int x, int y, int x_size, int y_size, uint32_t *fb) {
    if (x >= x_size || y >= y_size || x < 0 || y < 0)
        return 0;

    size_t fb_i = x + x_size * y;

    return fb[fb_i];
}

int memewm_window_resize(int x_size, int y_size, int window) {
    window_t *wptr = get_window_ptr(window);

    int new_x_size;
    int new_y_size;

    if (wptr->x_size + x_size < 1) {
        new_x_size = 1;
    } else {
        new_x_size = wptr->x_size + x_size;
    }
    if (wptr->y_size + y_size < 1) {
        new_y_size = 1;
    } else {
        new_y_size = wptr->y_size + y_size;
    }

    uint32_t *fb = memewm_alloc(new_x_size * new_y_size * sizeof(uint32_t));
    if (!fb)
        return -1;

    uint32_t *old_fb = wptr->framebuffer;
    wptr->framebuffer = fb;

    int old_x_size = wptr->x_size;
    int old_y_size = wptr->y_size;

    wptr->x_size = new_x_size;
    wptr->y_size = new_y_size;

    for (size_t y = 0; y < old_y_size; y++) {
        for (size_t x = 0; x < old_x_size; x++) {
            quick_plot_px(x, y, new_x_size, new_y_size, fb,
                quick_get_px(x, y, old_x_size, old_y_size, old_fb));
        }
    }

    memewm_free(old_fb);

    memewm_needs_refresh = 1;

    return 0;
}

int memewm_init(uint32_t *fb, int scrn_width, int scrn_height, int scrn_pitch,
                uint8_t *fnt, int fnt_width, int fnt_height) {
    memewm_framebuffer = fb;
    memewm_screen_width = scrn_width;
    memewm_screen_height = scrn_height;
    memewm_screen_pitch = scrn_pitch;
    memewm_font_bitmap = fnt;
    memewm_font_width = fnt_width;
    memewm_font_height = fnt_height;

    memewm_mouse_x = memewm_screen_width / 2;
    memewm_mouse_y = memewm_screen_height / 2;

    memewm_fb_size = (memewm_screen_pitch / sizeof(uint32_t)) * memewm_screen_height * sizeof(uint32_t);

    antibuffer = memewm_alloc(memewm_fb_size);

    if (!antibuffer)
        return -1;

    prevbuffer = memewm_alloc(memewm_fb_size);

    if (!prevbuffer) {
        memewm_free(antibuffer);
        return -1;
    }

    memewm_needs_refresh = 1;
    memewm_refresh();

    return 0;
}

void memewm_refresh(void) {
    if (!memewm_needs_refresh)
        return;

    memewm_needs_refresh = 0;

    uint32_t *tmpbufptr = prevbuffer;
    prevbuffer = antibuffer;
    antibuffer = tmpbufptr;

    /* draw background */
    for (size_t i = 0; i < memewm_fb_size / sizeof(uint32_t); i++)
        antibuffer[i] = BACKGROUND_COLOUR;

    /* draw every window */
    window_t *wptr = windows;
    for (;;) {
        if (!wptr)
            break;

        /* draw the title bar */
        for (size_t x = 0; x < TITLE_BAR_THICKNESS; x++)
            for (size_t i = 0; i < wptr->x_size + 2; i++)
                plot_px(wptr->x + i, wptr->y + x, TITLE_BAR_BACKG);

        /* draw the title */
        for (size_t i = 0; wptr->title[i]; i++) {
            if ((wptr->x + memewm_font_width + (i + 1) * memewm_font_width) >= wptr->x + wptr->x_size)
                break;
            plot_char(wptr->title[i], wptr->x + memewm_font_width + i * memewm_font_width, wptr->y + 1,
                TITLE_BAR_FOREG, TITLE_BAR_BACKG);
        }

        /* draw the window border */
        for (size_t i = 0; i < wptr->x_size + 2; i++)
            plot_px(wptr->x + i, wptr->y, WINDOW_BORDERS);
        for (size_t i = 0; i < wptr->x_size + 2; i++)
            plot_px(wptr->x + i, wptr->y + TITLE_BAR_THICKNESS + wptr->y_size, WINDOW_BORDERS);
        for (size_t i = 0; i < wptr->y_size + TITLE_BAR_THICKNESS + 1; i++)
            plot_px(wptr->x, wptr->y + i, WINDOW_BORDERS);
        for (size_t i = 0; i < wptr->y_size + TITLE_BAR_THICKNESS + 1; i++)
            plot_px(wptr->x + wptr->x_size + 1, wptr->y + i, WINDOW_BORDERS);

        /* paint the framebuffer */
        size_t in_x = 1;
        size_t in_y = TITLE_BAR_THICKNESS;
        for (size_t i = 0; i < wptr->x_size * wptr->y_size; i++) {
            plot_px(in_x++ + wptr->x, in_y + wptr->y, wptr->framebuffer[i]);
            if (in_x - 1 == wptr->x_size) {
                in_y++;
                in_x = 1;
            }
        }

        wptr = wptr->next;
    }

    /* copy over the buffer */
    for (size_t i = 0; i < memewm_fb_size / sizeof(uint32_t); i++) {
        if (antibuffer[i] != prevbuffer[i])
            memewm_framebuffer[i] = antibuffer[i];
    }

    memewm_update_cursor();

    return;
}

void memewm_set_cursor_pos(int x, int y) {
    if (memewm_mouse_x + x < 0) {
        memewm_mouse_x = 0;
    } else if (memewm_mouse_x + x >= memewm_screen_width) {
        memewm_mouse_x = memewm_screen_width - 1;
    } else {
        memewm_mouse_x += x;
    }

    if (memewm_mouse_y + y < 0) {
        memewm_mouse_y = 0;
    } else if (memewm_mouse_y + y >= memewm_screen_height) {
        memewm_mouse_y = memewm_screen_height - 1;
    } else {
        memewm_mouse_y += y;
    }

    memewm_update_cursor();

    return;
}

void memewm_set_cursor_pos_abs(int x, int y) {
    if (x < 0) {
        memewm_mouse_x = 0;
    } else if (x >= memewm_screen_width) {
        memewm_mouse_x = memewm_screen_width - 1;
    } else {
        memewm_mouse_x = x;
    }

    if (y < 0) {
        memewm_mouse_y = 0;
    } else if (y >= memewm_screen_height) {
        memewm_mouse_y = memewm_screen_height - 1;
    } else {
        memewm_mouse_y = y;
    }

    memewm_update_cursor();

    return;
}

void memewm_get_cursor_pos(int *x, int *y) {
    *x = memewm_mouse_x;
    *y = memewm_mouse_y;

    return;
}

window_click_data_t memewm_window_click(int x, int y) {
    window_click_data_t ret = {0};
    window_t *wptr = windows;

    if (!wptr)
        goto fail;

    /* get number of nodes */
    size_t nodes;
    for (nodes = 0; wptr->next; nodes++, wptr = wptr->next);

    for (;; nodes--) {
        wptr = windows;
        for (size_t i = 0; i < nodes; i++)
            wptr = wptr->next;

        if (x >= wptr->x && x < wptr->x + wptr->x_size + 2 &&
            y >= wptr->y && y < wptr->y + wptr->y_size + 1 + TITLE_BAR_THICKNESS) {
            int in_canvas = 1;
            ret.id = wptr->id;
            ret.rel_x = ret.rel_y = -1;
            if (y - wptr->y < TITLE_BAR_THICKNESS
                && y - wptr->y > 0 && y - wptr->y < wptr->y_size + TITLE_BAR_THICKNESS
                && x - wptr->x > 0 && x - wptr->x < wptr->x_size) {
                in_canvas = 0;
                ret.titlebar = 1;
            } else {
                ret.titlebar = 0;
            }
            if (y - wptr->y == 0) {
                in_canvas = 0;
                ret.top_border = 1;
                ret.bottom_border = 0;
            } else if (y - wptr->y == wptr->y_size + TITLE_BAR_THICKNESS) {
                in_canvas = 0;
                ret.bottom_border = 1;
                ret.top_border = 0;
            }
            if (x - wptr->x == 0) {
                in_canvas = 0;
                ret.left_border = 1;
                ret.right_border = 0;
            } else if (x - wptr->x == wptr->x_size + 1) {
                in_canvas = 0;
                ret.right_border = 1;
                ret.left_border = 0;
            }
            if (in_canvas) {
                ret.rel_x = x - (wptr->x + 1);
                ret.rel_y = y - (wptr->y + TITLE_BAR_THICKNESS);
            }
            return ret;
        }

        if (!nodes)
            break;
    }

fail:
    ret.id = -1;
    return ret;
}

void memewm_window_plot_px(int x, int y, uint32_t hex, int window) {
    window_t *wptr = get_window_ptr(window);

    if (!wptr)
        return;

    if (!wptr->is_drawable)
        return;

    if (x >= wptr->x_size || y >= wptr->y_size || x < 0 || y < 0)
        return;

    size_t fb_i = x + wptr->x_size * y;
    wptr->framebuffer[fb_i] = hex;
    memewm_needs_refresh = 1;
    return;
}

void memwm_make_window_toggle_drawable(int window) {
    window_t *wptr = get_window_ptr(window);

    if (!wptr)
        return;

    wptr->is_drawable = !wptr->is_drawable;
}
