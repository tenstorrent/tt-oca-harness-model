/* Common startup code for SEP platform */

.section .text.init
.global _start

_start:
    # Set trap vector
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
    # Point NMI at _nmi_handler in SRAM. cpu_ctrl / JSON default to
    # 0xC0000100 (empty ICCM); a bus-error NMI would fetch garbage there
    # and spin in trap_handler. stdout 4-byte write with low byte 0x81 is
    # the LOAD_NMI_ADDR mailbox (handler must be 256-byte aligned).
    la   t0, _nmi_handler
    ori  t0, t0, 0x81
    li   t1, 0x80000000
    sw   t0, 0(t1)

    # Call main
    call main

    # If main returns, loop forever
_exit:
    j _exit

# ============================================================================
# Generic trap handler
# ============================================================================
trap_handler:
    j trap_handler

# ============================================================================
# NMI handler — used by AON watchdog bark and by VeeR bus-error NMI.
# Linked in .text so the image actually contains it (the orphan
# .nmi_handler section was discarded by link.ld). 256-byte aligned for
# the stdout LOAD_NMI_ADDR mailbox.
# ============================================================================
.section .text
.balign 256
.global _nmi_handler

_nmi_handler:
    # Clear AON_INTR_STATE.wdog_timer_bark (bit 1)
    li   t0, 0x1080102C
    li   t1, 0x2
    sw   t1, 0(t0)

    # Disable watchdog to avoid re-triggering
    li   t0, 0x1080101C
    sw   zero, 0(t0)

    # Return from NMI
    mret