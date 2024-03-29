#include <stdint.h>
#include <stddef.h>
#include <memewm/memewm.h>
#include <limine.h>

#define port_out_b(port, value) ({				\
	asm volatile (	"out dx, al"				\
					:							\
					: "a" (value), "d" (port)	\
					: );						\
})

#define port_in_b(port) ({						\
	uint8_t value;								\
	asm volatile (	"in al, dx"					\
					: "=a" (value)				\
					: "d" (port)				\
					: );						\
	value;										\
})

static uint64_t ticks = 0;

#define PIT_FREQUENCY_HZ 1000

static void init_pit(void) {
    uint16_t x = 1193182 / PIT_FREQUENCY_HZ;
    if ((1193182 % PIT_FREQUENCY_HZ) > (PIT_FREQUENCY_HZ / 2))
        x++;

    port_out_b(0x40, (uint8_t)(x & 0x00ff));
    port_out_b(0x40, (uint8_t)((x & 0xff00) >> 8));
}

static void pic_set_mask(uint8_t line, int status) {
    uint16_t port;
    uint8_t value;

    if (line < 8) {
        port = 0x21;
    } else {
        port = 0xa1;
        line -= 8;
    }

    if (!status)
        value = port_in_b(port) & ~((uint8_t)1 << line);
    else
        value = port_in_b(port) | ((uint8_t)1 << line);

    port_out_b(port, value);
}

static void pic_eoi(uint8_t current_vector) {
    if (current_vector >= 8) {
        port_out_b(0xa0, 0x20);
    }

    port_out_b(0x20, 0x20);
}

static void pic_remap(uint8_t pic0_offset, uint8_t pic1_offset) {
    port_out_b(0x20, 0x11);
    port_out_b(0xa0, 0x11);

    port_out_b(0x21, pic0_offset);
    port_out_b(0xa1, pic1_offset);

    port_out_b(0x21, 4);
    port_out_b(0xa1, 2);

    port_out_b(0x21, 1);
    port_out_b(0xa1, 1);

    port_out_b(0x21, 0xff);
    port_out_b(0xa1, 0xff);
}

// mouse code partly (mostly) taken from https://forum.osdev.org/viewtopic.php?t=10247

static inline void mouse_wait(int type) {
    int timeout = 100000;

    if (type == 0) {
        while (timeout--) {
            if (port_in_b(0x64) & (1 << 0)) {
                return;
            }
        }
    } else {
        while (timeout--) {
            if (!(port_in_b(0x64) & (1 << 1))) {
                return;
            }
        }
    }
}

static inline void mouse_write(uint8_t val) {
    mouse_wait(1);
    port_out_b(0x64, 0xd4);
    mouse_wait(1);
    port_out_b(0x60, val);
}

static inline uint8_t mouse_read(void) {
    mouse_wait(0);
    return port_in_b(0x60);
}

static void init_mouse(void) {
    mouse_wait(1);
    port_out_b(0x64, 0xa8);

    mouse_wait(1);
    port_out_b(0x64, 0x20);
    uint8_t status = mouse_read();
    mouse_read();
    status |= (1 << 1);
    status &= ~(1 << 5);
    mouse_wait(1);
    port_out_b(0x64, 0x60);
    mouse_wait(1);
    port_out_b(0x60, status);
    mouse_read();

    mouse_write(0xff);
    mouse_read();

    mouse_write(0xf6);
    mouse_read();

    mouse_write(0xf4);
    mouse_read();
}

typedef struct {
    uint8_t flags;
    uint8_t x_mov;
    uint8_t y_mov;
} mouse_packet_t;

static int handler_cycle = 0;
static mouse_packet_t current_packet;
static int discard_packet = 0;

__attribute__((interrupt)) static void mouse_handler(void *p) {
    (void)p;

    // we will get some spurious packets at the beginning and they will fuck
    // up the alignment of the handler cycle so just ignore everything in
    // the first 250 milliseconds after boot
    if (ticks < 250) {
        port_in_b(0x60);
        goto out;
    }

    switch (handler_cycle) {
        case 0:
            current_packet.flags = port_in_b(0x60);
            handler_cycle++;
            if (current_packet.flags & (1 << 6) || current_packet.flags & (1 << 7))
                discard_packet = 1;     // discard rest of packet
            if (!(current_packet.flags & (1 << 3)))
                discard_packet = 1;     // discard rest of packet
            break;
        case 1:
            current_packet.x_mov = port_in_b(0x60);
            handler_cycle++;
            break;
        case 2: {
            current_packet.y_mov = port_in_b(0x60);
            handler_cycle = 0;

            if (discard_packet) {
                discard_packet = 0;
                break;
            }

            // process packet
            int64_t x_mov, y_mov;

            if (current_packet.flags & (1 << 4)) {
                x_mov = (int8_t)current_packet.x_mov;
            } else
                x_mov = current_packet.x_mov;

            if (current_packet.flags & (1 << 5)) {
                y_mov = (int8_t)current_packet.y_mov;
            } else
                y_mov = current_packet.y_mov;

            int last_x, last_y, new_x, new_y;

            memewm_get_cursor_pos(&last_x, &last_y);
            window_click_data_t last_click_data = memewm_window_click(last_x, last_y);

            memewm_set_cursor_pos(x_mov, -y_mov);

            if (!(current_packet.flags & (1 << 0)))
                break;

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

            if (last_click_data.rel_x != -1 && last_click_data.rel_y != -1) {
                memewm_window_focus(id);
                memewm_window_plot_px(last_click_data.rel_x, last_click_data.rel_y,
                                      0xffffff, id);
            }

            break;
        }
    }

out:
    pic_eoi(12);
}

struct idt_entry_t {
    uint16_t offset_lo;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_hi;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr_t {
    uint16_t size;
    uint64_t address;
} __attribute__((packed));

static struct idt_entry_t idt[256] = {0};

static void register_interrupt_handler(size_t vec, void *handler, uint8_t ist, uint8_t type) {
    uint64_t p = (uint64_t)handler;

    idt[vec].offset_lo  = (uint16_t)p;
    idt[vec].selector   = 0x28;
    idt[vec].ist        = ist;
    idt[vec].type_attr  = type;
    idt[vec].offset_mid = (uint16_t)(p >> 16);
    idt[vec].offset_hi  = (uint32_t)(p >> 32);
    idt[vec].zero       = 0;
}

__attribute__((interrupt)) static void unhandled_interrupt(void *p) {
    (void)p;
    asm volatile (
        "cli\n\t"
        "1: hlt\n\t"
        "jmp 1b\n\t"
    );
}

__attribute__((interrupt)) static void pit_handler(void *p) {
    (void)p;

    ticks++;

    // refresh wm at 30 hz
    if (!(ticks % (PIT_FREQUENCY_HZ / 30))) {
        memewm_refresh();
    }

    pic_eoi(0);
}

static void init_idt(void) {
    for (size_t i = 0; i < 256; i++)
        register_interrupt_handler(i, unhandled_interrupt, 0, 0x8e);

    register_interrupt_handler(0x20 + 0 , pit_handler,   0, 0x8e);
    register_interrupt_handler(0x20 + 12, mouse_handler, 0, 0x8e);

    struct idt_ptr_t idt_ptr = {
        sizeof(idt) - 1,
        (uint64_t)idt
    };

    asm volatile (
        "lidt %0"
        :
        : "m" (idt_ptr)
    );
}

static size_t bump_allocator_base = 0x1000000;

// Only power of 2 alignments
static void *balloc_aligned(size_t count, size_t alignment) {
    size_t new_base = bump_allocator_base;
    if (new_base & (alignment - 1)) {
        new_base &= ~(alignment - 1);
        new_base += alignment;
    }
    void *ret = (void *)new_base;
    new_base += count;
    bump_allocator_base = new_base;
    return ret;
}

void *memewm_malloc(size_t count) {
    return balloc_aligned(count, 4);
}

void memewm_free(void *ptr) {
    (void)ptr;
}

extern uint8_t font[];

static volatile struct limine_framebuffer_request fb_req = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

void _start(void) {
    pic_remap(0x20, 0x28);
    init_idt();

    // enable cascade
    pic_set_mask(2, 0);

    init_pit();
    pic_set_mask(0, 0);

    init_mouse();
    pic_set_mask(12, 0);

    struct limine_framebuffer *fb = fb_req.response->framebuffers[0];

    memewm_init(fb->address,
                fb->width,
                fb->height,
                fb->pitch,
                font,
                8,
                16);

    memewm_window_create("test1", 30, 30, 800, 400);
    memewm_window_create("test2", 50, 50, 800, 400);
    memewm_window_create("test3", 70, 70, 800, 400);
    memewm_window_create("test4", 90, 90, 800, 400);

    asm volatile ("sti");
    for (;;) {
        asm volatile ("hlt");
    }
}
