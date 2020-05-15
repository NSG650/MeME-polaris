section .stivalehdr

stivale_header:
    dq stack.top    ; rsp
    dw 0            ; video mode
    dw 0          ; fb_width
    dw 0          ; fb_height
    dw 0          ; fb_bpp

section .bss

stack:
    resb 4096
  .top:
