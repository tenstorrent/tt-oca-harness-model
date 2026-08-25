// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
// AES library functions
//-----------------------------------------------------------
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "test_completion.h"
#include "och_sep_top_reg.h"
#include "och_sep_common.h"

static int wait_for_idle(void) {
    int timeout = 1000000;
    while (timeout-- > 0) {
        AES_STATUS_reg_u status = {.val = READ_REG(AES_STATUS_REG_ADDR)};
        if (status.f.idle) {
            return 0;
        }
    }
    printf("ERROR: Timeout waiting for AES idle\n");
    return -1;
}

static int wait_for_input_ready(void) {
    int timeout = 1000000;
    while (timeout-- > 0) {
        AES_STATUS_reg_u status = {.val = READ_REG(AES_STATUS_REG_ADDR)};
        if (status.f.input_ready) {
            return 0;
        }
    }
    printf("ERROR: Timeout waiting for AES input ready\n");
    return -1;
}

static int wait_for_output_valid(void) {
    int timeout = 1000000;
    while (timeout-- > 0) {
        AES_STATUS_reg_u status = {.val = READ_REG(AES_STATUS_REG_ADDR)};
        if (status.f.output_valid) {
            return 0;
        }
    }
    printf("ERROR: Timeout waiting for AES output valid\n");
    return -1;
}

static void print_status(const char *tag) {
    AES_STATUS_reg_u s = {.val = READ_REG(AES_STATUS_REG_ADDR)};
    printf("%s STATUS=0x%08x idle=%u stall=%u input_ready=%u output_valid=%u\n",
           tag, s.val, s.f.idle, s.f.stall, s.f.input_ready, s.f.output_valid);
}

static uint32_t swap_bytes_uint32(uint32_t val) {
    return ((val << 24) & 0xFF000000) | // byte 0 --> byte 3
           ((val <<  8) & 0x00FF0000) | // byte 1 --> byte 2
           ((val >>  8) & 0x0000FF00) | // byte 2 --> byte 1
           ((val >> 24) & 0x000000FF);  // byte 3 --> byte 0
}

static void print_registers() {
    uint32_t regval;
    printf("AES CSRs @ 0x%08x -------------------------------\n", AES_REG_MAP_BASE_ADDR);
    printf("AES_STATUS_REG:                 0x%08x\n", READ_REG(AES_STATUS_REG_ADDR));
    printf("AES_CTRL_SHADOWED_REG_ADDR:     0x%08x\n", READ_REG(AES_CTRL_SHADOWED_REG_ADDR));
    printf("AES_CTRL_AUX_SHADOWED_REG_ADDR: 0x%08x\n", READ_REG(AES_CTRL_AUX_SHADOWED_REG_ADDR));
    printf("AES_CTRL_AUX_REGWEN_REG_ADDR:   0x%08x\n", READ_REG(AES_CTRL_AUX_REGWEN_REG_ADDR));
    printf("AES_TRIGGER_REG_ADDR:           0x%08x\n", READ_REG(AES_TRIGGER_REG_ADDR));
    printf("\nINFO: key share registers are write-only access, so expect reads to return 0x00000000\n");
    for (int i=0; i<4; i++) {
      regval = READ_REG(AES_KEY_SHARE1_0__REG_ADDR + 4*i);
      printf("AES_KEY_SHARE0[%d]:              0x%08x\n", i, regval);
    }
    for (int i=0; i<4; i++) {
      regval = READ_REG(AES_KEY_SHARE0_0__REG_ADDR + 4*i);
      printf("AES_KEY_SHARE1[%d]:              0x%08x\n", i, regval);
    }
    printf("\n");
    for (int i=0; i<4; i++) {
      regval = READ_REG(AES_IV_0__REG_ADDR + 4*i);
      printf("AES_IV[%d]:                      0x%08x\n", i, regval);
    }
    printf("\nINFO: data input registers are write-only access, so expect reads to return 0x00000000\n");    
    for (int i=0; i<4; i++) {
      regval = READ_REG(AES_DATA_IN_0__REG_ADDR + 4*i);
      printf("AES_DATA_IN[%d]:                 0x%08x\n", i, regval);
    }
    printf("\n");    
    for (int i=0; i<4; i++) {
      regval = READ_REG(AES_DATA_OUT_0__REG_ADDR + 4*i);
      printf("AES_DATA_OUT[%d]:                0x%08x\n", i, regval);
    }
    printf("DEBUG: byte endian swapped data out\n");
    for (int i=0; i<4; i++) {
      regval = READ_REG(AES_DATA_OUT_0__REG_ADDR + 4*i);      
      printf("AES_DATA_OUT[%d]:                0x%08x\n", i, swap_bytes_uint32(regval));
    }
}
