/* Minimal startup code for SEP platform */

.section .text.init
.global _start

_start:
    # Set up trap handler before anything else
    la   t0, trap_handler
    csrw mtvec, t0

    # Initialize stack pointer
    la sp, _stack_top

    # Clear BSS section
    la t0, __bss_start
    la t1, __bss_end
clear_bss:
    bge t0, t1, bss_done
    sw zero, 0(t0)
    addi t0, t0, 4
    j clear_bss

bss_done:
    # Call main
    call main

    # If main returns, loop forever
_exit:
    j _exit

# Trap handler - catches ecall and any other exceptions
trap_handler:
    j trap_handler
