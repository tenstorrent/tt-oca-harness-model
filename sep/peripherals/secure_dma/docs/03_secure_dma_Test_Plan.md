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
| 2 | test_alert_test_write_only | Verify ALERT_TEST register write triggers fatal_fault alert | ALERT_TEST | reg_target_socket | Positive |
| 3 | test_cfg_regwen_read_only | Verify CFG_REGWEN is read-only and reflects DMA busy/idle state (MuBi4: 0x9=locked/busy, 0x6=unlocked/idle) | CFG_REGWEN, CONTROL, STATUS | reg_target_socket | Positive |
| 4 | test_range_regwen_write_lock | Verify RANGE_REGWEN locks on any write that is not 0x6, reads back 0x9, and stays locked until reset | RANGE_REGWEN, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID | reg_target_socket | Positive |
| 5 | test_reserved_bits_read_zero | Verify reserved bits in all registers read as zero | All registers with reserved fields | reg_target_socket | Positive |
| 6 | test_reserved_bits_write_ignored | Verify writes to reserved bits are ignored without side effects | All registers with reserved fields | reg_target_socket | Positive |
| 7 | test_cfg_regwen_locked_registers | Verify configuration registers are read-only when CFG_REGWEN=0x9 (DMA busy) | SRC_ADDR_LO, SRC_ADDR_HI, DST_ADDR_LO, DST_ADDR_HI, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket | Positive |
| 8 | test_control_status_always_accessible | Verify CONTROL and STATUS registers remain accessible during transfer (not locked by CFG_REGWEN) | CONTROL, STATUS, CFG_REGWEN | reg_target_socket | Positive |
| **2. Basic Transfer Operations** |
| 9 | test_mem_to_mem_single_chunk_4byte | Simple memory-to-memory transfer, single chunk, 4-byte width, incrementing addresses | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 10 | test_mem_to_mem_single_chunk_2byte | Memory-to-memory transfer, single chunk, 2-byte width, incrementing addresses | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 11 | test_mem_to_mem_single_chunk_1byte | Memory-to-memory transfer, single chunk, 1-byte width, incrementing addresses | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 12 | test_mem_to_mem_multi_chunk | Memory-to-memory transfer with multiple chunks, verify chunk_done interrupts | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, INTR_ENABLE | reg_target_socket, ot_initiator_socket, dma_chunk_done_intr, dma_done_intr | Positive |
| 13 | test_mem_to_mem_ctn_32bit | Memory-to-memory transfer using CTN interface (32-bit address) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ctn_initiator_socket, dma_done_intr | Positive |
| 14 | test_mem_to_mem_sys_64bit | Memory-to-memory transfer using System bus (64-bit address) | SRC_ADDR_LO, SRC_ADDR_HI, DST_ADDR_LO, DST_ADDR_HI, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, sys_initiator_socket, dma_done_intr | Positive |
| 15 | test_transfer_size_16bytes | Transfer with TOTAL_DATA_SIZE=16 bytes, CHUNK_DATA_SIZE=16 bytes | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 16 | test_transfer_size_1024bytes | Transfer with TOTAL_DATA_SIZE=1024 bytes, CHUNK_DATA_SIZE=256 bytes | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 17 | test_transfer_size_4096bytes | Transfer with TOTAL_DATA_SIZE=4096 bytes, CHUNK_DATA_SIZE=512 bytes | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| **3. Addressing Modes** |
| 18 | test_src_increment_dst_increment | Source and destination both use incrementing addressing mode | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 19 | test_src_fixed_dst_increment | Source uses fixed address (FIFO-like), destination uses incrementing | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 20 | test_src_increment_dst_fixed | Source uses incrementing, destination uses fixed address (FIFO-like) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 21 | test_src_fixed_dst_fixed | Both source and destination use fixed address mode | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 22 | test_src_wrap_mode | Source uses wrap/circular buffer mode (increment=1, wrap=1) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 23 | test_dst_wrap_mode | Destination uses wrap/circular buffer mode (increment=1, wrap=1) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 24 | test_both_wrap_mode | Both source and destination use wrap mode | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| **4. Hardware Handshaking Mode** |
| 25 | test_hw_handshake_trigger0 | Hardware handshake mode with lsio_trigger[0] for I2C RX FIFO | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[0], ot_initiator_socket, dma_done_intr | Positive |
| 26 | test_hw_handshake_trigger1 | Hardware handshake mode with lsio_trigger[1] for I2C TX FIFO | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[1], ot_initiator_socket, dma_done_intr | Positive |
| 27 | test_hw_handshake_trigger2 | Hardware handshake mode with lsio_trigger[2] for UART RX FIFO | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[2], ot_initiator_socket, dma_done_intr | Positive |
| 28 | test_hw_handshake_trigger3 | Hardware handshake mode with lsio_trigger[3] for UART TX FIFO | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[3], ot_initiator_socket, dma_done_intr | Positive |
| 29 | test_hw_handshake_trigger4 | Hardware handshake mode with lsio_trigger[4] for SPI Device RX FIFO | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[4], ot_initiator_socket, dma_done_intr | Positive |
| 30 | test_hw_handshake_trigger5 | Hardware handshake mode with lsio_trigger[5] for SPI Device TX FIFO | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[5], ot_initiator_socket, dma_done_intr | Positive |
| 31 | test_hw_handshake_trigger6 | Hardware handshake mode with lsio_trigger[6] for SPI Host RX FIFO | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[6], ot_initiator_socket, dma_done_intr | Positive |
| 32 | test_hw_handshake_trigger7 | Hardware handshake mode with lsio_trigger[7] for SPI Host TX FIFO | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[7], ot_initiator_socket, dma_done_intr | Positive |
| 33 | test_hw_handshake_trigger8 | Hardware handshake mode with lsio_trigger[8] for peripheral FIFO | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[8], ot_initiator_socket, dma_done_intr | Positive |
| 34 | test_hw_handshake_trigger9 | Hardware handshake mode with lsio_trigger[9] for peripheral FIFO | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[9], ot_initiator_socket, dma_done_intr | Positive |
| 35 | test_hw_handshake_trigger10 | Hardware handshake mode with lsio_trigger[10] for peripheral FIFO | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[10], ot_initiator_socket, dma_done_intr | Positive |
| 36 | test_hw_handshake_auto_clear_ot_bus | Hardware handshake with automatic interrupt clearing on OT-internal bus | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CLEAR_INTR_SRC, CLEAR_INTR_BUS, INTR_SRC_ADDR_0, INTR_SRC_WR_VAL_0, CONTROL, STATUS | reg_target_socket, lsio_trigger[0], ot_initiator_socket, dma_done_intr | Positive |
| 37 | test_hw_handshake_auto_clear_ctn_bus | Hardware handshake with automatic interrupt clearing on CTN/System bus | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CLEAR_INTR_SRC, CLEAR_INTR_BUS, INTR_SRC_ADDR_0, INTR_SRC_WR_VAL_0, CONTROL, STATUS | reg_target_socket, lsio_trigger[0], ctn_initiator_socket, dma_done_intr | Positive |
| 38 | test_hw_handshake_go_bit_remains_set | Verify go bit remains set in hardware handshake mode after transfer completion | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[0], ot_initiator_socket, dma_done_intr | Positive |
| 39 | test_hw_handshake_no_chunk_done_intr | Verify chunk_done interrupt is not generated in hardware handshake mode | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[0], ot_initiator_socket, dma_chunk_done_intr, dma_done_intr | Positive |
| **5. Inline SHA-2 Hashing** |
| 40 | test_inline_sha256_single_chunk | Inline SHA-256 hashing with single-chunk transfer | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, SHA2_DIGEST_0-7 | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 41 | test_inline_sha384_single_chunk | Inline SHA-384 hashing with single-chunk transfer | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, SHA2_DIGEST_0-11 | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 42 | test_inline_sha512_single_chunk | Inline SHA-512 hashing with single-chunk transfer | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, SHA2_DIGEST_0-15 | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 43 | test_inline_sha256_multi_chunk | Inline SHA-256 hashing across multiple chunks with state preservation | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, SHA2_DIGEST_0-7 | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 44 | test_inline_sha384_multi_chunk | Inline SHA-384 hashing across multiple chunks with state preservation | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, SHA2_DIGEST_0-11 | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 45 | test_inline_sha512_multi_chunk | Inline SHA-512 hashing across multiple chunks with state preservation | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, SHA2_DIGEST_0-15 | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 46 | test_initial_transfer_bit_hash_reset | Verify initial_transfer=1 resets SHA-2 hash state for new computation | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, SHA2_DIGEST_0-7 | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 47 | test_digest_swap_endianness | Verify digest_swap bit controls endianness conversion of digest output | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, SHA2_DIGEST_0-7 | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 48 | test_sha2_digest_valid_bit | Verify sha2_digest_valid bit is set when digest is ready | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| **6. Security Features and Memory Isolation** |
| 49 | test_ot_private_to_ot_private | Transfer from OT Private Memory to OT Private Memory (allowed) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 50 | test_ot_private_to_ot_dma_enabled | Transfer from OT Private Memory to OT DMA-enabled Memory (allowed) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 51 | test_ot_dma_enabled_to_ot_private | Transfer from OT DMA-enabled Memory to OT Private Memory (allowed) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 52 | test_ot_dma_enabled_to_soc | Transfer from OT DMA-enabled Memory to SoC Memory (allowed) | SRC_ADDR_LO, DST_ADDR_LO, DST_ADDR_HI, ADDR_SPACE_ID, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, sys_initiator_socket, dma_done_intr | Positive |
| 53 | test_soc_to_ot_dma_enabled | Transfer from SoC Memory to OT DMA-enabled Memory (allowed) | SRC_ADDR_LO, SRC_ADDR_HI, DST_ADDR_LO, ADDR_SPACE_ID, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, sys_initiator_socket, ot_initiator_socket, dma_done_intr | Positive |
| 54 | test_soc_to_soc | Transfer from SoC Memory to SoC Memory (allowed) | SRC_ADDR_LO, SRC_ADDR_HI, DST_ADDR_LO, DST_ADDR_HI, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, sys_initiator_socket, dma_done_intr | Positive |
| 55 | test_memory_range_base_limit_config | Configure and verify memory range base and limit registers | ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID | reg_target_socket | Positive |
| 56 | test_range_valid_bit_requirement | Verify RANGE_VALID must be set before cross-boundary transfers | ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, dma_error_intr | Positive |
| **7. Interrupt Generation and Clearing** |
| 57 | test_dma_done_interrupt_assert | Verify dma_done interrupt asserts on transfer completion | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, INTR_ENABLE, CONTROL, STATUS, INTR_STATE | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 58 | test_dma_done_interrupt_clear_rw1c | Verify dma_done interrupt clears on write-1 to STATUS.done | STATUS, INTR_STATE | reg_target_socket, dma_done_intr | Positive |
| 59 | test_dma_done_auto_clear_on_new_transfer | Verify dma_done bit auto-clears when new transfer starts (go bit set) | CONTROL, STATUS, INTR_STATE | reg_target_socket, dma_done_intr | Positive |
| 60 | test_dma_chunk_done_interrupt_assert | Verify dma_chunk_done interrupt asserts on chunk completion | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, INTR_ENABLE, CONTROL, STATUS, INTR_STATE | reg_target_socket, ot_initiator_socket, dma_chunk_done_intr | Positive |
| 61 | test_dma_chunk_done_interrupt_clear | Verify dma_chunk_done interrupt clears on write-1 to STATUS.chunk_done | STATUS, INTR_STATE | reg_target_socket, dma_chunk_done_intr | Positive |
| 62 | test_dma_chunk_done_auto_clear | Verify dma_chunk_done bit auto-clears when next chunk starts | CONTROL, STATUS, INTR_STATE | reg_target_socket, dma_chunk_done_intr | Positive |
| 63 | test_dma_error_interrupt_assert | Verify dma_error interrupt asserts on error conditions | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, INTR_ENABLE, CONTROL, STATUS, ERROR_CODE, INTR_STATE | reg_target_socket, dma_error_intr | Positive |
| 64 | test_dma_error_interrupt_clear | Verify dma_error interrupt clears on write-1 to STATUS.error | STATUS, ERROR_CODE, INTR_STATE | reg_target_socket, dma_error_intr | Positive |
| 65 | test_intr_enable_masking | Verify INTR_ENABLE register masks interrupt outputs correctly | INTR_ENABLE, INTR_STATE | reg_target_socket, dma_done_intr, dma_chunk_done_intr, dma_error_intr | Positive |
| **8. Error Conditions** |
| 66 | test_error_src_addr_upper32_ot_asid | Source upper 32 bits non-zero for OT_ADDR (ASID=0x7) | SRC_ADDR_LO, SRC_ADDR_HI, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 67 | test_error_dst_addr_upper32_ot_asid | Destination upper 32 bits non-zero for OT_ADDR (ASID=0x7) | SRC_ADDR_LO, DST_ADDR_LO, DST_ADDR_HI, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 68 | test_error_invalid_asid_src | Invalid source ASID value (not 0x7, 0x9, or 0xA) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 69 | test_error_invalid_asid_dst | Invalid destination ASID value (not 0x7, 0x9, or 0xA) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 70 | test_error_zero_total_data_size | TOTAL_DATA_SIZE register set to zero | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 71 | test_error_zero_chunk_data_size | CHUNK_DATA_SIZE register set to zero | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 72 | test_error_invalid_transfer_width | TRANSFER_WIDTH register contains reserved value (0x3) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 73 | test_error_hash_width_mismatch | Inline hashing enabled but TRANSFER_WIDTH is not 4-byte | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 74 | test_error_base_greater_than_limit | ENABLED_MEMORY_RANGE_BASE greater than ENABLED_MEMORY_RANGE_LIMIT | ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 75 | test_error_range_not_valid | RANGE_VALID not set but cross-boundary transfer attempted | ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 76 | test_error_ot_private_to_soc_blocked | Transfer from OT Private Memory to SoC Memory (blocked by security policy) | SRC_ADDR_LO, DST_ADDR_LO, DST_ADDR_HI, ADDR_SPACE_ID, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 77 | test_error_soc_to_ot_private_blocked | Transfer from SoC Memory to OT Private Memory (blocked by security policy) | SRC_ADDR_LO, SRC_ADDR_HI, DST_ADDR_LO, ADDR_SPACE_ID, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 78 | test_error_recovery_sequence | Verify error recovery sequence (read ERROR_CODE, clear STATUS.error, reconfigure, retry) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Positive |
| **9. Transfer Abort Mechanism** |
| 79 | test_abort_during_transfer | Verify abort operation halts ongoing transfer and sets STATUS.aborted | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket | Positive |
| 80 | test_abort_ot_transactions_complete | Verify OT-internal transactions complete before abort finishes | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket | Positive |
| 81 | test_abort_during_multi_chunk | Abort during multi-chunk transfer, verify partial completion | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket | Positive |
| 82 | test_abort_during_inline_hashing | Abort during inline hashing operation, verify digest invalid | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, SHA2_DIGEST_0-7 | reg_target_socket, ot_initiator_socket | Positive |
| 83 | test_abort_clear_status | Verify STATUS.aborted clears on write-1 | STATUS | reg_target_socket | Positive |
| **10. Corner Cases and Boundary Conditions** |
| 84 | test_chunk_size_not_divisor_of_total | CHUNK_DATA_SIZE does not evenly divide TOTAL_DATA_SIZE (final chunk smaller) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 85 | test_minimum_transfer_size_1byte | Minimum transfer size of 1 byte | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 86 | test_maximum_transfer_size_4gb | Maximum transfer size approaching 4GB (TOTAL_DATA_SIZE=0xFFFFFFFF) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 87 | test_address_alignment_byte_boundary | 1-byte transfer with any byte-aligned address | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 88 | test_address_alignment_halfword_boundary | 2-byte transfer with halfword-aligned address | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 89 | test_address_alignment_word_boundary | 4-byte transfer with word-aligned address | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 90 | test_wrap_mode_chunk_boundary | Wrap mode address wraps correctly at chunk boundary | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 91 | test_64bit_address_full_range | 64-bit System bus address using full address range | SRC_ADDR_LO, SRC_ADDR_HI, DST_ADDR_LO, DST_ADDR_HI, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, sys_initiator_socket, dma_done_intr | Positive |
| 92 | test_32bit_address_max_value | 32-bit OT internal address at maximum value (0xFFFFFFFF) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 93 | test_sub_word_extract_1byte_lane0 | 1-byte transfer from byte lane 0 (address[1:0]=00) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 94 | test_sub_word_extract_1byte_lane3 | 1-byte transfer from byte lane 3 (address[1:0]=11) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 95 | test_sub_word_extract_2byte_lane0 | 2-byte transfer from byte lanes 0-1 (address[1:0]=00) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 96 | test_sub_word_extract_2byte_lane2 | 2-byte transfer from byte lanes 2-3 (address[1:0]=10) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 97 | test_hw_handshake_total_size_reached | Hardware handshake mode completes total size, go bit remains set, no response to further triggers | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, HANDSHAKE_INTR_ENABLE, CONTROL, STATUS | reg_target_socket, lsio_trigger[0], ot_initiator_socket, dma_done_intr | Positive |
| 98 | test_memory_range_boundary_base | Transfer with address exactly at ENABLED_MEMORY_RANGE_BASE | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 99 | test_memory_range_boundary_limit | Transfer with address exactly at ENABLED_MEMORY_RANGE_LIMIT (inclusive) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| 100 | test_memory_range_below_base | Transfer with address below ENABLED_MEMORY_RANGE_BASE (error) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 101 | test_memory_range_above_limit | Transfer with address above ENABLED_MEMORY_RANGE_LIMIT (error) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT, RANGE_VALID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 102 | test_address_overflow_32bit | Address increment causes 32-bit address overflow (wraps to 0) | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | reg_target_socket, ot_initiator_socket, dma_done_intr | Positive |
| **11. Reset Behavior** |
| 103 | test_reset_during_idle | Verify reset clears all registers when DMA is idle | All registers | rst_ni, reg_target_socket | Positive |
| 104 | test_reset_during_active_transfer | Verify reset aborts transfer and clears all registers | SRC_ADDR_LO, DST_ADDR_LO, ADDR_SPACE_ID, TOTAL_DATA_SIZE, CHUNK_DATA_SIZE, TRANSFER_WIDTH, SRC_CONFIG, DST_CONFIG, CONTROL, STATUS | rst_ni, reg_target_socket, ot_initiator_socket | Positive |
| 105 | test_reset_unlocks_range_regwen | Verify reset unlocks RANGE_REGWEN (returns to 0x6) | RANGE_REGWEN | rst_ni, reg_target_socket | Positive |
| 106 | test_reset_deasserts_interrupts | Verify reset de-asserts all interrupt outputs | INTR_STATE, STATUS | rst_ni, reg_target_socket, dma_done_intr, dma_chunk_done_intr, dma_error_intr | Positive |
| **12. other test behaviours** |
| 107 | test_register_ro | Verify all read only register |  |  | Positive |
| 108 | test_register_wo | Verify all write only register |  |  | Positive |
| 109 | test_register_rw | Verify all rw register |  |  | Positive |
| 110 | test_register_rw0c | Verify all rw0c register |  |  | Positive |
| 111 | test_register_rw1c | Verify all rw1c register |  |  | Positive |
| 112 | test_error_code_bit0_src_addr_error | Verify source address is invalid | ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 113 | test_error_code_bit1_dst_addr_error | Verify destination address is invalid | ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 114 | test_error_code_bit2_opcode_error | Verify opcode is invalid | ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 115 | test_error_code_bit3_size_error | Verify TRANSFER_WIDTH encodes an invalid value, TOTAL_DATA_SIZE or CHUNK_SIZE are zero, or inline hashing is not using 32-bit transfer width | ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 116 | test_error_code_bit4_bus_error | Verify the bus transfer returned an error | ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 117 | test_error_code_bit5_base_limit_error | Verify the base and limit addresses contain an invalid value | ERROR_CODE, ENABLED_MEMORY_RANGE_BASE, ENABLED_MEMORY_RANGE_LIMIT | reg_target_socket, dma_error_intr | Negative |
| 118 | test_error_code_bit6_range_valid_error | Verify the DMA enabled memory range is not configured | ERROR_CODE, RANGE_VALID | reg_target_socket, dma_error_intr | Negative |
| 119 | test_error_code_bit7_asid_error | Verify the source or destination ASID contains an invalid value | ERROR_CODE, ADDR_SPACE_ID | reg_target_socket, dma_error_intr | Negative |
| 120 | test_error_2byte_misaligned | Source and destination address misaligned for 2-byte transfer (address[0] != 0) | SRC_ADDR_LO, DST_ADDR_LO, TRANSFER_WIDTH, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |
| 121 | test_error_4byte_misaligned | Source and destination address misaligned for 4-byte transfer (address[1:0] != 00) | SRC_ADDR_LO, DST_ADDR_LO, TRANSFER_WIDTH, ERROR_CODE | reg_target_socket, dma_error_intr | Negative |

---

## Test Categories

### 1. Register Access Tests (Tests 1-8)
Validate register access types, reset values, reserved fields, and locking mechanisms (CFG_REGWEN, RANGE_REGWEN).

### 2. Basic Transfer Operations (Tests 9-18)
Verify fundamental data transfer operations across different bus interfaces (OT, CTN, System) with various transfer sizes and widths.

### 3. Addressing Modes (Tests 19-25)
Test all addressing mode combinations: incrementing, fixed, and wrap/circular buffer modes for source and destination.

### 4. Hardware Handshaking Mode (Tests 26-40)
Validate autonomous peripheral FIFO servicing with all 11 lsio_trigger sources and automatic interrupt clearing mechanism.

### 5. Inline SHA-2 Hashing (Tests 41-49)
Verify SHA-256/384/512 hash computation during data transfers, including multi-chunk hashing with state preservation.

### 6. Security Features and Memory Isolation (Tests 50-56)
Test three-tier memory model enforcement, ASID-based routing, and memory range validation for cross-boundary transfers.

### 7. Interrupt Generation and Clearing (Tests 57-65)
Validate assertion/deassertion conditions for dma_done, dma_chunk_done, and dma_error interrupts with various clearing mechanisms.

### 8. Error Conditions (Tests 66-78)
Test all 8 error types: src_addr_error, dst_addr_error, opcode_error, size_error, bus_error, base_limit_error, range_valid_error, asid_error.

### 9. Transfer Abort Mechanism (Tests 79-83)
Verify abort operation behavior, completion guarantees for OT-internal transactions, and status handling.

### 10. Corner Cases and Boundary Conditions (Tests 84-102)
Test edge cases including non-divisible chunk sizes, minimum/maximum transfer sizes, address boundaries, sub-word extraction, and address overflow.

### 11. Reset Behavior (Tests 103-106)
Validate reset operation during idle and active states, register clearing, and interrupt de-assertion.

### 12. other test behaviours (Tests 107-121)
Validate register access groups (RO/WO/RW/RW0C/RW1C), ERROR_CODE bit-specific reporting, and misalignment behavior checks.

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

---

## Writing tests against this model

Five hardware behaviours trip up tests written from intuition rather than from the RTL.
Each one caused real failures when the model was brought into line, so they are recorded
here rather than left to be rediscovered.

1. **Set `RANGE_VALID` in every transfer configuration.** It is checked unconditionally, not
   only when a transfer crosses the enabled-range boundary. Omit it and the transfer is
   rejected with `ERROR_CODE.range_valid_error` (`0x40`) before it starts.

2. **A locked REGWEN reads `0x9`, not `0x0`.** MuBi4False is `0x9`, and `RANGE_REGWEN` locks
   on any write that is not `0x6`.

3. **Address CSRs do not advance in increment or wrap mode.** Writeback happens only for a
   port in fixed mode, at chunk end. Assert a *stationary* `SRC_ADDR_LO` for an incrementing
   transfer.

4. **A fixed-address FIFO endpoint is `SRC_CONFIG = 0x2`**, wrap set and increment clear —
   not `0x0`. Hardware-handshake sources need this to pin the address across a chunk.

5. **A bus error on the interrupt-clearing write halts the transfer.** Expect `STATUS.error`
   set, `STATUS.done` and `STATUS.busy` clear, `ERROR_CODE.bus_error` set, and the
   destination untouched. The DMA does not carry on with the remaining chunks.

One further caution about test isolation: one-shot error injection helpers such as
`inject_ot_read_bus_error_once()` stay armed until something consumes them. If the transfer
they were meant for is rejected during validation, the injection survives into the next test
and fails it instead. When a test fails with an error it never asked for, check whether an
earlier test left an injection unconsumed.

### Suites not executed

`run_func004_tests()` and `run_func005_tests()` are commented out in `testbench.cpp` and have
been since the initial import. Neither suite writes `CONTROL`, so neither ever starts a
transfer — all 68 cases configure registers and read them back. The behaviour their names
suggest (addressing modes, multi-bus routing) is covered by FUNC-009, whose TC024–TC027
exercise increment and fixed mode and whose TC018–TC020 exercise the CTN and SYS paths with
real transfers. Enabling FUNC-004 and FUNC-005 would add green lines without adding
coverage.
