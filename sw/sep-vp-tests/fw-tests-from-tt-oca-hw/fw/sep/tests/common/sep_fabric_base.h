/*
 * SEP Fabric Base Header
 * Basic definitions and functions for fabric testing
 */

#ifndef SEP_FABRIC_BASE_H
#define SEP_FABRIC_BASE_H

// Fabric test definitions
#define AP_CHANNEL              0
#define STEE_CHANNEL           1

#define AXI_READ               0
#define AXI_WRITE              1

#define CACHE_ATTR_DEVICE      0x00
#define CACHE_ATTR_NORMAL_NC   0x04
#define CACHE_ATTR_NORMAL_WT   0x08
#define CACHE_ATTR_NORMAL_WB   0x0C
#define CACHE_ATTR_WRITEBACK   CACHE_ATTR_NORMAL_WB
#define CACHE_ATTR_INSTRUCTION 0x02

// Basic fabric function stubs for compilation
static inline int init_sep_fabric(void) {
    return 0;
}

static inline void sep_delay_cycles(uint32_t cycles) {
    for (volatile uint32_t i = 0; i < cycles * 100; i++) {
        __asm__ __volatile__("nop");
    }
}

static inline int setup_output_remap_region(int region, uint32_t src_start, uint32_t dest_start, int enable, int channel) {
    // Stub implementation
    return 0;
}

static inline int test_axi_transaction(uint32_t addr, uint32_t size, int type) {
    // Stub implementation
    return 0;
}

// More stub functions for compilation success
static inline int setup_output_remap_region_extended(int region, uint32_t src, uint32_t dest, int enable, int channel, uint32_t mask, uint32_t attr) { return 0; }
static inline int read_output_remap_reg(int region, uint32_t offset, uint32_t *value) { *value = 0xDEADBEEF; return 0; }
static inline int toggle_output_remap_region_enable(int region) { return 0; }
static inline int toggle_output_remap_channel(int region) { return 0; }
static inline int lock_output_remap_region(int region) { return 0; }
static inline int unlock_output_remap_region(int region) { return 0; }
static inline int reset_output_remap_region(int region) { return 0; }
static inline int write_output_remap_reg(int region, uint32_t offset, uint32_t value) { return 0; }

// Offset definitions
#define OUTPUT_REMAP_SRC_ADDR_LOW_OFFSET  0x00
#define OUTPUT_REMAP_CTRL_OFFSET          0x10
#define OUTPUT_REMAP_STATUS_OFFSET        0x14

static inline uint32_t get_cycle_count(void) {
    uint32_t cycles;
    __asm__ volatile ("rdcycle %0" : "=r"(cycles));
    return cycles;
}

static inline uint32_t get_system_frequency(void) {
    return 100000000; // 100 MHz
}

#endif // SEP_FABRIC_BASE_H