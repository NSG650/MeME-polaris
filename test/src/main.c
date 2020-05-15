#include <stdint.h>
#include <stddef.h>
#include <memewm/memewm.h>

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
void *balloc_aligned(size_t count, size_t alignment) {
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

void main(struct stivale_struct *stivale_struct) {
    memewm_init(stivale_struct->framebuffer_addr,
                stivale_struct->framebuffer_width,
                stivale_struct->framebuffer_height,
                stivale_struct->framebuffer_pitch,
                NULL,
                8,
                16);

    memewm_window_create("test", 30, 30, 800, 400);

    memewm_refresh();

    for (;;);
}
