#include <stdint.h>

/* UART */
#define UART_TX        (*(volatile uint8_t*)0x80000000)

/* CLINT */
#define CLINT_BASE     0x02000000
#define CLINT_MSIP     (*(volatile uint32_t*)(CLINT_BASE + 0x0000))
#define CLINT_MTIMECMP (*(volatile uint32_t*)(CLINT_BASE + 0x4000))
#define CLINT_MTIME    (*(volatile uint32_t*)(CLINT_BASE + 0xBFF8))

static void putc(char c)
{
    UART_TX = c;
}

static void puts(const char* s)
{
    while (*s)
        putc(*s++);
}

/* ------------------------------------------------------------
 * Trigger software interrupt
 * ------------------------------------------------------------ */
void trigger_msip(void)
{
    puts("[MSIP]\n");
    CLINT_MSIP = 1;   /* causes MSIP */
    for (volatile int i = 0; i < 1000; i++);
}

/* ------------------------------------------------------------
 * Trigger timer interrupt
 * ------------------------------------------------------------ */
void trigger_mtip(void)
{
    puts("[MTIP]\n");
    uint32_t now = CLINT_MTIME;
    CLINT_MTIMECMP = now + 20000;
    for (volatile int i = 0; i < 50000; i++);
}

/* ------------------------------------------------------------
 * Trigger synchronous exception
 * ------------------------------------------------------------ */
void trigger_exception(void)
{
    puts("[EXCEPTION]\n");

    /* Illegal instruction */
    asm volatile (".word 0xffffffff");

    puts("[EXCEPTION RETURNED]\n");
}

/* ------------------------------------------------------------
 * main()
 * ------------------------------------------------------------ */
int main(void)
{
    puts("=== Interrupt / Exception Test ===\n");

    trigger_msip();
    puts("Returned from MSIP\n");

    trigger_mtip();
    puts("Returned from MTIP\n");

    puts("[WAITING FOR EXTERNAL INTERRUPT]\n");
    for (volatile int i = 0; i < 10; i++);

    trigger_exception();

    puts("Program continues after exception\n");
    puts("ALL TESTS DONE\n");

    return 0;
}


// Following is the boot code which works for this test case
#if 0
#include "defines.h"
#include "tb.h"

/* Machine Interrupts */
#define MSTATUS_MIE     (1 << 3)
#define MIE_MSIE        (1 << 3)
#define MIE_MTIE        (1 << 7)
#define MIE_MEIE        (1 << 11)
#define STDOUT 0x80000000
#define CLINT_BASE     0x02000000
#define CLINT_MSIP     CLINT_BASE + 0x0000
#define CLINT_MTIMECMP CLINT_BASE + 0x4000
#define CLINT_MTIME    CLINT_BASE + 0xBFF8


.section .text.init
.align 4
.global _start
_start:

# setup stack befpore enabling interrupts
la sp, STACK

## Set trap handler
la t0, _trap
csrw mtvec, t0

# Clear pending interrupts, may not work
csrw mip, zero

# Set all 256 MB PMA regions as side-effect regions except for TCM/PIC internal (0),
# Boot ROM/SRAM (1) and memory-mapped SPI Flash (3)
li t0, 0xAAAAAA64
#csrw 0x7c0, t0

/* Enable machine interrupts: SW, timer, external */
li   t0, (MIE_MSIE | MIE_MTIE | MIE_MEIE)
	csrw mie, t0
	li   t0, MSTATUS_MIE
	csrs mstatus, t0

	call main

# Map exit code of main() to command to be written to tohost
	snez a0, a0
	bnez a0, _finish
	li   a0, 0xFF

	.global _finish
	_finish:

#la t0, tohost
	li t0, STDOUT
	sb a0, 0(t0)  # DemoTB test termination
	.rept 10
	nop
	.endr
	1:
#infinite loop to terminate bare metal programs
	wfi
	j 1b

	.align 4
	_trap:
	li   t0, MSTATUS_MIE
	csrc mstatus, t0

	csrr t0, mcause

# ---- MSIP ----
	li   t1, 0x80000003
	bne  t0, t1, 1f
	la   t0, handle_msip
	jr   t0

	1:
# ---- MTIP ----
	li   t1, 0x80000007
	bne  t0, t1, 2f
	la   t0, handle_mtip
	jr   t0

	2:
# ---- Exception ----
	la   t0, handle_exception
	jr   t0

# return for debug purposes
	mret
# ---- Save registers ----
	addi sp, sp, -32
	sw   ra, 28(sp)
	sw   t0, 24(sp)
	sw   t1, 20(sp)
	sw   a0, 16(sp)
sw   a1, 12(sp)

# Print "MTVEC="
	li   t1, STDOUT
	li   a0, 'M' ; sb a0, 0(t1)
	li   a0, 'T' ; sb a0, 0(t1)
	li   a0, 'V' ; sb a0, 0(t1)
	li   a0, 'E' ; sb a0, 0(t1)
	li   a0, 'C' ; sb a0, 0(t1)
	li   a0, '=' ; sb a0, 0(t1)

	csrr a0, mtvec
	call print_hex32

	li   t1, STDOUT
	li   a0, '\n'
sb   a0, 0(t1)



# Print "MEPC="
	li   t1, STDOUT
	li   a0, 'M' ; sb a0, 0(t1)
	li   a0, 'E' ; sb a0, 0(t1)
	li   a0, 'P' ; sb a0, 0(t1)
	li   a0, 'C' ; sb a0, 0(t1)
	li   a0, '=' ; sb a0, 0(t1)

	csrr a0, mepc
	call print_hex32

	li   t1, STDOUT
	li   a0, '\n'
sb   a0, 0(t1)

	lw   a1, 12(sp)
	lw   a0, 16(sp)
	lw   t1, 20(sp)
	lw   t0, 24(sp)
lw   ra, 28(sp)
	addi sp, sp, 32

##1:
##    wfi
	.global _exit
	_exit:
	j _finish

	.section .data.io
	.global tohost
	tohost: .word STDOUT

	handle_msip:
	li t0, 0x2000000   # CLINT MSIP
	sw zero, 0(t0)     # clear interrupt

	csrr t0, mepc
	addi t0, t0, 4
	csrw mepc, t0
	mret

	handle_mtip:
# update mtimecmp here
	csrr t0, mepc
	addi t0, t0, 4
	csrw mepc, t0
	mret

	handle_exception:
# Skip the instruction that caused the exception
	csrr t0, mepc
	addi t0, t0, 4
	csrw mepc, t0
	mret

	print_hex_nibble:
	addi sp, sp, -8
	sw   ra, 4(sp)
sw   t0, 0(sp)

	li   t0, 10
	blt  a0, t0, 1f
	addi a0, a0, 55     # 'A' - 10
	j    2f
	1:
	addi a0, a0, 48     # '0'
	2:
	li   t0, STDOUT
sb   a0, 0(t0)

	lw   t0, 0(sp)
lw   ra, 4(sp)
	addi sp, sp, 8
	ret

	print_hex32:
	addi sp, sp, -16
	sw   ra, 12(sp)
	sw   t2, 8(sp)
sw   t3, 4(sp)
	sw   a0, 0(sp)      # save original value

	li   t2, 28

	1:
	lw   a0, 0(sp)      # restore original value each time
	srl  t3, a0, t2
	andi a0, t3, 0xF
	call print_hex_nibble

	addi t2, t2, -4
	bgez t2, 1b

	lw   a0, 0(sp)
	lw   t3, 4(sp)
	lw   t2, 8(sp)
lw   ra, 12(sp)
	addi sp, sp, 16
	ret
#endif
