.text
.globl SetPageDirectory
SetPageDirectory:

mov 4(%esp), %eax
mov %eax, %cr3

ret
