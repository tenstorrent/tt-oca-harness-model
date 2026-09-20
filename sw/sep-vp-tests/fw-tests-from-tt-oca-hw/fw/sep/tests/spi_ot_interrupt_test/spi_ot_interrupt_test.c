// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
/*
 * SPI OT Interrupt Test - TC_SPIOT_011 (P0)
 *
 * Verifies interrupt generation/masking for error and event classes,
 * INTR_TEST register forcing, and EVENT_ENABLE individual control.
 *
 * Test Flow:
 *   1. Configure SPI mux, enable controller
 *   2. Test INTR_ENABLE defaults and write-readback
 *   3. INTR_TEST injection → verify INTR_STATUS asserts/deasserts (ERROR and SPI_EVENT)
 *   3.5. Real UNDERFLOW event → INTR_STATUS.error functional path
 *   4. EVENT_ENABLE configuration write-readback
 *   4.5. Real TXEMPTY event → INTR_STATUS.spi_event functional path
 *   4.6. EVENT_ENABLE.ready / .idle functional path
 *   5. Masking test: INTR_ENABLE=0 blocks INTR_TEST injection (ERROR and SPI_EVENT)
 *
 * Execution:
 *   make test-sep TEST_NAME=sep_spi_ot_interrupt_test STACK=sim
 *
 * Copyright 2025 Tenstorrent Inc.
 */

#include <stdint.h>
#include <stdio.h>
#include "och_sep_top_reg.h"
#include "och_sep_common.h"
#include "test_completion.h"
#include "sep_outbound_filter.h"

static void configure_spi_mux_ot(void)
{
    OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_reg_u spi_mux = {
        .val = OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_DEFAULT
    };
    spi_mux.f.spi_sel = 1;
    spi_mux.f.cs_force_high = 0;
    WRITE_REG(SEP_EXTERNAL_OCH_SEP_SPI_MUX_CTRL_SPI_MUX_CTRL_REG_ADDR, spi_mux.val);
}

static int check_reg(const char *name, uint32_t actual, uint32_t expected)
{
    int ok = (actual == expected);
    printf("  %s: 0x%08x (expected 0x%08x) - %s\n",
           name, actual, expected, ok ? "PASS" : "FAIL");
    return ok;
}

int main(void)
{
    sep_outbound_filter_init();

    printf("\n========================================\n");
    printf("SPI OT Interrupt Test (TC_SPIOT_011)\n");
    printf("========================================\n\n");

    int pass = 1;
    SPI_CONTROLLER_INTR_STATE_reg_u intr_status;
    SPI_CONTROLLER_INTR_ENABLE_reg_u intr_enable;
    SPI_CONTROLLER_INTR_TEST_reg_u intr_test;
    SPI_CONTROLLER_EVENT_ENABLE_reg_u event_enable;
    SPI_CONTROLLER_ERROR_STATUS_reg_u err_status;
    SPI_CONTROLLER_CONTROL_reg_u ctrl;
    uint32_t dummy_rx;

    configure_spi_mux_ot();
    printf("SPI mux configured for OpenTitan\n");

    /* Enable controller (required for event signals to be valid) */
    ctrl.val = SPI_CONTROLLER_CONTROL_REG_DEFAULT;
    ctrl.f.spien     = 1;
    ctrl.f.output_en = 1;
    WRITE_REG(SPI_CONTROLLER_CONTROL_REG_ADDR, ctrl.val);

    /* Step 1: INTR_STATUS default */
    printf("\nStep 1: INTR_STATUS default\n");
    intr_status.val = READ_REG(SPI_CONTROLLER_INTR_STATE_REG_ADDR);
    printf("  INTR_STATUS=0x%08x (ERROR=%u, SPI_EVENT=%u)\n",
           intr_status.val, intr_status.f.error, intr_status.f.spi_event);

    /* Step 2: INTR_ENABLE write-readback */
    printf("\nStep 2: INTR_ENABLE write-readback\n");
    intr_enable.val = READ_REG(SPI_CONTROLLER_INTR_ENABLE_REG_ADDR);
    if (!check_reg("INTR_ENABLE default", intr_enable.val, 0)) pass = 0;

    intr_enable.val = 0;
    intr_enable.f.error = 1;
    intr_enable.f.spi_event = 1;
    WRITE_REG(SPI_CONTROLLER_INTR_ENABLE_REG_ADDR, intr_enable.val);
    intr_enable.val = READ_REG(SPI_CONTROLLER_INTR_ENABLE_REG_ADDR);
    if (!check_reg("INTR_ENABLE.error", intr_enable.f.error, 1)) pass = 0;
    if (!check_reg("INTR_ENABLE SPI_EVENT", intr_enable.f.spi_event, 1)) pass = 0;

    /* Step 3: INTR_TEST forcing — verify INTR_TEST injection sets INTR_STATUS
     * RTL: error_intr = (ERROR_STATUS.intr || INTR_TEST.ERROR.value) && INTR_ENABLE.ERROR.value
     *      INTR_STATUS.ERROR.next = error_intr  (spi_controller.sv:363-365)
     * INTR_ENABLE.error is already set from Step 2.
     */
    printf("\nStep 3: INTR_TEST forcing → INTR_STATUS assertion\n");
    intr_test.val = 0;
    intr_test.f.error = 1;
    WRITE_REG(SPI_CONTROLLER_INTR_TEST_REG_ADDR, intr_test.val);
    intr_status.val = READ_REG(SPI_CONTROLLER_INTR_STATE_REG_ADDR);
    if (!check_reg("INTR_STATUS.error=1 via INTR_TEST injection", intr_status.f.error, 1)) pass = 0;

    /* Release INTR_TEST — INTR_STATUS should deassert (no real error active) */
    intr_test.val = 0;
    WRITE_REG(SPI_CONTROLLER_INTR_TEST_REG_ADDR, intr_test.val);
    intr_status.val = READ_REG(SPI_CONTROLLER_INTR_STATE_REG_ADDR);
    if (!check_reg("INTR_STATUS.error=0 after INTR_TEST release", intr_status.f.error, 0)) pass = 0;

    intr_test.val = 0;
    intr_test.f.spi_event = 1;
    WRITE_REG(SPI_CONTROLLER_INTR_TEST_REG_ADDR, intr_test.val);
    intr_status.val = READ_REG(SPI_CONTROLLER_INTR_STATE_REG_ADDR);
    if (!check_reg("INTR_STATUS.spi_event=1 via INTR_TEST injection", intr_status.f.spi_event, 1)) pass = 0;

    /* Release INTR_TEST — INTR_STATUS should deassert */
    intr_test.val = 0;
    WRITE_REG(SPI_CONTROLLER_INTR_TEST_REG_ADDR, intr_test.val);
    intr_status.val = READ_REG(SPI_CONTROLLER_INTR_STATE_REG_ADDR);
    if (!check_reg("INTR_STATUS.spi_event=0 after INTR_TEST release", intr_status.f.spi_event, 0)) pass = 0;

    /* Step 3.5: INTR_STATUS.error functional verification via UNDERFLOW */
    printf("\nStep 3.5: INTR_STATUS.error via UNDERFLOW\n");
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, 0xFFFFFFFF);  /* clear */
    dummy_rx = READ_REG(SPI_CONTROLLER_RXDATA_REG_ADDR);          /* trigger UNDERFLOW */
    (void)dummy_rx;
    err_status.val = READ_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR);
    if (!check_reg("UNDERFLOW triggered", err_status.f.underflow, 1)) pass = 0;
    intr_status.val = READ_REG(SPI_CONTROLLER_INTR_STATE_REG_ADDR);
    if (!check_reg("INTR_STATUS.error=1 on underflow", intr_status.f.error, 1)) pass = 0;
    /* W1C clear ERROR_STATUS → INTR_STATUS.error should deassert */
    WRITE_REG(SPI_CONTROLLER_ERROR_STATUS_REG_ADDR, err_status.val);
    intr_status.val = READ_REG(SPI_CONTROLLER_INTR_STATE_REG_ADDR);
    if (!check_reg("INTR_STATUS.error=0 after clear", intr_status.f.error, 0)) pass = 0;

    /* Step 4: EVENT_ENABLE configuration */
    printf("\nStep 4: EVENT_ENABLE configuration\n");
    event_enable.val = READ_REG(SPI_CONTROLLER_EVENT_ENABLE_REG_ADDR);
    if (!check_reg("EVENT_ENABLE default", event_enable.val, 0)) pass = 0;

    /* Step 4.5: INTR_STATUS.spi_event via TXEMPTY event */
    printf("\nStep 4.5: INTR_STATUS.spi_event via TXEMPTY\n");
    event_enable.val = 0;
    event_enable.f.txempty = 1;
    WRITE_REG(SPI_CONTROLLER_EVENT_ENABLE_REG_ADDR, event_enable.val);
    intr_status.val = READ_REG(SPI_CONTROLLER_INTR_STATE_REG_ADDR);
    if (!check_reg("INTR_STATUS.spi_event=1 (txempty)", intr_status.f.spi_event, 1)) pass = 0;
    /* Disable txempty → spi_event should deassert */
    event_enable.val = 0;
    WRITE_REG(SPI_CONTROLLER_EVENT_ENABLE_REG_ADDR, event_enable.val);
    intr_status.val = READ_REG(SPI_CONTROLLER_INTR_STATE_REG_ADDR);
    if (!check_reg("INTR_STATUS.spi_event=0 (no events)", intr_status.f.spi_event, 0)) pass = 0;

    /* Step 4.6: EVENT_ENABLE.ready and .idle functional path
     * SPI is in idle state (SPIEN=1, no transaction), so ready and idle events
     * should be asserted immediately when their enables are set.
     */
    printf("\nStep 4.6: EVENT_ENABLE.ready/.idle functional path\n");
    event_enable.val = 0;
    event_enable.f.ready = 1;
    WRITE_REG(SPI_CONTROLLER_EVENT_ENABLE_REG_ADDR, event_enable.val);
    intr_status.val = READ_REG(SPI_CONTROLLER_INTR_STATE_REG_ADDR);
    if (!check_reg("INTR_STATUS.spi_event=1 (ready event)", intr_status.f.spi_event, 1)) pass = 0;
    event_enable.val = 0;
    WRITE_REG(SPI_CONTROLLER_EVENT_ENABLE_REG_ADDR, event_enable.val);
    intr_status.val = READ_REG(SPI_CONTROLLER_INTR_STATE_REG_ADDR);
    if (!check_reg("INTR_STATUS.spi_event=0 after ready disable", intr_status.f.spi_event, 0)) pass = 0;

    event_enable.val = 0;
    event_enable.f.idle = 1;
    WRITE_REG(SPI_CONTROLLER_EVENT_ENABLE_REG_ADDR, event_enable.val);
    intr_status.val = READ_REG(SPI_CONTROLLER_INTR_STATE_REG_ADDR);
    if (!check_reg("INTR_STATUS.spi_event=1 (idle event)", intr_status.f.spi_event, 1)) pass = 0;
    event_enable.val = 0;
    WRITE_REG(SPI_CONTROLLER_EVENT_ENABLE_REG_ADDR, event_enable.val);
    intr_status.val = READ_REG(SPI_CONTROLLER_INTR_STATE_REG_ADDR);
    if (!check_reg("INTR_STATUS.spi_event=0 after idle disable", intr_status.f.spi_event, 0)) pass = 0;

    /* Enable all events for write-readback verification */
    event_enable.val = 0;
    event_enable.f.rxfull = 1;
    event_enable.f.txempty = 1;
    event_enable.f.rxwm = 1;
    event_enable.f.txwm = 1;
    event_enable.f.ready = 1;
    event_enable.f.idle = 1;
    WRITE_REG(SPI_CONTROLLER_EVENT_ENABLE_REG_ADDR, event_enable.val);
    event_enable.val = READ_REG(SPI_CONTROLLER_EVENT_ENABLE_REG_ADDR);
    if (!check_reg("RXFULL enable", event_enable.f.rxfull, 1)) pass = 0;
    if (!check_reg("TXEMPTY enable", event_enable.f.txempty, 1)) pass = 0;
    if (!check_reg("RXWM enable", event_enable.f.rxwm, 1)) pass = 0;
    if (!check_reg("TXWM enable", event_enable.f.txwm, 1)) pass = 0;
    if (!check_reg("READY enable", event_enable.f.ready, 1)) pass = 0;
    if (!check_reg("IDLE enable", event_enable.f.idle, 1)) pass = 0;

    /* Step 5: Masking test — INTR_ENABLE=0 blocks INTR_TEST injection from reaching INTR_STATUS
     * RTL: error_intr = (... || INTR_TEST.ERROR.value) && INTR_ENABLE.ERROR.value
     * When INTR_ENABLE.error=0, INTR_STATUS.error must remain 0 even if INTR_TEST.error=1.
     */
    printf("\nStep 5: Masking test (INTR_ENABLE=0 blocks INTR_TEST)\n");
    intr_enable.val = 0;
    WRITE_REG(SPI_CONTROLLER_INTR_ENABLE_REG_ADDR, intr_enable.val);
    intr_enable.val = READ_REG(SPI_CONTROLLER_INTR_ENABLE_REG_ADDR);
    if (!check_reg("INTR_ENABLE all disabled", intr_enable.val, 0)) pass = 0;

    /* Force INTR_TEST.error=1 with INTR_ENABLE.error=0 — INTR_STATUS must stay 0 */
    intr_test.val = 0;
    intr_test.f.error = 1;
    WRITE_REG(SPI_CONTROLLER_INTR_TEST_REG_ADDR, intr_test.val);
    intr_status.val = READ_REG(SPI_CONTROLLER_INTR_STATE_REG_ADDR);
    if (!check_reg("INTR_STATUS.error=0 when INTR_ENABLE.error=0 (masked)", intr_status.f.error, 0)) pass = 0;

    /* Release INTR_TEST */
    intr_test.val = 0;
    WRITE_REG(SPI_CONTROLLER_INTR_TEST_REG_ADDR, intr_test.val);

    /* Step 5b: spi_event masking — INTR_ENABLE.spi_event=0 blocks INTR_TEST.spi_event
     * INTR_ENABLE is still 0 from step 5.
     */
    printf("\nStep 5b: spi_event masking (INTR_ENABLE.spi_event=0)\n");
    intr_test.val = 0;
    intr_test.f.spi_event = 1;
    WRITE_REG(SPI_CONTROLLER_INTR_TEST_REG_ADDR, intr_test.val);
    intr_status.val = READ_REG(SPI_CONTROLLER_INTR_STATE_REG_ADDR);
    if (!check_reg("INTR_STATUS.spi_event=0 when INTR_ENABLE.spi_event=0 (masked)", intr_status.f.spi_event, 0)) pass = 0;
    intr_test.val = 0;
    WRITE_REG(SPI_CONTROLLER_INTR_TEST_REG_ADDR, intr_test.val);

    /* Disable all events */
    event_enable.val = 0;
    WRITE_REG(SPI_CONTROLLER_EVENT_ENABLE_REG_ADDR, event_enable.val);
    event_enable.val = READ_REG(SPI_CONTROLLER_EVENT_ENABLE_REG_ADDR);
    if (!check_reg("EVENT_ENABLE all disabled", event_enable.val, 0)) pass = 0;

    printf("\n========================================\n");
    if (pass) {
        printf("=== SPI OT INTERRUPT TEST PASSED ===\n");
        test_pass(0);
    } else {
        printf("=== SPI OT INTERRUPT TEST FAILED ===\n");
        test_fail(0);
    }
    printf("========================================\n");

    while (1) { __asm__("wfi"); }
    return pass ? 0 : -1;
}
