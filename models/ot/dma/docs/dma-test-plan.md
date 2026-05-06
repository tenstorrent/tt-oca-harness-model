# DMA Controller - SystemC TLM2.0 Test Plan

## Document Metadata

| Field | Value |
|-------|-------|
| Document Title | DMA Controller Test Plan |
| IP Name | dma |
| Version | 1.0 |
| Date | 2026-02-10 |
| Purpose | SystemC TLM Model Verification and Validation |
| Target Audience | Verification Engineers, SystemC/TLM Model Developers |

---

## Table of Contents

1. [Introduction](#introduction)
2. [Test Coverage Strategy](#test-coverage-strategy)
3. [Test Plan Table](#test-plan-table)
4. [Test Categories](#test-categories)

---

## Introduction

This document provides a comprehensive test plan for the DMA Controller SystemC TLM2.0 model. The test plan covers all functional features, register access patterns, error conditions, security enforcement mechanisms, and corner cases. Each test case is designed to verify software-visible behavior at the transaction level without requiring cycle-accurate timing.

### Coverage Objectives

- **Register Access**: Validate all register read/write operations, access types (RO/RW/WO/RW1C/RW0C), reset values, and reserved field behavior
- **Transfer Operations**: Test all data transfer modes, sizes, widths, and addressing modes
- **Security Features**: Verify memory range validation, ASID-based access control, and three-tier memory model enforcement
- **Hardware Handshaking**: Validate autonomous peripheral FIFO servicing with 11 trigger sources
- **Inline SHA-2 Hashing**: Test SHA-256/384/512 computation during data transfers
- **Interrupts**: Verify assertion/deassertion conditions for all three interrupt types
- **Error Handling**: Test all 8 error conditions and recovery mechanisms
- **Register Locking**: Validate CFG_REGWEN and RANGE_REGWEN protection mechanisms
- **Corner Cases**: Test boundary conditions, abort scenarios, and edge cases

---

## Test Coverage Strategy

### Register Coverage
- Reset value verification for all 66 registers
- Read-only, Write-only, Read-Write, RW1C, RW0C access type validation
- Reserved field behavior (reads return 0, writes ignored)
- Register locking by CFG_REGWEN and RANGE_REGWEN

### Functional Coverage
- All transfer modes (Memory-to-Memory, Memory-to-Peripheral, Peripheral-to-Memory)
- All addressing modes (Increment, Fixed, Wrap) for source and destination independently
- All transfer widths (1-byte, 2-byte, 4-byte)
- Single-chunk and multi-chunk transfers
- Hardware handshake mode with all 11 trigger sources
- Inline SHA-2 hashing (SHA-256, SHA-384, SHA-512)

### Security Coverage
- Three-tier memory model (OT Private, OT DMA-enabled, SoC memory)
- All valid ASID combinations (0x7=OT_ADDR, 0x9=SYS_ADDR, 0xA=SOC_ADDR)
- Memory range validation for cross-boundary transfers
- RANGE_REGWEN and CFG_REGWEN locking mechanisms

### Error Coverage
- Configuration errors (8 types: src_addr_error, dst_addr_error, opcode_error, size_error, bus_error, base_limit_error, range_valid_error, asid_error)
- Error reporting and recovery procedures

### Interrupt Coverage
- dma_done interrupt (transfer completion)
- dma_chunk_done interrupt (chunk completion)
- dma_error interrupt (error conditions)

---

## Test Plan Table

| Sl. No. | TestCase Name | Description | Registers Programmed | Ports/Signals Used | Test Type |
| ------- | ------------- | ----------- | -------------------- | ------------------ | --------- |
| **1. Register Access Tests** |
| 1 | test_reset_values | Verify all registers return correct reset values after reset assertion | All 66 registers | rst_ni, reg_target_socket | Positive |
| 2 | test_intr_state_read_only | Verify INTR_STATE register is read-only and write attempts are ignored | INTR_STATE | reg_target_socket | Positive |
| 3 | test_intr_enable_read_write | Verify INTR_ENABLE register supports read/write operations for bits[2:0] | INTR_ENABLE | reg_target_socket | Positive |
| 4 | test_intr_test_write_only | Verify INTR_TEST register is write-only and forces interrupt state bits | INTR_TEST, INTR_STATE | reg_target_socket, dma_done_intr, dma_chunk_done_intr, dma_error_intr | Positive |
| 5 | test_alert_test_write_only | Verify ALERT_TEST register write triggers fatal_fault alert | ALERT_TEST | reg_target_socket | Positive |
| 6 | test_control_abort_write_only | Verify CONTROL.abort bit is write-only and always reads as 0 | CONTROL | reg_target_socket | Positive |
| 7 | test_status_rw1c_clear | Verify STATUS register bits clear on write-1-to-clear (done, aborted, error, chunk_done) | STATUS | reg_target_socket | Positive |
| 8 | test_cfg_regwen_read_only | Verify CFG_REGWEN is read-only and reflects DMA busy/idle state (0x0=locked, 0x6=unlocked) | CFG_REGWEN, CONTROL, STATUS | reg_target_socket | Positive |
| 9 | test_range_regwen_write_lock | Verify RANGE_REGWEN write-0-to-lock permanently locks memory range registers until reset | RANGE_REGWEN, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID | reg_target_socket | Positive |
| 10 | test_reserved_bits_read_zero | Verify reserved bits in all registers read as zero | All registers with reserved fields | reg_target_socket | Positive |
| 11 | test_reserved_bits_write_ignored | Verify writes to reserved bits are ignored without side effects | All registers with reserved fields | reg_target_socket | Positive |
| 12 | test_cfg_regwen_locked_registers | Verify configuration registers are read-only when CFG_REGWEN=0x0 (DMA busy) | SRC_ADDR_LO, SRC_ADDR_HI, DST_ADDR_LO, DST_ADDR_HI, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket | Positive |
| 13 | test_control_status_always_accessible | Verify CONTROL and STATUS registers remain accessible during transfer (not locked by CFG_REGWEN) | CONTROL, STATUS, CFG_REGWEN | reg_target_socket | Positive |
| **2. Basic Transfer Operations** |
| 14 | test_mem_to_mem_single_chunk_4byte | Simple memory-to-memory transfer, single chunk, 4-byte width, incrementing addresses | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 15 | test_mem_to_mem_single_chunk_2byte | Memory-to-memory transfer, single chunk, 2-byte width, incrementing addresses | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 16 | test_mem_to_mem_single_chunk_1byte | Memory-to-memory transfer, single chunk, 1-byte width, incrementing addresses | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 17 | test_mem_to_mem_multi_chunk | Memory-to-memory transfer with multiple chunks, verify chunk_done interrupts | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, INTR_ENABLE | reg_target_socket, ot_initiator_socket, dma_chunk_done_intr, dma_done_intr | Positive |
| 18 | test_mem_to_mem_ctn_32bit | Memory-to-memory transfer using CTN interface (32-bit address) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ctn_initiator_socket, dma_done_intr | Positive |
| 19 | test_mem_to_mem_ctn_64bit | Memory-to-memory transfer using CTN interface (64-bit address) | SRC_ADDR_LO, SRC_ADDR_HI, DST_ADDR_LO, DST_ADDR_HI, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ctn_initiator_socket, dma_done_intr | Positive |
| 20 | test_mem_to_mem_sys_64bit | Memory-to-memory transfer using System bus (64-bit address) | SRC_ADDR_LO, SRC_ADDR_HI, DST_ADDR_LO, DST_ADDR_HI, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, sys_initiator_socket, dma_done_intr | Positive |
| 21 | test_transfer_size_16bytes | Transfer with TOTAL_DATA_SIZE=16 bytes, CHUNK_DATA_SIZE=16 bytes | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 22 | test_transfer_size_1024bytes | Transfer with TOTAL_DATA_SIZE=1024 bytes, CHUNK_DATA_SIZE=256 bytes | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 23 | test_transfer_size_4096bytes | Transfer with TOTAL_DATA_SIZE=4096 bytes, CHUNK_DATA_SIZE=512 bytes | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| **3. Addressing Modes** |
| 24 | test_src_increment_dst_increment | Source and destination both use incrementing addressing mode | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 25 | test_src_fixed_dst_increment | Source uses fixed address (FIFO-like), destination uses incrementing | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 26 | test_src_increment_dst_fixed | Source uses incrementing, destination uses fixed address (FIFO-like) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 27 | test_src_fixed_dst_fixed | Both source and destination use fixed address mode | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 28 | test_src_wrap_mode | Source uses wrap/circular buffer mode (increment=1, wrap=1) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 29 | test_dst_wrap_mode | Destination uses wrap/circular buffer mode (increment=1, wrap=1) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 30 | test_both_wrap_mode | Both source and destination use wrap mode | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| **4. Hardware Handshaking Mode** |
| 31 | test_hw_handshake_trigger0 | Hardware handshake mode with lsio_trigger[0] for I2C RX FIFO | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[0], ot_initiator_socket, dma_done_intr | Positive |
| 32 | test_hw_handshake_trigger1 | Hardware handshake mode with lsio_trigger[1] for I2C TX FIFO | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[1], ot_initiator_socket, dma_done_intr | Positive |
| 33 | test_hw_handshake_trigger2 | Hardware handshake mode with lsio_trigger[2] for UART RX FIFO | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[2], ot_initiator_socket, dma_done_intr | Positive |
| 34 | test_hw_handshake_trigger3 | Hardware handshake mode with lsio_trigger[3] for UART TX FIFO | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[3], ot_initiator_socket, dma_done_intr | Positive |
| 35 | test_hw_handshake_trigger4 | Hardware handshake mode with lsio_trigger[4] for SPI Device RX FIFO | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[4], ot_initiator_socket, dma_done_intr | Positive |
| 36 | test_hw_handshake_trigger5 | Hardware handshake mode with lsio_trigger[5] for SPI Device TX FIFO | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[5], ot_initiator_socket, dma_done_intr | Positive |
| 37 | test_hw_handshake_trigger6 | Hardware handshake mode with lsio_trigger[6] for SPI Host RX FIFO | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[6], ot_initiator_socket, dma_done_intr | Positive |
| 38 | test_hw_handshake_trigger7 | Hardware handshake mode with lsio_trigger[7] for SPI Host TX FIFO | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[7], ot_initiator_socket, dma_done_intr | Positive |
| 39 | test_hw_handshake_trigger8 | Hardware handshake mode with lsio_trigger[8] for peripheral FIFO | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[8], ot_initiator_socket, dma_done_intr | Positive |
| 40 | test_hw_handshake_trigger9 | Hardware handshake mode with lsio_trigger[9] for peripheral FIFO | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[9], ot_initiator_socket, dma_done_intr | Positive |
| 41 | test_hw_handshake_trigger10 | Hardware handshake mode with lsio_trigger[10] for peripheral FIFO | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[10], ot_initiator_socket, dma_done_intr | Positive |
| 42 | test_hw_handshake_auto_clear_ot_bus | Hardware handshake with automatic interrupt clearing on OT-internal bus | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CLEAR_INTR_SRC, CLEAR_INTR_BUS, INTR_SRC_ADDR_0, INTR_SRC_WR_VAL_0, CONTROL, STATUS | reg_target_socket, lsio_trigger[0], ot_initiator_socket, dma_done_intr | Positive |
| 43 | test_hw_handshake_auto_clear_ctn_bus | Hardware handshake with automatic interrupt clearing on CTN/System bus | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CLEAR_INTR_SRC, CLEAR_INTR_BUS, INTR_SRC_ADDR_0, INTR_SRC_WR_VAL_0, CONTROL, STATUS | reg_target_socket, lsio_trigger[0], ctn_initiator_socket, dma_done_intr | Positive |
| 44 | test_hw_handshake_go_bit_remains_set | Verify go bit remains set in hardware handshake mode after transfer completion | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[0], ot_initiator_socket, dma_done_intr | Positive |
| 45 | test_hw_handshake_no_chunk_done_intr | Verify chunk_done interrupt is not generated in hardware handshake mode | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[0], ot_initiator_socket, dma_chunk_done_intr, dma_done_intr | Positive |
| **5. Inline SHA-2 Hashing** |
| 46 | test_inline_sha256_single_chunk | Inline SHA-256 hashing with single-chunk transfer | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, SHA2_DIGEST_0-7 | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 47 | test_inline_sha384_single_chunk | Inline SHA-384 hashing with single-chunk transfer | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, SHA2_DIGEST_0-11 | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 48 | test_inline_sha512_single_chunk | Inline SHA-512 hashing with single-chunk transfer | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, SHA2_DIGEST_0-15 | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 49 | test_inline_sha256_multi_chunk | Inline SHA-256 hashing across multiple chunks with state preservation | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, SHA2_DIGEST_0-7 | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 50 | test_inline_sha384_multi_chunk | Inline SHA-384 hashing across multiple chunks with state preservation | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, SHA2_DIGEST_0-11 | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 51 | test_inline_sha512_multi_chunk | Inline SHA-512 hashing across multiple chunks with state preservation | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, SHA2_DIGEST_0-15 | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 52 | test_initial_transfer_bit_hash_reset | Verify initial_transfer=1 resets SHA-2 hash state for new computation | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, SHA2_DIGEST_0-7 | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 53 | test_digest_swap_endianness | Verify digest_swap bit controls endianness conversion of digest output | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, SHA2_DIGEST_0-7 | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 54 | test_sha2_digest_valid_bit | Verify sha2_digest_valid bit is set when digest is ready | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| **6. Security Features and Memory Isolation** |
| 55 | test_ot_private_to_ot_private | Transfer from OT Private Memory to OT Private Memory (allowed) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 56 | test_ot_private_to_ot_dma_enabled | Transfer from OT Private Memory to OT DMA-enabled Memory (allowed) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 57 | test_ot_dma_enabled_to_ot_private | Transfer from OT DMA-enabled Memory to OT Private Memory (allowed) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 58 | test_ot_dma_enabled_to_soc | Transfer from OT DMA-enabled Memory to SoC Memory (allowed) | SRC_ADDR_LO, DST_ADDR_LO, DST_ADDR_HI, ADDR_SPACE_ID, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, sys_initiator_socket, dma_done_intr | Positive |
| 59 | test_soc_to_ot_dma_enabled | Transfer from SoC Memory to OT DMA-enabled Memory (allowed) | SRC_ADDR_LO, SRC_ADDR_HI, DST_ADDR_LO, ADDR_SPACE_ID, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, sys_initiator_socket, ot_initiator_socket, dma_done_intr | Positive |
| 60 | test_soc_to_soc | Transfer from SoC Memory to SoC Memory (allowed) | SRC_ADDR_LO, SRC_ADDR_HI, DST_ADDR_LO, DST_ADDR_HI, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, sys_initiator_socket, dma_done_intr | Positive |
| 61 | test_asid_ot_addr_validation | Verify ASID=0x7 (OT_ADDR) routes to ot_initiator_socket | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 62 | test_asid_soc_addr_validation | Verify ASID=0xA (SOC_ADDR) routes to ctn_initiator_socket | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ctn_initiator_socket, dma_done_intr | Positive |
| 63 | test_asid_sys_addr_validation | Verify ASID=0x9 (SYS_ADDR) routes to sys_initiator_socket | SRC_ADDR_LO, SRC_ADDR_HI, DST_ADDR_LO, DST_ADDR_HI, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, sys_initiator_socket, dma_done_intr | Positive |
| 64 | test_memory_range_base_limit_config | Configure and verify memory range base and limit registers | ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID | reg_target_socket | Positive |
| 65 | test_range_valid_bit_requirement | Verify RANGE_VALID must be set before cross-boundary transfers | ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, dma_error_intr | Positive |
| **7. Interrupt Generation and Clearing** |
| 66 | test_dma_done_interrupt_assert | Verify dma_done interrupt asserts on transfer completion | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, INTR_ENABLE, CONTROL, STATUS, INTR_STATE | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 67 | test_dma_done_interrupt_clear_rw1c | Verify dma_done interrupt clears on write-1 to STATUS.done | STATUS, INTR_STATE | reg_target_socket, dma_done_intr | Positive |
| 68 | test_dma_done_auto_clear_on_new_transfer | Verify dma_done bit auto-clears when new transfer starts (go bit set) | CONTROL, STATUS, INTR_STATE | reg_target_socket, dma_done_intr | Positive |
| 69 | test_dma_chunk_done_interrupt_assert | Verify dma_chunk_done interrupt asserts on chunk completion | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, INTR_ENABLE, CONTROL, STATUS, INTR_STATE | reg_target_socket, ot_initiator_socket, dma_chunk_done_intr | Positive |
| 70 | test_dma_chunk_done_interrupt_clear | Verify dma_chunk_done interrupt clears on write-1 to STATUS.chunk_done | STATUS, INTR_STATE | reg_target_socket, dma_chunk_done_intr | Positive |
| 71 | test_dma_chunk_done_auto_clear | Verify dma_chunk_done bit auto-clears when next chunk starts | CONTROL, STATUS, INTR_STATE | reg_target_socket, dma_chunk_done_intr | Positive |
| 72 | test_dma_error_interrupt_assert | Verify dma_error interrupt asserts on error conditions | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, INTR_ENABLE, CONTROL, STATUS, ERROR_CODE, INTR_STATE | reg_target_socket, dma_error_intr | Positive |
| 73 | test_dma_error_interrupt_clear | Verify dma_error interrupt clears on write-1 to STATUS.error | STATUS, ERROR_CODE, INTR_STATE | reg_target_socket, dma_error_intr | Positive |
| 74 | test_intr_enable_masking | Verify INTR_ENABLE register masks interrupt outputs correctly | INTR_ENABLE, INTR_STATE | reg_target_socket, dma_done_intr, dma_chunk_done_intr, dma_error_intr | Positive |
| **8. Error Conditions** |
| 75 | test_error_src_addr_misalignment_2byte | Source address misaligned for 2-byte transfer (address[0] != 0) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 76 | test_error_src_addr_misalignment_4byte | Source address misaligned for 4-byte transfer (address[1:0] != 00) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 77 | test_error_dst_addr_misalignment_2byte | Destination address misaligned for 2-byte transfer | DST_ADDR_LO, SRC_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 78 | test_error_dst_addr_misalignment_4byte | Destination address misaligned for 4-byte transfer | DST_ADDR_LO, SRC_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 79 | test_error_src_addr_upper32_ot_asid | Source upper 32 bits non-zero for OT_ADDR (ASID=0x7) | SRC_ADDR_LO, SRC_ADDR_HI, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 80 | test_error_dst_addr_upper32_ot_asid | Destination upper 32 bits non-zero for OT_ADDR (ASID=0x7) | SRC_ADDR_LO, DST_ADDR_LO, DST_ADDR_HI, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 81 | test_error_invalid_asid_src | Invalid source ASID value (not 0x7, 0x9, or 0xA) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 82 | test_error_invalid_asid_dst | Invalid destination ASID value (not 0x7, 0x9, or 0xA) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 83 | test_error_zero_total_data_size | TOTAL_DATA_SIZE register set to zero | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 84 | test_error_zero_chunk_data_size | CHUNK_DATA_SIZE register set to zero | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 85 | test_error_invalid_transfer_width | TRANSFER_WIDTH register contains reserved value (0x3) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 86 | test_error_invalid_opcode | CONTROL.opcode contains invalid value (not 0x0, 0x1, 0x2, 0x3) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 87 | test_error_hash_width_mismatch | Inline hashing enabled but TRANSFER_WIDTH is not 4-byte | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 88 | test_error_base_greater_than_limit | ENABLED_MEMORY_RANGE_BASE greater than ENABLED_MEMORY_RANGE_LIMIT | ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 89 | test_error_range_not_valid | RANGE_VALID not set but cross-boundary transfer attempted | ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 90 | test_error_ot_private_to_soc_blocked | Transfer from OT Private Memory to SoC Memory (blocked by security policy) | SRC_ADDR_LO, DST_ADDR_LO, DST_ADDR_HI, ADDR_SPACE_ID, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 91 | test_error_soc_to_ot_private_blocked | Transfer from SoC Memory to OT Private Memory (blocked by security policy) | SRC_ADDR_LO, SRC_ADDR_HI, DST_ADDR_LO, ADDR_SPACE_ID, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 92 | test_error_bus_error_src_read | Bus error response during source read transaction | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, ot_initiator_socket, dma_error_intr | Negative |
| 93 | test_error_bus_error_dst_write | Bus error response during destination write transaction | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, ot_initiator_socket, dma_error_intr | Negative |
| 94 | test_error_multiple_simultaneous | Multiple configuration errors detected simultaneously (all ERROR_CODE bits set) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 95 | test_error_recovery_sequence | Verify error recovery sequence (read ERROR_CODE, clear STATUS.error, reconfigure, retry) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Positive |
| **9. Transfer Abort Mechanism** |
| 96 | test_abort_during_transfer | Verify abort operation halts ongoing transfer and sets STATUS.aborted | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket | Positive |
| 97 | test_abort_ot_transactions_complete | Verify OT-internal transactions complete before abort finishes | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket | Positive |
| 98 | test_abort_during_multi_chunk | Abort during multi-chunk transfer, verify partial completion | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket | Positive |
| 99 | test_abort_during_inline_hashing | Abort during inline hashing operation, verify digest invalid | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, SHA2_DIGEST_0-7 | reg_target_socket, ot_initiator_socket | Positive |
| 100 | test_abort_clear_status | Verify STATUS.aborted clears on write-1 | STATUS | reg_target_socket | Positive |
| **10. Corner Cases and Boundary Conditions** |
| 101 | test_chunk_size_not_divisor_of_total | CHUNK_DATA_SIZE does not evenly divide TOTAL_DATA_SIZE (final chunk smaller) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 102 | test_minimum_transfer_size_1byte | Minimum transfer size of 1 byte | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 103 | test_minimum_transfer_size_4bytes | Minimum transfer size of 4 bytes with 4-byte width | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 104 | test_maximum_transfer_size_4gb | Maximum transfer size approaching 4GB (TOTAL_DATA_SIZE=0xFFFFFFFF) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 105 | test_address_alignment_byte_boundary | 1-byte transfer with any byte-aligned address | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 106 | test_address_alignment_halfword_boundary | 2-byte transfer with halfword-aligned address | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 107 | test_address_alignment_word_boundary | 4-byte transfer with word-aligned address | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 108 | test_wrap_mode_chunk_boundary | Wrap mode address wraps correctly at chunk boundary | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 109 | test_64bit_address_full_range | 64-bit System bus address using full address range | SRC_ADDR_LO, SRC_ADDR_HI, DST_ADDR_LO, DST_ADDR_HI, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, sys_initiator_socket, dma_done_intr | Positive |
| 110 | test_32bit_address_max_value | 32-bit OT internal address at maximum value (0xFFFFFFFF) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 111 | test_sub_word_extract_1byte_lane0 | 1-byte transfer from byte lane 0 (address[1:0]=00) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 112 | test_sub_word_extract_1byte_lane3 | 1-byte transfer from byte lane 3 (address[1:0]=11) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 113 | test_sub_word_extract_2byte_lane0 | 2-byte transfer from byte lanes 0-1 (address[1:0]=00) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 114 | test_sub_word_extract_2byte_lane2 | 2-byte transfer from byte lanes 2-3 (address[1:0]=10) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 115 | test_hw_handshake_total_size_reached | Hardware handshake mode completes total size, go bit remains set, no response to further triggers | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[0], ot_initiator_socket, dma_done_intr | Positive |
| 116 | test_memory_range_boundary_base | Transfer with address exactly at ENABLED_MEMORY_RANGE_BASE | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 117 | test_memory_range_boundary_limit | Transfer with address exactly at ENABLED_MEMORY_RANGE_LIMIT (inclusive) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 118 | test_memory_range_below_base | Transfer with address below ENABLED_MEMORY_RANGE_BASE (error) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 119 | test_memory_range_above_limit | Transfer with address above ENABLED_MEMORY_RANGE_LIMIT (error) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 120 | test_address_overflow_32bit | Address increment causes 32-bit address overflow (wraps to 0) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| **11. Reset Behavior** |
| 121 | test_reset_during_idle | Verify reset clears all registers when DMA is idle | All registers | rst_ni, reg_target_socket | Positive |
| 122 | test_reset_during_active_transfer | Verify reset aborts transfer and clears all registers | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | rst_ni, reg_target_socket, ot_initiator_socket | Positive |
| 123 | test_reset_unlocks_range_regwen | Verify reset unlocks RANGE_REGWEN (returns to 0x6) | RANGE_REGWEN | rst_ni, reg_target_socket | Positive |
| 124 | test_reset_deasserts_interrupts | Verify reset de-asserts all interrupt outputs | INTR_STATE, STATUS | rst_ni, reg_target_socket, dma_done_intr, dma_chunk_done_intr, dma_error_intr | Positive |

---

## Test Categories

### 1. Register Access Tests (Tests 1-13)
Validate register access types, reset values, reserved fields, and locking mechanisms (CFG_REGWEN, RANGE_REGWEN).

### 2. Basic Transfer Operations (Tests 14-23)
Verify fundamental data transfer operations across different bus interfaces (OT, CTN, System) with various transfer sizes and widths.

### 3. Addressing Modes (Tests 24-30)
Test all addressing mode combinations: incrementing, fixed, and wrap/circular buffer modes for source and destination.

### 4. Hardware Handshaking Mode (Tests 31-45)
Validate autonomous peripheral FIFO servicing with all 11 lsio_trigger sources and automatic interrupt clearing mechanism.

### 5. Inline SHA-2 Hashing (Tests 46-54)
Verify SHA-256/384/512 hash computation during data transfers, including multi-chunk hashing with state preservation.

### 6. Security Features and Memory Isolation (Tests 55-65)
Test three-tier memory model enforcement, ASID-based routing, and memory range validation for cross-boundary transfers.

### 7. Interrupt Generation and Clearing (Tests 66-74)
Validate assertion/deassertion conditions for dma_done, dma_chunk_done, and dma_error interrupts with various clearing mechanisms.

### 8. Error Conditions (Tests 75-95)
Test all 8 error types: src_addr_error, dst_addr_error, opcode_error, size_error, bus_error, base_limit_error, range_valid_error, asid_error.

### 9. Transfer Abort Mechanism (Tests 96-100)
Verify abort operation behavior, completion guarantees for OT-internal transactions, and status handling.

### 10. Corner Cases and Boundary Conditions (Tests 101-120)
Test edge cases including non-divisible chunk sizes, minimum/maximum transfer sizes, address boundaries, sub-word extraction, and address overflow.

### 11. Reset Behavior (Tests 121-124)
Validate reset operation during idle and active states, register clearing, and interrupt de-assertion.

---

## Ambiguous Items

No conflicts or ambiguities were identified between the knowledge base and design documentation. All register names, port/signal names, error conditions, and functional behaviors are consistent across sources.

---

## Notes

- All test cases use exact register names as defined in dma-detailed-design.md
- All port/signal names match specifications in dma-port-interfaces.md
- Each hardware handshake trigger (lsio_trigger[0] through lsio_trigger[10]) has a dedicated test case
- All 8 error types have individual negative test cases
- Security enforcement tests cover all valid and invalid ASID combinations
- Test plan excludes non-modeled features (clock gating circuits, pin-level timing, sparse FSM encoding, multibit signal bit patterns)
- C/C++ code snippets are not included in test descriptions
- Test cases focus on software-visible functional behavior at transaction level
- No cycle-accurate timing requirements
