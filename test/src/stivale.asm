section .stivalehdr

stivale_header:
    dq stack.top    ; rsp
    dw 1            ; video mode
    dw 0          ; fb_width
    dw 0          ; fb_height
    dw 0          ; fb_bpp
    dq 0

section .bss

stack:
    resb 4096
  .top:

section .rodata

global font
font:
    incbin 'src/bitmap_font.fnt'
