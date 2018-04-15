#include <stdint.h>
#include <stddef.h>
#include "memewm.h"
#include "memewm_glue.h"

typedef struct window_t {
    int id;
    char title[2048];
    int x;
    int y;
    int x_size;
    int y_size;
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

int memewm_needs_refresh = 0;
int memewm_current_window = -1;

int memewm_mouse_x = 0;
int memewm_mouse_y = 0;

static uint32_t *antibuffer;
static uint32_t *prevbuffer;

static window_t *windows = 0;

static void plot_px(int x, int y, uint32_t hex) {
    if (x > memewm_screen_width || y > memewm_screen_height || x < 0 || y < 0)
        return;

    size_t fb_i = x + (memewm_screen_pitch / sizeof(uint32_t)) * y;

    antibuffer[fb_i] = hex;

    return;
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

static void put_mouse_cursor(void) {
    for (size_t x = 0; x < 16; x++) {
        for (size_t y = 0; y < 16; y++) {
            if (cursor.bitmap[x * 16 + y] != -1)
                plot_px(memewm_mouse_x + x, memewm_mouse_y + y, cursor.bitmap[x * 16 + y]);
        }
    }
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

    /* check if no windows were allocated */
    if (!windows) {
        /* allocate root window */
        windows = memewm_malloc(sizeof(window_t));
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
                wptr->next = memewm_malloc(sizeof(window_t));
                wptr = wptr->next;
                break;
            }
        }
    }

    wptr->id = id;
    memewm_strcpy(wptr->title, title);
    wptr->x = x;
    wptr->y = y;
    wptr->x_size = x_size;
    wptr->y_size = y_size;
    wptr->framebuffer = memewm_malloc(x_size * y_size * sizeof(uint32_t));
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

void memewm_window_resize(int x_size, int y_size, int window) {
    window_t *wptr = get_window_ptr(window);

    wptr->x_size += x_size;
    wptr->y_size += y_size;

    memewm_free(wptr->framebuffer);
    wptr->framebuffer = memewm_malloc(wptr->x_size * wptr->y_size * sizeof(uint32_t));

    memewm_needs_refresh = 1;

    return;
}

void memewm_init(void) {
    antibuffer = memewm_malloc(memewm_screen_width * memewm_screen_height * sizeof(uint32_t));
    prevbuffer = memewm_malloc(memewm_screen_width * memewm_screen_height * sizeof(uint32_t));

    return;
}

void memewm_refresh(void) {
    uint32_t *tmpbufptr = prevbuffer;
    prevbuffer = antibuffer;
    antibuffer = tmpbufptr;

    /* clear screen */
    for (size_t i = 0; i < memewm_screen_width * memewm_screen_height; i++)
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
        for (size_t i = 0; wptr->title[i]; i++)
            plot_char(wptr->title[i], wptr->x + 8 + i * 8, wptr->y + 1,
                TITLE_BAR_FOREG, TITLE_BAR_BACKG);

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

    put_mouse_cursor();

    /* copy over the buffer */
    for (size_t i = 0; i < memewm_screen_width * memewm_screen_height; i++) {
        if (antibuffer[i] != prevbuffer[i])
            memewm_framebuffer[i] = antibuffer[i];
    }

    return;
}

window_click_data_t memewm_window_click(int x, int y) {
    window_click_data_t ret = {0};
    window_t *wptr = windows;

    if (!wptr)
        goto fail;

    /* get number of nodes */
    size_t nodes;
    for (nodes = 1; wptr->next; nodes++, wptr = wptr->next);

    for (;; nodes--) {
        wptr = windows;
        for (size_t i = 0; i < nodes; i++)
            wptr = wptr->next;

        if (x >= wptr->x && x < wptr->x + wptr->x_size + 2 &&
            y >= wptr->y && y < wptr->y + wptr->y_size + 1 + TITLE_BAR_THICKNESS) {
            int titlebar = 0;
            int top_border = 0;
            int bottom_border = 0;
            int right_border = 0;
            int left_border = 0;
            if (y - wptr->y < TITLE_BAR_THICKNESS
                && y - wptr->y > 0 && y - wptr->y < wptr->y_size + TITLE_BAR_THICKNESS
                && x - wptr->x > 0 && x - wptr->x < wptr->x_size)
                titlebar = 1;
            if (y - wptr->y == 0)
                top_border = 1;
            else if (y - wptr->y == wptr->y_size + TITLE_BAR_THICKNESS)
                bottom_border = 1;
            if (x - wptr->x == 0)
                left_border = 1;
            else if (x - wptr->x == wptr->x_size + 1)
                right_border = 1;
            ret.id = wptr->id;
            ret.rel_x = x - (wptr->x - 1);
            ret.rel_y = y - (wptr->y - TITLE_BAR_THICKNESS);
            ret.titlebar = titlebar;
            ret.top_border = top_border;
            ret.bottom_border = bottom_border;
            ret.right_border = right_border;
            ret.left_border = left_border;
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
    size_t fb_i = x + wptr->x_size * y;
    wptr->framebuffer[fb_i] = hex;
    memewm_needs_refresh = 1;
    return;
}
