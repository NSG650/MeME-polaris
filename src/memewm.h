#ifndef __MEMEWM_H__
#define __MEMEWM_H__

#include <stdint.h>
#include <stddef.h>

typedef struct {
    int id;
    int rel_x;
    int rel_y;
    int titlebar;
    int top_border;
    int bottom_border;
    int right_border;
    int left_border;
} window_click_data_t;

int memewm_init(uint32_t *, int, int, int, uint8_t *, int, int);

void memewm_window_plot_px(int, int, uint32_t, int);
void memwm_make_window_toggle_drawable(int);
int memewm_window_create(char *, size_t, size_t, size_t, size_t);
void memewm_window_focus(int);
void memewm_window_move(int, int, int);
int memewm_window_resize(int, int, int);
window_click_data_t memewm_window_click(int, int);
void memewm_set_cursor_pos(int, int);
void memewm_set_cursor_pos_abs(int, int);
void memewm_get_cursor_pos(int *, int *);
void memewm_refresh(void);

#endif
