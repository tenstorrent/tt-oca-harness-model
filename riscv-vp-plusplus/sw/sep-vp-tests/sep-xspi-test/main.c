/*
 * XSPI bring-up checks for SEP VP.
 *
 * Uses SEP AXI extension base for cdns_xspi_ctrl_reg (see SEP_AXI_EXTENSION_
 * CDNS_XSPI_CTRL_REG_REG_MAP_BASE_ADDR = 0x20002000).
 */

#include <stdint.h>

extern void uart_print(const char *str);
extern void uart_print_dec(uint32_t value);
extern void uart_print_hex(uint32_t value, int digits);

#define STDOUT_ADDR  0x80000000

static void console_putc(char c)
{
    *((volatile uint32_t *)STDOUT_ADDR) = (uint32_t)c;
}

static void console_print(const char *str)
{
    while (*str) {
        console_putc(*str++);
    }
}

#define REG_READ(addr)       (*((volatile uint32_t *)(addr)))
#define REG_WRITE(addr, val) (*((volatile uint32_t *)(addr)) = (val))

/* SEP_AXI_EXTENSION_CDNS_XSPI_CTRL_REG_REG_MAP_BASE_ADDR */
#define XSPI_BASE              0x20002000u

/* ctrl_cmd_stat_a.ctrl_status — RO @ offset 0x0100 (see register_map / och_sep_top RDL) */
#define XSPI_CTRL_STATUS_OFF    0x100u
#define XSPI_CTRL_STATUS_INIT_COMP_MSK          (1u << 16)
/* RO status: 1 = discovery inhibited at PoR strap; 0 = discovery allowed (bootstrap pin = 0) */
#define XSPI_CTRL_STATUS_DISCOVERY_INHIBIT_MSK  (1u << 5)

/* ctrl_cfg_common_a.long_polling — reset 0x3E8; write mask lower 16 bits */
#define XSPI_LONG_POLLING_OFF  0x208u

/* Poll bound so a stuck controller fails the test instead of hanging forever */
#define INIT_COMP_POLL_MAX      50000u

/**
 * Hardware (bootstrap / PoR straps):
 * discovery_inhibit must be 0 before the xSPI controller exits reset so device
 * discovery can run. Software cannot change straps after reset; integrated SoCs
 * must drive this pin. Optional observation: the physical init_comp pin tracks
 * init completion like bit 16 below — this test only polls MMIO.
 *
 * SEP VP note: PoR_input_signals may be tied by the platform; this firmware
 * verifies behaviour via ctrl_status after boot.
 */
static int wait_init_comp_and_check_por_status(void)
{
    uint32_t n;

    uart_print("  Waiting for ctrl_status.init_comp (bit 16) @ offset 0x100...\n");

    for (n = 0; n < INIT_COMP_POLL_MAX; n++) {
        uint32_t st = REG_READ(XSPI_BASE + XSPI_CTRL_STATUS_OFF);
        if ((st & XSPI_CTRL_STATUS_INIT_COMP_MSK) != 0u) {
            uart_print("  init_comp asserted. ctrl_status = 0x");
            uart_print_hex(st, 8);
            uart_print("\n");

            if ((st & XSPI_CTRL_STATUS_DISCOVERY_INHIBIT_MSK) != 0u) {
                uart_print(
                    "RESULT: FAIL (discovery_inhibit status=1 — bootstrap inhibited discovery)\n");
                console_print("\n*** discovery_inhibit strap check FAILED ***\n");
                return -1;
            }

            uart_print(
                "  discovery_inhibit status bit = 0 (discovery allowed at PoR).\n");
            return 0;
        }
    }

    uart_print("RESULT: FAIL (timeout waiting for init_comp)\n");
    console_print("\n*** init_comp poll FAILED ***\n");
    return -1;
}

int main(void)
{
    const uint32_t test_val = 0x155Au;
    uint32_t rb;
    int por_rc;

    uart_print("\n=== XSPI PoR / init + register R/W test ===\n");

    por_rc = wait_init_comp_and_check_por_status();
    if (por_rc != 0) {
        return 1;
    }

    uart_print("\n=== XSPI long_polling R/W test ===\n");

    REG_WRITE(XSPI_BASE + XSPI_LONG_POLLING_OFF, test_val);
    rb = REG_READ(XSPI_BASE + XSPI_LONG_POLLING_OFF);

    uart_print("  wrote long_polling = 0x");
    uart_print_hex(test_val, 4);
    uart_print(", read back = 0x");
    uart_print_hex(rb, 4);
    uart_print("\n");

    if (rb == test_val) {
        uart_print("RESULT: PASS\n");
        console_print("\n*** XSPI test PASSED ***\n");
        return 0;
    }

    uart_print("RESULT: FAIL\n");
    console_print("\n*** XSPI long_polling test FAILED ***\n");
    return 1;
}
