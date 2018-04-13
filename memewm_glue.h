#ifndef __MEMEWM_GLUE_H__
#define __MEMEWM_GLUE_H__

#include <stdint.h>
#include <stddef.h>

extern uint32_t *memewm_framebuffer;
extern int memewm_screen_width;
extern int memewm_screen_height;

extern uint8_t *memewm_font_bitmap;
extern int memewm_font_width;
extern int memewm_font_height;

void *memewm_malloc(size_t);
void memewm_free(void *);

char *memewm_strcpy(char *, const char *);

#endif
