.text
.globl GetIDTR
GetIDTR:

mov 0x0, %eax
sidt (%eax)

ret
