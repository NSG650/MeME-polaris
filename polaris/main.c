#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>

#include <errno.h>
#include <time.h>

#include <linux/fb.h>

#if defined (__x86_64__)
#include <cpuid.h>
#endif


#include "../src/memewm.h"

uint8_t font[];

void *memewm_malloc(size_t size) {
	return malloc(size);
}

void memewm_free(void *addr) {
	free(addr);
}

struct mouse_packet {
	uint8_t flags;
	int32_t x_mov;
	int32_t y_mov;
};

static void window_putc(char c, int x, int y, int window_handle) {
	x *= 8;
	y *= 16;
	uint8_t line;
	for (uint32_t x_bit = 0; x_bit < 16; x_bit++) {
		line = font[c * 16 + x_bit];
		for (uint32_t y_bit = 0; y_bit <= 8; y_bit++) {
			if (line & (1 << (8 - y_bit - 1))) {
				memewm_window_plot_px(x + y_bit, y + x_bit, 0xFFFFFF, window_handle);
			} else if (c == ' ') {
				memewm_window_plot_px(x + y_bit, y + x_bit, 0x000000, window_handle);
			}
		}
	}
}

static void window_puts(char *str, int x, int y, int window_handle) {
	while (*str) {
		window_putc(*str++, x++, y, window_handle);
	}
}

int main(void) {
	printf("MEME :^)\n");
	struct mouse_packet mouse_pack = {0};
	struct fb_fix_screeninfo fix = {0};
	struct fb_var_screeninfo var = {0};

	int mouse_fd = open("/dev/mouse", O_RDONLY);
	if (mouse_fd == -1) {
		printf("[!] Failed to find mouse device\n");
		return -1;
	}

	int framebuffer_fd = open("/dev/fbdev", O_RDWR);

	if (framebuffer_fd < 0) {
		printf("[!] Failed to find framebuffer\n");
		return -1;
	}

	if (ioctl(framebuffer_fd, FBIOGET_FSCREENINFO, &fix) < 0) {
		printf("[!] Failed to query framebuffer\n");
		return -1;
	}

	if (ioctl(framebuffer_fd, FBIOGET_VSCREENINFO, &var) < 0) {
		printf("[!] Failed to query framebuffer\n");
		return -1;
	}

	uint32_t *fb = mmap(0,
						fix.line_length * var.yres,
					 PROT_READ | PROT_WRITE, MAP_SHARED, framebuffer_fd, 0);

	memewm_init(fb, var.xres, var.yres,
				fix.line_length, font, 8, 16);

	struct utsname uname_buffer = {0};

	char buffer[1024] = {0};
	uname(&uname_buffer);

	snprintf(buffer, 128, "%s on %s!", uname_buffer.sysname, uname_buffer.machine);

	int sysinfo_window_handle = memewm_window_create(buffer, 15, 20, 640, 480);
	memset(buffer, 0, 1024);

#if defined (__x86_64__)
	char cpu_brand_string[64] = {0};
	__get_cpuid(0x80000002, (uint32_t *)(cpu_brand_string +  0), (uint32_t *)(cpu_brand_string +  4), (uint32_t *)(cpu_brand_string +  8), (uint32_t *)(cpu_brand_string + 12));
	__get_cpuid(0x80000003, (uint32_t *)(cpu_brand_string + 16), (uint32_t *)(cpu_brand_string + 20), (uint32_t *)(cpu_brand_string + 24), (uint32_t *)(cpu_brand_string + 28));
	__get_cpuid(0x80000004, (uint32_t *)(cpu_brand_string + 32), (uint32_t *)(cpu_brand_string + 36), (uint32_t *)(cpu_brand_string + 40), (uint32_t *)(cpu_brand_string + 44));
	const char *p = cpu_brand_string;
	while (*p == ' ')
	{
		++p;
	}
#endif

	snprintf(buffer, 128, "%s running on %s!", uname_buffer.sysname, p);
	window_puts(buffer, 1, 0, sysinfo_window_handle);
	memwm_make_window_toggle_drawable(sysinfo_window_handle);

	memewm_window_create("Chalkboard", 30, 30, 800, 400);
	memewm_refresh();
	for (;;) {
		if (read(mouse_fd, &mouse_pack, sizeof(struct mouse_packet)) > 0) {
			int last_x = 0, last_y = 0, new_x = 0, new_y = 0;
			int64_t x_mov = 0, y_mov = 0;
			if (mouse_pack.flags & (1 << 4)) {
				x_mov = (int8_t)mouse_pack.x_mov;
			} else
				x_mov = mouse_pack.x_mov;

			if (mouse_pack.flags & (1 << 5)) {
				y_mov = (int8_t)mouse_pack.y_mov;
			} else
				y_mov = mouse_pack.y_mov;

			memewm_get_cursor_pos(&last_x, &last_y);
			window_click_data_t last_click_data =
			memewm_window_click(last_x, last_y);
			memewm_set_cursor_pos(x_mov, -y_mov);
			// there was a click!!!
			if ((mouse_pack.flags & (1 << 0))) {
				memewm_get_cursor_pos(&new_x, &new_y);

				int id = last_click_data.id;

				if (last_click_data.top_border) {
					memewm_window_focus(id);
					memewm_window_resize(0, -(new_y - last_y), id);
					memewm_window_move(0, new_y - last_y, id);
				}

				if (last_click_data.bottom_border) {
					memewm_window_focus(id);
					memewm_window_resize(0, new_y - last_y, id);
				}

				if (last_click_data.left_border) {
					memewm_window_focus(id);
					memewm_window_resize(-(new_x - last_x), 0, id);
					memewm_window_move(new_x - last_x, 0, id);
				}

				if (last_click_data.right_border) {
					memewm_window_focus(id);
					memewm_window_resize(new_x - last_x, 0, id);
				}

				if (last_click_data.titlebar) {
					memewm_window_focus(id);
					memewm_window_move(new_x - last_x, new_y - last_y, id);
				}

				if (last_click_data.rel_x != -1 &&
					last_click_data.rel_y != -1) {
					memewm_window_focus(id);
					for (int i = 0; i < 10; i++) {
						for (int j = 0; j < 10; j++) {
							memewm_window_plot_px(last_click_data.rel_x + i,
									  		last_click_data.rel_y + j, 0xffffff, id);
						}
					}
					}
					memewm_refresh();
			}
		}
	}

	return 0;
}

uint8_t font[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00,
	0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x12, 0x12, 0x12, 0x7e, 0x24, 0x24, 0x7e, 0x48, 0x48, 0x48, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x3e, 0x49, 0x48, 0x38, 0x0e, 0x09, 0x49,
	0x3e, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x31, 0x4a, 0x4a, 0x34,
	0x08, 0x08, 0x16, 0x29, 0x29, 0x46, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x1c, 0x22, 0x22, 0x14, 0x18, 0x29, 0x45, 0x42, 0x46, 0x39, 0x00, 0x00,
	0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x08, 0x08, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x08, 0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x20,
	0x10, 0x10, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x10, 0x10, 0x20, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x49, 0x2a, 0x1c, 0x2a, 0x49,
	0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08,
	0x08, 0x7f, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x08, 0x08, 0x10,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0x02, 0x04, 0x08, 0x08, 0x10, 0x10, 0x20, 0x40, 0x40, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x18, 0x24, 0x42, 0x46, 0x4a, 0x52, 0x62, 0x42,
	0x24, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x18, 0x28, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x3c, 0x42, 0x42, 0x02, 0x0c, 0x10, 0x20, 0x40, 0x40, 0x7e, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x3c, 0x42, 0x42, 0x02, 0x1c, 0x02, 0x02, 0x42,
	0x42, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x0c, 0x14, 0x24,
	0x44, 0x44, 0x7e, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x7e, 0x40, 0x40, 0x40, 0x7c, 0x02, 0x02, 0x02, 0x42, 0x3c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x1c, 0x20, 0x40, 0x40, 0x7c, 0x42, 0x42, 0x42,
	0x42, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x02, 0x02, 0x04,
	0x04, 0x04, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x3c, 0x42, 0x42, 0x42, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x3e, 0x02, 0x02, 0x02,
	0x04, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18,
	0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x18, 0x08, 0x08, 0x10, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x04, 0x08, 0x10, 0x20, 0x10, 0x08,
	0x04, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e,
	0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x40, 0x20, 0x10, 0x08, 0x04, 0x08, 0x10, 0x20, 0x40, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x3c, 0x42, 0x42, 0x02, 0x04, 0x08, 0x08, 0x00,
	0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x22, 0x4a, 0x56,
	0x52, 0x52, 0x52, 0x4e, 0x20, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x18, 0x24, 0x24, 0x42, 0x42, 0x7e, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x7c, 0x42, 0x42, 0x42, 0x7c, 0x42, 0x42, 0x42,
	0x42, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x42, 0x42, 0x40,
	0x40, 0x40, 0x40, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x78, 0x44, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x44, 0x78, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x7e, 0x40, 0x40, 0x40, 0x7c, 0x40, 0x40, 0x40,
	0x40, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x40, 0x40, 0x40,
	0x7c, 0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x3c, 0x42, 0x42, 0x40, 0x40, 0x4e, 0x42, 0x42, 0x46, 0x3a, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x42, 0x42, 0x42, 0x42, 0x7e, 0x42, 0x42, 0x42,
	0x42, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x44, 0x44, 0x38, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x42, 0x44, 0x48, 0x50, 0x60, 0x60, 0x50, 0x48,
	0x44, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x40, 0x40, 0x40, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x42, 0x42, 0x66, 0x66, 0x5a, 0x5a, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x42, 0x62, 0x62, 0x52, 0x52, 0x4a, 0x4a, 0x46,
	0x46, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x42, 0x42, 0x42,
	0x42, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x7c, 0x42, 0x42, 0x42, 0x7c, 0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x5a,
	0x66, 0x3c, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x42, 0x42, 0x42,
	0x7c, 0x48, 0x44, 0x44, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x3c, 0x42, 0x42, 0x40, 0x30, 0x0c, 0x02, 0x42, 0x42, 0x3c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x7f, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x42, 0x42, 0x42,
	0x42, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x41, 0x41, 0x41, 0x22, 0x22, 0x22, 0x14, 0x14, 0x08, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x42, 0x42, 0x42, 0x42, 0x5a, 0x5a, 0x66, 0x66,
	0x42, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x42, 0x24, 0x24,
	0x18, 0x18, 0x24, 0x24, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x41, 0x41, 0x22, 0x22, 0x14, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x7e, 0x02, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40,
	0x40, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x40, 0x40, 0x20, 0x10, 0x10, 0x08, 0x08, 0x04, 0x02, 0x02, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x70, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x70, 0x00, 0x00, 0x00, 0x18, 0x24, 0x42, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x00,
	0x00, 0x20, 0x10, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x42,
	0x02, 0x3e, 0x42, 0x42, 0x46, 0x3a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
	0x40, 0x40, 0x5c, 0x62, 0x42, 0x42, 0x42, 0x42, 0x62, 0x5c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x42, 0x40, 0x40, 0x40, 0x40,
	0x42, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x3a, 0x46,
	0x42, 0x42, 0x42, 0x42, 0x46, 0x3a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x3c, 0x42, 0x42, 0x7e, 0x40, 0x40, 0x42, 0x3c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x0c, 0x10, 0x10, 0x10, 0x7c, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x3a, 0x44,
	0x44, 0x44, 0x38, 0x20, 0x3c, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x40,
	0x40, 0x40, 0x5c, 0x62, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x08, 0x00, 0x18, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x00, 0x0c, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x48, 0x30, 0x00, 0x00, 0x00, 0x40,
	0x40, 0x40, 0x44, 0x48, 0x50, 0x60, 0x50, 0x48, 0x44, 0x42, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x18, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0x49,
	0x49, 0x49, 0x49, 0x49, 0x49, 0x49, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x5c, 0x62, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x42,
	0x42, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5c, 0x62,
	0x42, 0x42, 0x42, 0x42, 0x62, 0x5c, 0x40, 0x40, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x3a, 0x46, 0x42, 0x42, 0x42, 0x42, 0x46, 0x3a, 0x02, 0x02,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5c, 0x62, 0x42, 0x40, 0x40, 0x40,
	0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x42,
	0x40, 0x30, 0x0c, 0x02, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x10, 0x10, 0x10, 0x7c, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
	0x46, 0x3a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x42,
	0x42, 0x24, 0x24, 0x24, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x41, 0x49, 0x49, 0x49, 0x49, 0x49, 0x49, 0x36, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x42, 0x24, 0x18, 0x18, 0x24,
	0x42, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x42,
	0x42, 0x42, 0x42, 0x26, 0x1a, 0x02, 0x02, 0x3c, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x7e, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x7e, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x0c, 0x10, 0x10, 0x08, 0x08, 0x10, 0x20, 0x10, 0x08,
	0x08, 0x10, 0x10, 0x0c, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x30,
	0x08, 0x08, 0x10, 0x10, 0x08, 0x04, 0x08, 0x10, 0x10, 0x08, 0x08, 0x30,
	0x00, 0x00, 0x00, 0x31, 0x49, 0x46, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xaa, 0x00, 0x80, 0x00, 0x80, 0x73, 0xca, 0x4b,
	0xca, 0x73, 0x80, 0x00, 0x80, 0x00, 0x80, 0x55, 0xaa, 0x00, 0x80, 0x00,
	0x80, 0x71, 0xca, 0x73, 0xc2, 0x42, 0x80, 0x00, 0x80, 0x00, 0x80, 0x55,
	0xaa, 0x00, 0x80, 0x00, 0x80, 0x49, 0xca, 0x7a, 0xca, 0x49, 0x80, 0x00,
	0x80, 0x00, 0x80, 0x55, 0xaa, 0x00, 0x80, 0x00, 0x80, 0x73, 0xca, 0x73,
	0xca, 0x72, 0x80, 0x00, 0x80, 0x00, 0x80, 0x55, 0xaa, 0x00, 0x80, 0x00,
	0x80, 0x4b, 0xea, 0x5b, 0xca, 0x4b, 0x80, 0x00, 0x80, 0x00, 0x80, 0x55,
	0xaa, 0x00, 0x80, 0x00, 0x80, 0x74, 0xa6, 0x25, 0xa4, 0x74, 0x80, 0x00,
	0x80, 0x00, 0x80, 0x55, 0xaa, 0x00, 0x80, 0x00, 0x80, 0x4b, 0xea, 0x5b,
	0xca, 0x4b, 0x80, 0x00, 0x80, 0x00, 0x80, 0x55, 0xaa, 0x00, 0x80, 0x00,
	0x80, 0x39, 0xc2, 0x31, 0x88, 0x73, 0x80, 0x00, 0x80, 0x00, 0x80, 0x55,
	0xaa, 0x00, 0x80, 0x00, 0x80, 0x79, 0xc2, 0x79, 0xc0, 0x7b, 0x80, 0x00,
	0x80, 0x00, 0x80, 0x55, 0xaa, 0x00, 0x80, 0x00, 0x80, 0x4b, 0xc9, 0x79,
	0xc9, 0x49, 0x80, 0x00, 0x80, 0x00, 0x80, 0x55, 0xaa, 0x00, 0x80, 0x00,
	0x80, 0x25, 0xa4, 0x3c, 0xa4, 0x24, 0x80, 0x00, 0x80, 0x00, 0x80, 0x55,
	0xaa, 0x00, 0x80, 0x00, 0x80, 0x45, 0xc4, 0x44, 0xa8, 0x10, 0x80, 0x00,
	0x80, 0x00, 0x80, 0x55, 0xaa, 0x00, 0x80, 0x00, 0x80, 0x72, 0xca, 0x72,
	0xc2, 0x43, 0x80, 0x00, 0x80, 0x00, 0x80, 0x55, 0xaa, 0x00, 0x80, 0x00,
	0x80, 0x72, 0xca, 0x72, 0xc2, 0x43, 0x80, 0x00, 0x80, 0x00, 0x80, 0x55,
	0xaa, 0x00, 0x80, 0x00, 0x80, 0x0e, 0x89, 0x0e, 0x8a, 0x09, 0x80, 0x00,
	0x80, 0x00, 0x80, 0x55, 0xaa, 0x00, 0x80, 0x00, 0x80, 0x39, 0xc2, 0x31,
	0x88, 0x73, 0x80, 0x00, 0x80, 0x00, 0x80, 0x55, 0xaa, 0x00, 0x80, 0x00,
	0x80, 0x39, 0xc2, 0x31, 0x88, 0x73, 0x80, 0x00, 0x80, 0x00, 0x80, 0x55,
	0xaa, 0x00, 0x80, 0x00, 0x80, 0x71, 0xca, 0x4a, 0xca, 0x71, 0x80, 0x00,
	0x80, 0x00, 0x80, 0x55, 0xaa, 0x00, 0x80, 0x00, 0x80, 0x72, 0xca, 0x72,
	0xc2, 0x41, 0x80, 0x00, 0x80, 0x00, 0x80, 0x55, 0xaa, 0x00, 0x80, 0x00,
	0x80, 0x72, 0xca, 0x72, 0xc2, 0x41, 0x80, 0x00, 0x80, 0x00, 0x80, 0x55,
	0xaa, 0x00, 0x80, 0x00, 0x80, 0x3b, 0xc1, 0x31, 0x89, 0x71, 0x80, 0x00,
	0x80, 0x00, 0x80, 0x55, 0xaa, 0x00, 0x80, 0x00, 0x80, 0x39, 0xc2, 0x42,
	0xc2, 0x39, 0x80, 0x00, 0x80, 0x00, 0x80, 0x55, 0xaa, 0x00, 0x80, 0x00,
	0x80, 0x22, 0xb6, 0x2a, 0xa2, 0x22, 0x80, 0x00, 0x80, 0x00, 0x80, 0x55,
	0xaa, 0x00, 0x80, 0x00, 0x80, 0x3b, 0xc2, 0x33, 0x8a, 0x72, 0x80, 0x00,
	0x80, 0x00, 0x80, 0x55, 0xaa, 0x00, 0x80, 0x00, 0x80, 0x7b, 0xc2, 0x7b,
	0xc2, 0x7a, 0x80, 0x00, 0x80, 0x00, 0x80, 0x55, 0xaa, 0x00, 0x80, 0x00,
	0x80, 0x39, 0xc2, 0x32, 0x8a, 0x71, 0x80, 0x00, 0x80, 0x00, 0x80, 0x55,
	0xaa, 0x00, 0x80, 0x00, 0x80, 0x33, 0xc4, 0x25, 0x94, 0x63, 0x80, 0x00,
	0x80, 0x00, 0x80, 0x55, 0xaa, 0x00, 0x80, 0x00, 0x80, 0x39, 0xc2, 0x32,
	0x8a, 0x71, 0x80, 0x00, 0x80, 0x00, 0x80, 0x55, 0xaa, 0x00, 0x80, 0x00,
	0x80, 0x39, 0xc2, 0x41, 0xc0, 0x3b, 0x80, 0x00, 0x80, 0x00, 0x80, 0x55,
	0xaa, 0x00, 0x80, 0x00, 0x80, 0x0e, 0x90, 0x0c, 0x82, 0x1c, 0x80, 0x00,
	0x80, 0x00, 0x80, 0x55, 0xaa, 0x00, 0x80, 0x00, 0x80, 0x31, 0xca, 0x49,
	0xc8, 0x33, 0x80, 0x00, 0x80, 0x00, 0x80, 0x55, 0xaa, 0x00, 0x80, 0x00,
	0x80, 0x1c, 0x92, 0x1c, 0x90, 0x10, 0x80, 0x00, 0x80, 0x00, 0x80, 0x55,
	0xaa, 0x00, 0x80, 0x00, 0x80, 0x33, 0xca, 0x7b, 0xca, 0x4a, 0x80, 0x00,
	0x80, 0x00, 0x80, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x08, 0x08, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x3e, 0x49, 0x48, 0x48, 0x49, 0x3e,
	0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x10, 0x10, 0x10,
	0x7c, 0x10, 0x10, 0x10, 0x3e, 0x61, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x42, 0x3c, 0x24, 0x42, 0x42, 0x24, 0x3c, 0x42, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x41, 0x22, 0x14, 0x08, 0x7f, 0x08, 0x7f, 0x08,
	0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08,
	0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x3c, 0x42, 0x40, 0x3c, 0x42, 0x42, 0x3c, 0x02, 0x42, 0x3c, 0x00, 0x00,
	0x24, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x42, 0x99, 0xa5,
	0xa1, 0xa1, 0xa5, 0x99, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x02,
	0x1e, 0x22, 0x1e, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0x12, 0x24, 0x24, 0x48, 0x24, 0x24,
	0x12, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x7e, 0x02, 0x02, 0x02, 0x00, 0x00, 0xaa, 0x00, 0x80, 0x3a,
	0xc2, 0x33, 0x8a, 0x72, 0x80, 0x00, 0x80, 0x03, 0x80, 0x00, 0x80, 0x55,
	0x00, 0x00, 0x00, 0x00, 0x3c, 0x42, 0xb9, 0xa5, 0xa5, 0xb9, 0xa9, 0xa5,
	0x42, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x18, 0x24, 0x24, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x7f, 0x08, 0x08, 0x08, 0x00,
	0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x44, 0x04, 0x18, 0x20,
	0x40, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38,
	0x44, 0x04, 0x38, 0x04, 0x44, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x04, 0x08, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x42,
	0x42, 0x42, 0x42, 0x42, 0x66, 0x59, 0x40, 0x80, 0x00, 0x00, 0x00, 0x00,
	0x3f, 0x7a, 0x7a, 0x7a, 0x3a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x30, 0x00, 0x00, 0x00, 0x10,
	0x30, 0x50, 0x10, 0x10, 0x10, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x1c, 0x22, 0x22, 0x22, 0x1c, 0x00, 0x3e, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x48, 0x24,
	0x24, 0x12, 0x24, 0x24, 0x48, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x22, 0x62, 0x24, 0x28, 0x28, 0x12, 0x16, 0x2a, 0x4e, 0x42, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x22, 0x62, 0x24, 0x28, 0x28, 0x14, 0x1a, 0x22,
	0x44, 0x4e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x62, 0x12, 0x24, 0x18,
	0x68, 0x12, 0x16, 0x2a, 0x4e, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x10, 0x10, 0x00, 0x10, 0x10, 0x20, 0x40, 0x42, 0x42, 0x3c, 0x00, 0x00,
	0x30, 0x0c, 0x00, 0x00, 0x18, 0x24, 0x24, 0x42, 0x42, 0x7e, 0x42, 0x42,
	0x42, 0x42, 0x00, 0x00, 0x0c, 0x30, 0x00, 0x00, 0x18, 0x24, 0x24, 0x42,
	0x42, 0x7e, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00, 0x18, 0x24, 0x00, 0x00,
	0x18, 0x24, 0x24, 0x42, 0x42, 0x7e, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00,
	0x32, 0x4c, 0x00, 0x00, 0x18, 0x24, 0x24, 0x42, 0x42, 0x7e, 0x42, 0x42,
	0x42, 0x42, 0x00, 0x00, 0x24, 0x24, 0x00, 0x00, 0x18, 0x24, 0x24, 0x42,
	0x42, 0x7e, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00, 0x18, 0x24, 0x18, 0x00,
	0x18, 0x24, 0x24, 0x42, 0x42, 0x7e, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x1f, 0x28, 0x48, 0x48, 0x7f, 0x48, 0x48, 0x48,
	0x48, 0x4f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x42, 0x42, 0x40,
	0x40, 0x40, 0x40, 0x42, 0x42, 0x3c, 0x08, 0x30, 0x30, 0x0c, 0x00, 0x00,
	0x7e, 0x40, 0x40, 0x40, 0x7c, 0x40, 0x40, 0x40, 0x40, 0x7e, 0x00, 0x00,
	0x0c, 0x30, 0x00, 0x00, 0x7e, 0x40, 0x40, 0x40, 0x7c, 0x40, 0x40, 0x40,
	0x40, 0x7e, 0x00, 0x00, 0x18, 0x24, 0x00, 0x00, 0x7e, 0x40, 0x40, 0x40,
	0x7c, 0x40, 0x40, 0x40, 0x40, 0x7e, 0x00, 0x00, 0x24, 0x24, 0x00, 0x00,
	0x7e, 0x40, 0x40, 0x40, 0x7c, 0x40, 0x40, 0x40, 0x40, 0x7e, 0x00, 0x00,
	0x18, 0x06, 0x00, 0x00, 0x3e, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x3e, 0x00, 0x00, 0x0c, 0x30, 0x00, 0x00, 0x3e, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x3e, 0x00, 0x00, 0x18, 0x24, 0x00, 0x00,
	0x3e, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x3e, 0x00, 0x00,
	0x24, 0x24, 0x00, 0x00, 0x3e, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x44, 0x42, 0x42,
	0xf2, 0x42, 0x42, 0x42, 0x44, 0x78, 0x00, 0x00, 0x32, 0x4c, 0x00, 0x00,
	0x42, 0x62, 0x62, 0x52, 0x52, 0x4a, 0x4a, 0x46, 0x46, 0x42, 0x00, 0x00,
	0x30, 0x0c, 0x00, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
	0x42, 0x3c, 0x00, 0x00, 0x0c, 0x30, 0x00, 0x00, 0x3c, 0x42, 0x42, 0x42,
	0x42, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x18, 0x24, 0x00, 0x00,
	0x3c, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00,
	0x32, 0x4c, 0x00, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
	0x42, 0x3c, 0x00, 0x00, 0x24, 0x24, 0x00, 0x00, 0x3c, 0x42, 0x42, 0x42,
	0x42, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x42, 0x24, 0x18, 0x24, 0x42, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 0x3a, 0x44, 0x46, 0x4a, 0x4a, 0x52, 0x52, 0x62,
	0x22, 0x5c, 0x40, 0x00, 0x30, 0x0c, 0x00, 0x00, 0x42, 0x42, 0x42, 0x42,
	0x42, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x0c, 0x30, 0x00, 0x00,
	0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00,
	0x18, 0x24, 0x00, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
	0x42, 0x3c, 0x00, 0x00, 0x24, 0x24, 0x00, 0x00, 0x42, 0x42, 0x42, 0x42,
	0x42, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x0c, 0x30, 0x00, 0x00,
	0x41, 0x41, 0x22, 0x22, 0x14, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x40, 0x40, 0x78, 0x44, 0x42, 0x42, 0x44, 0x78, 0x40,
	0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x44, 0x44, 0x48,
	0x58, 0x44, 0x42, 0x42, 0x52, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x30, 0x0c,
	0x00, 0x00, 0x3c, 0x42, 0x02, 0x3e, 0x42, 0x42, 0x46, 0x3a, 0x00, 0x00,
	0x00, 0x00, 0x0c, 0x30, 0x00, 0x00, 0x3c, 0x42, 0x02, 0x3e, 0x42, 0x42,
	0x46, 0x3a, 0x00, 0x00, 0x00, 0x00, 0x18, 0x24, 0x00, 0x00, 0x3c, 0x42,
	0x02, 0x3e, 0x42, 0x42, 0x46, 0x3a, 0x00, 0x00, 0x00, 0x00, 0x32, 0x4c,
	0x00, 0x00, 0x3c, 0x42, 0x02, 0x3e, 0x42, 0x42, 0x46, 0x3a, 0x00, 0x00,
	0x00, 0x00, 0x24, 0x24, 0x00, 0x00, 0x3c, 0x42, 0x02, 0x3e, 0x42, 0x42,
	0x46, 0x3a, 0x00, 0x00, 0x00, 0x18, 0x24, 0x18, 0x00, 0x00, 0x3c, 0x42,
	0x02, 0x3e, 0x42, 0x42, 0x46, 0x3a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x3e, 0x49, 0x09, 0x3f, 0x48, 0x48, 0x49, 0x3e, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x42, 0x40, 0x40, 0x40, 0x40,
	0x42, 0x3c, 0x08, 0x30, 0x00, 0x00, 0x30, 0x0c, 0x00, 0x00, 0x3c, 0x42,
	0x42, 0x7e, 0x40, 0x40, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x30,
	0x00, 0x00, 0x3c, 0x42, 0x42, 0x7e, 0x40, 0x40, 0x42, 0x3c, 0x00, 0x00,
	0x00, 0x00, 0x18, 0x24, 0x00, 0x00, 0x3c, 0x42, 0x42, 0x7e, 0x40, 0x40,
	0x42, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x24, 0x24, 0x00, 0x00, 0x3c, 0x42,
	0x42, 0x7e, 0x40, 0x40, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x30, 0x0c,
	0x00, 0x00, 0x18, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x3e, 0x00, 0x00,
	0x00, 0x00, 0x0c, 0x30, 0x00, 0x00, 0x18, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x18, 0x24, 0x00, 0x00, 0x18, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x24, 0x24,
	0x00, 0x00, 0x18, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x3e, 0x00, 0x00,
	0x00, 0x00, 0x32, 0x0c, 0x14, 0x22, 0x02, 0x3e, 0x42, 0x42, 0x42, 0x42,
	0x42, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x32, 0x4c, 0x00, 0x00, 0x5c, 0x62,
	0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00, 0x30, 0x0c,
	0x00, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00,
	0x00, 0x00, 0x0c, 0x30, 0x00, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x42,
	0x42, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x18, 0x24, 0x00, 0x00, 0x3c, 0x42,
	0x42, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x32, 0x4c,
	0x00, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00,
	0x00, 0x00, 0x24, 0x24, 0x00, 0x00, 0x3c, 0x42, 0x42, 0x42, 0x42, 0x42,
	0x42, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00,
	0x00, 0x7e, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x02, 0x3c, 0x46, 0x4a, 0x4a, 0x52, 0x52, 0x62, 0x3c, 0x40, 0x00,
	0x00, 0x00, 0x30, 0x0c, 0x00, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
	0x46, 0x3a, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x30, 0x00, 0x00, 0x42, 0x42,
	0x42, 0x42, 0x42, 0x42, 0x46, 0x3a, 0x00, 0x00, 0x00, 0x00, 0x18, 0x24,
	0x00, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x46, 0x3a, 0x00, 0x00,
	0x00, 0x00, 0x24, 0x24, 0x00, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
	0x46, 0x3a, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x30, 0x00, 0x00, 0x42, 0x42,
	0x42, 0x42, 0x42, 0x26, 0x1a, 0x02, 0x02, 0x3c, 0x00, 0x00, 0x00, 0x40,
	0x40, 0x40, 0x5c, 0x62, 0x42, 0x42, 0x42, 0x42, 0x62, 0x5c, 0x40, 0x40,
	0x00, 0x00, 0x24, 0x24, 0x00, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x26,
	0x1a, 0x02, 0x02, 0x3c,
};
