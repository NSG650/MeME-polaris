#include <stdint.h>
#include <stddef.h>
#include <memewm/memewm.h>

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

static uint64_t ticks = 0;

__attribute__((interrupt)) static void pit_handler(void *p) {
    (void)p;

    ticks++;

    // refresh wm at 30 hz
    if (!(ticks % 30)) {
        memewm_refresh();
    }

    pic_eoi(0);
}

static void init_idt(void) {
    for (size_t i = 0; i < 256; i++)
        register_interrupt_handler(i, unhandled_interrupt, 0, 0x8e);

    register_interrupt_handler(0x20, pit_handler, 0, 0x8e);

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

struct stivale_struct {
    uint64_t  cmdline;
    uint64_t  memory_map_addr;
    uint64_t  memory_map_entries;
    uint32_t *framebuffer_addr;
    uint16_t  framebuffer_pitch;
    uint16_t  framebuffer_width;
    uint16_t  framebuffer_height;
    uint16_t  framebuffer_bpp;
    uint64_t  rsdp;
    uint64_t  module_count;
    uint64_t  modules;
    uint64_t  epoch;
    uint64_t  flags;       // bit 0: 1 if booted with BIOS, 0 if booted with UEFI
} __attribute__((packed));

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

void main(struct stivale_struct *stivale_struct) {
    pic_remap(0x20, 0x28);
    init_idt();

    asm volatile ("sti");

    init_pit();
    pic_set_mask(0, 0);

    memewm_init(stivale_struct->framebuffer_addr,
                stivale_struct->framebuffer_width,
                stivale_struct->framebuffer_height,
                stivale_struct->framebuffer_pitch,
                font,
                8,
                16);

    memewm_window_create("test1", 30, 30, 800, 400);
    memewm_window_create("test2", 50, 50, 800, 400);
    memewm_window_create("test3", 70, 70, 800, 400);
    memewm_window_create("test4", 90, 90, 800, 400);

    for (;;) {
        asm volatile ("hlt");
    }
}
