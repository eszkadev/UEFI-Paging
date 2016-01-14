.text
.globl PageFaultHandler
PageFaultHandler:

call SetPagePresent

add $0x4, %esp
iret
