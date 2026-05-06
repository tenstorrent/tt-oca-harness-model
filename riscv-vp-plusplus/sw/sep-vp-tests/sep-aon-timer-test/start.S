/* Minimal startup code for SEP platform */

.section .text.init
.global _start

_start:
    # Set up trap handler before anything else
    .option push
    .option arch, +zicsr
    la   t0, trap_handler
    csrw mtvec, t0
    .option pop

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

# ============================================================================
# NMI handler — entered when intr_wdog_timer_bark_o asserts.
# Must be 256-byte aligned (VeeR's nmi_vec carries only bits [31:8]).
# Clears the bark interrupt and disables the watchdog, then returns via mret.
# ============================================================================
.section .nmi_handler, "ax"
.balign 256
.global _nmi_handler
_nmi_handler:
    # Clear AON_INTR_STATE.wdog_timer_bark (W1C, bit 1)
    li   t0, 0x1080102C    # AON_TIMER_BASE + 0x2C
    li   t1, 0x2
    sw   t1, 0(t0)

    # Disable watchdog so it can't immediately re-fire
    li   t0, 0x1080101C    # AON_TIMER_BASE + 0x1C  (WDOG_CTRL)
    sw   zero, 0(t0)

    # Resume interrupted code
    .option push
    .option arch, +zicsr
    mret
    .option pop
