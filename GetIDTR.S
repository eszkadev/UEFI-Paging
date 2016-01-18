.text
.globl GetIDTR
GetIDTR:

mov 4(%esp), %eax
sidt (%eax)

ret
