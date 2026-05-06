# HMAC IP SystemC TLM2.0 Test Plan

**Document Version:** 1.0
**Date:** 2025-10-23
**IP Name:** HMAC (Hash-based Message Authentication Code)
**Target:** SystemC TLM2.0 Model Verification

---

## 1. Introduction

### 1.1 Purpose

This test plan defines a comprehensive verification strategy for the HMAC IP SystemC TLM2.0 model. The test plan covers functional verification, register access validation, error condition testing, interrupt behavior, timing verification, and cryptographic correctness validation.

### 1.2 Verification Scope

The verification focuses on:
- All 59 register groups and 60 register callbacks
- State machine transitions (IDLE, PROCESSING, FINALIZED)
- Three operational modes (SHA-2 256, SHA-2 384, SHA-2 512)
- HMAC mode with five key lengths (128, 256, 384, 512, 1024 bits)
- Context switching for multi-stream processing
- Three interrupt sources (hmac_done, fifo_empty, hmac_err)
- Six error conditions and recovery mechanisms
- Message FIFO management and back-pressure
- OpenSSL library integration for cryptographic operations
- Timing annotations for block processing latencies

### 1.3 Verification Methodology

The verification uses a SystemC TLM2.0 testbench with:
- Directed test cases for specific feature validation
- Test vector validation using NIST and RFC standards
- Error injection for robustness testing
- Corner case testing for boundary conditions
- Timing measurement for performance validation

### 1.4 Success Criteria

Verification is complete when:
- All test cases pass without errors
- All registers are accessed correctly
- All interrupts assert and deassert as specified
- All error conditions are detected and reported
- Digest results match expected values from test vectors
- Timing annotations match specification
- Context switching preserves hash state accurately

---

## 2. Test Case Organization

Test cases are organized into nine categories:
1. Reset and Initialization Tests
2. Register Access Tests
3. Functional Feature Tests
4. Interrupt Tests
5. Error Condition Tests
6. Context Switching Tests
7. Corner Case Tests
8. Performance and Timing Tests
9. Cryptographic Correctness Tests

---

## 3. Test Cases

| Sl. No. | TestCase Name | Description | Registers Programmed | Ports/Signals Used | Test Type |
| ------- | ------------- | ----------- | -------------------- | ------------------ | --------- |
| 1 | test_reset_mechanisms | Verify reset initializes all registers to default values, clears FIFO, sets state to IDLE, deasserts all interrupts, and validates both power-on reset and runtime reset during active operations | All registers: INTR_STATE=0x0, INTR_ENABLE=0x0, CFG=0x4100, STATUS=0x3, ERR_CODE=0x0, all KEY/DIGEST/MSG_LENGTH registers | rst_ni, intr_hmac_done, intr_fifo_empty, intr_hmac_err, alert_fatal_fault | Positive |
| 2 | test_readonly_registers | Verify read-only registers STATUS and ERR_CODE return correct values and reject write attempts without side effects | STATUS (read), ERR_CODE (read), attempt writes to both | tl_socket | Positive |
| 3 | test_writeonly_registers | Verify write-only registers CMD, INTR_TEST, ALERT_TEST, WIPE_SECRET, KEY_0 through KEY_31, and MSG_FIFO accept writes but return zero or undefined values on reads | CMD, INTR_TEST, ALERT_TEST, WIPE_SECRET, KEY_0 through KEY_31, MSG_FIFO (write), attempt reads | tl_socket | Positive |
| 4 | test_readwrite_registers | Verify read-write registers INTR_STATE, INTR_ENABLE, CFG, DIGEST_0 through DIGEST_15, MSG_LENGTH_LOWER, MSG_LENGTH_UPPER accept both reads and writes with correct data preservation | INTR_STATE, INTR_ENABLE, CFG, DIGEST_0 through DIGEST_15, MSG_LENGTH_LOWER, MSG_LENGTH_UPPER | tl_socket | Positive |
| 5 | test_reserved_fields | Verify writes to reserved register fields are ignored and reads return zero for reserved bits across all registers | All registers with reserved fields | tl_socket | Positive |
| 6 | test_sha256_hash_short_message | Compute SHA-2 256 hash of a short message (less than 64 bytes fitting in single block), validate against expected digest | CFG (digest_size=SHA2_256, sha_en=1, hmac_en=0), CMD (hash_start, hash_process), MSG_FIFO, DIGEST_0 through DIGEST_7 | tl_socket, intr_hmac_done | Positive |
| 7 | test_sha256_hash_multiblock_message | Compute SHA-2 256 hash of a message spanning multiple blocks (128 bytes requiring 2 blocks), validate FIFO management and block processing | CFG, CMD, MSG_FIFO (multiple block writes), DIGEST_0 through DIGEST_7, STATUS (fifo_depth monitoring) | tl_socket, intr_hmac_done | Positive |
| 8 | test_sha384_hash | Compute SHA-2 384 hash of a message, validate 384-bit digest output using appropriate number of DIGEST registers | CFG (digest_size=SHA2_384), CMD, MSG_FIFO, DIGEST_0 through DIGEST_11 | tl_socket, intr_hmac_done | Positive |
| 9 | test_sha512_hash | Compute SHA-2 512 hash of a message, validate 512-bit digest output using all DIGEST registers | CFG (digest_size=SHA2_512), CMD, MSG_FIFO, DIGEST_0 through DIGEST_15 | tl_socket, intr_hmac_done | Positive |
| 10 | test_hmac_sha256_key128 | Compute HMAC-SHA256 with 128-bit key, validate authentication tag matches expected result | CFG (hmac_en=1, key_length=Key_128), KEY_0 through KEY_3, CMD, MSG_FIFO, DIGEST_0 through DIGEST_7 | tl_socket, intr_hmac_done | Positive |
| 11 | test_hmac_sha256_key256 | Compute HMAC-SHA256 with 256-bit key, validate authentication tag and verify two-round HMAC processing | CFG (key_length=Key_256), KEY_0 through KEY_7, CMD, MSG_FIFO, DIGEST_0 through DIGEST_7 | tl_socket, intr_hmac_done | Positive |
| 12 | test_hmac_sha256_key512 | Compute HMAC-SHA256 with 512-bit key (maximum for SHA-2 256 mode), validate authentication tag | CFG (key_length=Key_512), KEY_0 through KEY_15, CMD, MSG_FIFO, DIGEST_0 through DIGEST_7 | tl_socket, intr_hmac_done | Positive |
| 13 | test_hmac_sha384_key384 | Compute HMAC-SHA384 with 384-bit key, validate 384-bit authentication tag output | CFG (digest_size=SHA2_384, hmac_en=1, key_length=Key_384), KEY_0 through KEY_11, CMD, MSG_FIFO, DIGEST_0 through DIGEST_11 | tl_socket, intr_hmac_done | Positive |
| 14 | test_hmac_sha512_key1024 | Compute HMAC-SHA512 with 1024-bit key (maximum supported), validate 512-bit authentication tag using all DIGEST registers | CFG (digest_size=SHA2_512, key_length=Key_1024), KEY_0 through KEY_31, CMD, MSG_FIFO, DIGEST_0 through DIGEST_15 | tl_socket, intr_hmac_done | Positive |
| 15 | test_empty_message_hash | Compute SHA-2 256 hash of empty message (zero bytes), verify automatic padding and correct digest generation | CFG, CMD (hash_start immediately followed by hash_process with no MSG_FIFO writes), DIGEST_0 through DIGEST_7 | tl_socket, intr_hmac_done | Positive |
| 16 | test_minimum_length_transfer | Compute hash of single-byte message, validate byte write handling and message length tracking | CFG, CMD, MSG_FIFO (single byte write), MSG_LENGTH_LOWER, DIGEST_0 through DIGEST_7 | tl_socket, intr_hmac_done | Positive |
| 17 | test_maximum_length_transfer | Compute hash of large message approaching maximum 64-bit message length counter capacity, validate MSG_LENGTH_UPPER overflow handling | CFG, CMD, MSG_FIFO (many block writes), MSG_LENGTH_LOWER, MSG_LENGTH_UPPER, DIGEST_0 through DIGEST_7 | tl_socket, intr_hmac_done | Positive |
| 18 | test_block_boundary_message | Compute hash of message with length exactly equal to block size (64 bytes for SHA-2 256), validate block boundary handling | CFG, CMD, MSG_FIFO (exactly 16 words), DIGEST_0 through DIGEST_7 | tl_socket, intr_hmac_done | Positive |
| 19 | test_endian_swap_message | Compute hash with endian_swap enabled, verify byte-order conversion applied to message input data | CFG (endian_swap=1), CMD, MSG_FIFO, DIGEST_0 through DIGEST_7 | tl_socket, intr_hmac_done | Positive |
| 20 | test_digest_swap | Compute hash with digest_swap enabled, verify byte-order conversion applied to digest output registers | CFG (digest_swap=1), CMD, MSG_FIFO, DIGEST_0 through DIGEST_7 | tl_socket, intr_hmac_done | Positive |
| 21 | test_key_swap | Compute HMAC with key_swap enabled, verify byte-order interpretation for key register array | CFG (hmac_en=1, key_swap=1), KEY_0 through KEY_7, CMD, MSG_FIFO, DIGEST_0 through DIGEST_7 | tl_socket, intr_hmac_done | Positive |
| 22 | test_subword_writes_byte | Write message data using byte writes to MSG_FIFO, verify packer logic accumulates bytes into 32-bit words correctly | CFG, CMD, MSG_FIFO (multiple byte writes), DIGEST_0 through DIGEST_7 | tl_socket, intr_hmac_done | Positive |
| 23 | test_subword_writes_halfword | Write message data using halfword (16-bit) writes to MSG_FIFO, verify packer logic handles halfword accumulation | CFG, CMD, MSG_FIFO (halfword writes), DIGEST_0 through DIGEST_7 | tl_socket, intr_hmac_done | Positive |
| 24 | test_fifo_status_updates | Monitor STATUS register fields (fifo_empty, fifo_full, fifo_depth) during message writes and block processing, validate real-time status reflection | CFG, CMD, MSG_FIFO, STATUS (repeated reads to monitor fifo_empty, fifo_full, fifo_depth) | tl_socket, intr_hmac_done | Positive |
| 25 | test_message_length_tracking | Write message data and verify MSG_LENGTH_LOWER and MSG_LENGTH_UPPER registers accurately track message length in bits | CFG, CMD, MSG_FIFO, MSG_LENGTH_LOWER, MSG_LENGTH_UPPER | tl_socket | Positive |
| 26 | test_hmac_done_interrupt | Verify hmac_done interrupt asserts when hash_process completes and digest is available, test both interrupt-driven and polling detection | INTR_ENABLE (hmac_done=1), CFG, CMD (hash_process), INTR_STATE (hmac_done) | tl_socket, intr_hmac_done | Positive |
| 27 | test_fifo_empty_interrupt | Verify fifo_empty interrupt asserts conditionally when FIFO becomes empty after being full for incomplete message, test flow control signaling | INTR_ENABLE (fifo_empty=1), CFG, CMD, MSG_FIFO (fill FIFO, allow processing to empty), INTR_STATE (fifo_empty) | tl_socket, intr_fifo_empty | Positive |
| 28 | test_hmac_err_interrupt | Trigger error condition (e.g., write MSG_FIFO before hash_start), verify hmac_err interrupt asserts and ERR_CODE is set appropriately | INTR_ENABLE (hmac_err=1), MSG_FIFO (write before hash_start), INTR_STATE (hmac_err), ERR_CODE | tl_socket, intr_hmac_err | Negative |
| 29 | test_interrupt_masking | Enable and disable interrupt masks via INTR_ENABLE register, verify output ports respond correctly to masking changes while INTR_STATE flags persist | INTR_ENABLE (toggle enable bits), INTR_STATE, trigger interrupts | tl_socket, intr_hmac_done, intr_fifo_empty, intr_hmac_err | Positive |
| 30 | test_interrupt_clearing | Clear interrupts using write-one-to-clear semantics on INTR_STATE register, verify interrupt output ports deassert correctly | INTR_STATE (W1C on hmac_done, hmac_err), INTR_ENABLE | tl_socket, intr_hmac_done, intr_hmac_err | Positive |
| 31 | test_interrupt_injection | Use INTR_TEST register to inject test interrupts for all three interrupt types, verify INTR_STATE updates and output ports assert | INTR_TEST (write 1 to each bit), INTR_STATE, INTR_ENABLE | tl_socket, intr_hmac_done, intr_fifo_empty, intr_hmac_err | Positive |
| 32 | test_alert_fatal_fault | Use ALERT_TEST register to inject fatal fault alert, verify alert_fatal_fault output port asserts for testing | ALERT_TEST (fatal_fault=1) | tl_socket, alert_fatal_fault | Positive |
| 33 | test_error_conditions | Test all six error codes in consolidated sequence: SwPushMsgWhenShaDisabled, SwHashStartWhenShaDisabled, SwUpdateSecretKeyInProcess, SwHashStartWhenActive, SwPushMsgWhenDisallowed, SwInvalidConfig | CFG (various invalid configurations), CMD (invalid command sequences), KEY (write during processing), MSG_FIFO (write at wrong times), ERR_CODE, INTR_STATE (hmac_err) | tl_socket, intr_hmac_err | Negative |
| 34 | test_error_invalid_digest_size | Write CFG with digest_size=SHA2_None (0x8), issue hash_start, verify ERR_CODE=0x6 (SwInvalidConfig) and hmac_err interrupt | CFG (digest_size=0x8), CMD (hash_start), ERR_CODE, INTR_STATE (hmac_err) | tl_socket, intr_hmac_err | Negative |
| 35 | test_error_invalid_key_length_hmac | Enable HMAC mode with key_length=Key_None (0x20), issue hash_start, verify ERR_CODE=0x6 and hmac_err interrupt | CFG (hmac_en=1, key_length=0x20), CMD (hash_start), ERR_CODE, INTR_STATE (hmac_err) | tl_socket, intr_hmac_err | Negative |
| 36 | test_error_key1024_sha256 | Configure SHA-2 256 with Key_1024, issue hash_start, verify ERR_CODE=0x6 (key exceeds block size) | CFG (digest_size=SHA2_256, key_length=Key_1024), CMD (hash_start), ERR_CODE, INTR_STATE (hmac_err) | tl_socket, intr_hmac_err | Negative |
| 37 | test_error_hash_start_sha_disabled | Issue hash_start command with CFG.sha_en=0, verify ERR_CODE=0x2 (SwHashStartWhenShaDisabled) | CFG (sha_en=0), CMD (hash_start), ERR_CODE, INTR_STATE (hmac_err) | tl_socket, intr_hmac_err | Negative |
| 38 | test_error_hash_start_when_active | Issue hash_start when engine is already processing (STATUS.hmac_idle=0), verify ERR_CODE=0x4 (SwHashStartWhenActive) | CFG, CMD (first hash_start, then immediate second hash_start), ERR_CODE, INTR_STATE (hmac_err), STATUS | tl_socket, intr_hmac_err | Negative |
| 39 | test_error_key_write_during_processing | Start hash operation, attempt to write KEY registers while STATUS.hmac_idle=0, verify ERR_CODE=0x3 (SwUpdateSecretKeyInProcess) | CFG, CMD (hash_start), KEY_0 (write during processing), ERR_CODE, INTR_STATE (hmac_err), STATUS | tl_socket, intr_hmac_err | Negative |
| 40 | test_error_msg_fifo_before_start | Write to MSG_FIFO before issuing hash_start command, verify ERR_CODE=0x5 (SwPushMsgWhenDisallowed) | MSG_FIFO (write before hash_start), ERR_CODE, INTR_STATE (hmac_err) | tl_socket, intr_hmac_err | Negative |
| 41 | test_error_msg_fifo_after_process | Issue hash_process command, then attempt to write MSG_FIFO, verify ERR_CODE=0x5 or 0x1 (SwPushMsgWhenDisallowed) | CFG, CMD (hash_start, hash_process), MSG_FIFO (write after hash_process), ERR_CODE, INTR_STATE (hmac_err) | tl_socket, intr_hmac_err | Negative |
| 42 | test_error_recovery | Inject error condition, clear hmac_err interrupt, correct the error condition, restart operation, verify successful completion without residual error state | ERR_CODE, INTR_STATE (hmac_err clear via W1C), CFG, CMD, MSG_FIFO, DIGEST_0 through DIGEST_7 | tl_socket, intr_hmac_err, intr_hmac_done | Positive |
| 43 | test_context_save_basic | Start hash operation, write partial message, issue hash_stop at block boundary, verify INTR_STATE.hmac_done asserts and DIGEST/MSG_LENGTH contain intermediate state | CFG, CMD (hash_start, write 16-word block, hash_stop), MSG_FIFO, DIGEST_0 through DIGEST_15, MSG_LENGTH_LOWER, MSG_LENGTH_UPPER, INTR_STATE (hmac_done), STATUS (hmac_idle) | tl_socket, intr_hmac_done | Positive |
| 44 | test_context_restore_basic | Restore previously saved context by writing DIGEST and MSG_LENGTH registers, issue hash_continue, complete remaining message, verify final digest matches uninterrupted hash | CFG, DIGEST_0 through DIGEST_15 (write saved values), MSG_LENGTH_LOWER, MSG_LENGTH_UPPER (write saved values), CMD (hash_continue, write remaining message, hash_process), DIGEST_0 through DIGEST_7 | tl_socket, intr_hmac_done | Positive |
| 45 | test_context_switch_two_streams | Implement two-stream interleaved processing: start stream A, pause at block boundary, save context; start stream B, complete stream B; restore stream A context, complete stream A; verify both digests are correct | CFG, CMD (hash_start, hash_stop, hash_continue for both streams), MSG_FIFO, DIGEST_0 through DIGEST_15 (save/restore), MSG_LENGTH_LOWER/UPPER, INTR_STATE (hmac_done) | tl_socket, intr_hmac_done | Positive |
| 46 | test_context_switch_hmac_mode | Perform context switching in HMAC mode, verify KEY registers persist across context switches and HMAC computation completes correctly after resume | CFG (hmac_en=1), KEY_0 through KEY_7, CMD (hash_start, hash_stop, hash_continue), MSG_FIFO, DIGEST_0 through DIGEST_7, MSG_LENGTH_LOWER/UPPER | tl_socket, intr_hmac_done | Positive |
| 47 | test_context_sha_en_disable_clear | Disable SHA engine by writing CFG.sha_en=0, verify DIGEST registers are cleared to prevent context leakage | CFG (sha_en=0), DIGEST_0 through DIGEST_15 (read to verify clear) | tl_socket | Positive |
| 48 | test_fifo_back_pressure | Fill MSG_FIFO to capacity (16 words for SHA-2 256), write additional word, verify TLM transaction is delayed until block processing creates space | CFG, CMD (hash_start), MSG_FIFO (17 word writes), STATUS (fifo_full, fifo_depth monitoring), measure transaction latency | tl_socket, intr_hmac_done | Positive |
| 49 | test_fifo_wraparound | Write multiple blocks to FIFO continuously, verify FIFO depth correctly wraps and block processing maintains data integrity | CFG, CMD, MSG_FIFO (continuous writes exceeding FIFO capacity), STATUS (fifo_depth), DIGEST_0 through DIGEST_7 | tl_socket, intr_hmac_done | Positive |
| 50 | test_partial_block_deadlock | Write partial block (e.g., 10 words for SHA-2 256 instead of full 16-word block), issue hash_stop, verify engine waits indefinitely as expected (test timeout mechanism) | CFG, CMD (hash_start, write 10 words to MSG_FIFO, hash_stop), STATUS (hmac_idle should remain 0), timeout detection | tl_socket | Negative |
| 51 | test_cfg_write_protection | Attempt to write CFG register while engine is processing (STATUS.hmac_idle=0), verify write is silently ignored without error | CFG (initial write), CMD (hash_start to enter processing), CFG (second write during processing), STATUS (hmac_idle), verify CFG unchanged | tl_socket | Positive |
| 52 | test_digest_write_protection | Attempt to write DIGEST registers while engine is processing, verify writes are rejected (unpredictable behavior prevention) | CFG, CMD (hash_start), DIGEST_0 (write during processing), STATUS (hmac_idle) | tl_socket | Negative |
| 53 | test_key_register_writeonly | Write values to KEY registers, attempt reads, verify reads return zero or undefined values (not actual key data) | KEY_0 through KEY_7 (write known values, then read) | tl_socket | Positive |
| 54 | test_wipe_secret | Write pattern to WIPE_SECRET register, verify all KEY and DIGEST registers are overwritten with the wipe pattern | WIPE_SECRET (write 32-bit pattern), KEY_0 through KEY_31 (verify cannot read back actual values), DIGEST_0 through DIGEST_15 (read to check if wiped) | tl_socket | Positive |
| 55 | test_digest_read_before_completion | Read DIGEST registers before hash_process completes (before hmac_done asserts), verify stale values returned, then read after completion to verify correct digest | CFG, CMD (hash_start, hash_process), DIGEST_0 through DIGEST_7 (read before and after completion), INTR_STATE (hmac_done) | tl_socket, intr_hmac_done | Positive |
| 56 | test_digest_register_count_sha256 | Compute SHA-2 256 hash, verify only DIGEST_0 through DIGEST_7 contain valid result, DIGEST_8 through DIGEST_15 contain irrelevant data | CFG (digest_size=SHA2_256), CMD, MSG_FIFO, DIGEST_0 through DIGEST_15 (read all, validate only 0-7) | tl_socket, intr_hmac_done | Positive |
| 57 | test_digest_register_count_sha384 | Compute SHA-2 384 hash, verify only DIGEST_0 through DIGEST_11 contain valid result, DIGEST_12 through DIGEST_15 are truncated | CFG (digest_size=SHA2_384), CMD, MSG_FIFO, DIGEST_0 through DIGEST_15 (read all, validate only 0-11) | tl_socket, intr_hmac_done | Positive |
| 58 | test_digest_register_count_sha512 | Compute SHA-2 512 hash, verify all DIGEST_0 through DIGEST_15 contain valid result | CFG (digest_size=SHA2_512), CMD, MSG_FIFO, DIGEST_0 through DIGEST_15 (read all, validate all 16 registers) | tl_socket, intr_hmac_done | Positive |
| 59 | test_sha256_block_latency | Measure timing from hash_process command to hmac_done interrupt assertion, verify latency matches expected 80 cycles per block for SHA-2 256 | CFG (digest_size=SHA2_256), CMD (hash_process), measure time to INTR_STATE.hmac_done=1, clk_i for timing reference | tl_socket, intr_hmac_done, clk_i | Positive |
| 60 | test_sha384_block_latency | Measure block processing latency for SHA-2 384/512 mode, verify 96 cycles per 1024-bit block | CFG (digest_size=SHA2_384), CMD, measure timing to hmac_done, clk_i | tl_socket, intr_hmac_done, clk_i | Positive |
| 61 | test_hmac_overhead | Compute HMAC hash and measure total timing, verify additional 240 cycles overhead for two-round HMAC computation beyond base hash processing | CFG (hmac_en=1), CMD, MSG_FIFO, measure timing, compare to SHA-2 only timing, clk_i | tl_socket, intr_hmac_done, clk_i | Positive |
| 62 | test_throughput_small_message | Measure throughput for small message (64 bytes, 1 block for SHA-2 256), calculate effective bytes per second | CFG, CMD, MSG_FIFO, measure total time from hash_start to digest ready, clk_i | tl_socket, intr_hmac_done, clk_i | Positive |
| 63 | test_throughput_large_message | Measure throughput for large message (1024 bytes, 16 blocks for SHA-2 256), verify throughput scales with message size | CFG, CMD, MSG_FIFO, measure total time, calculate throughput, clk_i | tl_socket, intr_hmac_done, clk_i | Positive |
| 64 | test_nist_sha256_vectors | Validate SHA-2 256 implementation using NIST SHA-2 test vectors with messages of various lengths, compare DIGEST output against expected results | CFG (digest_size=SHA2_256), CMD, MSG_FIFO (NIST test messages), DIGEST_0 through DIGEST_7 (compare against NIST expected digests) | tl_socket, intr_hmac_done | Positive |
| 65 | test_nist_sha384_vectors | Validate SHA-2 384 implementation using NIST test vectors, verify correct 384-bit digest computation | CFG (digest_size=SHA2_384), CMD, MSG_FIFO, DIGEST_0 through DIGEST_11 (compare against NIST) | tl_socket, intr_hmac_done | Positive |
| 66 | test_nist_sha512_vectors | Validate SHA-2 512 implementation using NIST test vectors, verify correct 512-bit digest computation | CFG (digest_size=SHA2_512), CMD, MSG_FIFO, DIGEST_0 through DIGEST_15 (compare against NIST) | tl_socket, intr_hmac_done | Positive |
| 67 | test_rfc4231_hmac_vectors | Validate HMAC implementation using RFC 4231 test vectors covering various key and message lengths, compare authentication tags against expected | CFG (hmac_en=1, various key_length), KEY registers, CMD, MSG_FIFO (RFC test messages), DIGEST_0 through DIGEST_7 (compare against RFC expected tags) | tl_socket, intr_hmac_done | Positive |
| 68 | test_botan_integration_sha256 | Verify OpenSSL library integration for SHA-2 256: initialize OpenSSL context, process blocks, finalize hash, retrieve digest correctly | CFG (digest_size=SHA2_256), CMD (hash_start initializes OpenSSL, hash_process invokes OpenSSL final), MSG_FIFO (triggers OpenSSL update), DIGEST_0 through DIGEST_7 | tl_socket, intr_hmac_done | Positive |
| 69 | test_botan_integration_hmac | Verify OpenSSL HMAC context creation with key data from KEY registers, two-round processing, and authentication tag retrieval | CFG (hmac_en=1), KEY_0 through KEY_7, CMD, MSG_FIFO, DIGEST_0 through DIGEST_7 | tl_socket, intr_hmac_done | Positive |
| 70 | test_msg_fifo_address_window | Write message data to various addresses within MSG_FIFO window (0x1000, 0x1004, 0x1FFC), verify all writes are directed to FIFO correctly | CFG, CMD, MSG_FIFO (writes to different addresses in 0x1000-0x1FFF range), DIGEST_0 through DIGEST_7 | tl_socket, intr_hmac_done | Positive |
| 71 | test_status_hmac_idle_transitions | Monitor STATUS.hmac_idle flag transitions during state changes: IDLE(1) -> PROCESSING(0) on hash_start, PROCESSING(0) -> IDLE(1) on hash_process completion | CFG, CMD (hash_start, hash_process), STATUS (hmac_idle repeated reads) | tl_socket, intr_hmac_done | Positive |
| 72 | test_command_self_clearing | Write commands to CMD register (hash_start, hash_process, hash_stop, hash_continue), verify all bits self-clear after processing (read CMD returns 0x0) | CMD (write commands), read CMD after each command | tl_socket | Positive |
| 73 | test_rapid_command_sequence | Issue rapid command sequences (hash_start immediately followed by hash_process with minimal data), verify model handles without errors | CFG, CMD (rapid hash_start then hash_process), MSG_FIFO (minimal writes), DIGEST_0 through DIGEST_7 | tl_socket, intr_hmac_done | Positive |
| 74 | test_concurrent_register_access | Simulate concurrent accesses to different registers (e.g., reading STATUS while writing MSG_FIFO), verify no race conditions or data corruption | STATUS (reads), MSG_FIFO (writes), CFG, CMD | tl_socket | Positive |
| 75 | test_reset_during_processing | Assert reset signal while engine is in PROCESSING state, verify operation is aborted immediately, state returns to IDLE, all registers reset | CFG, CMD (hash_start), MSG_FIFO (partial message), rst_ni (assert reset), verify STATUS, INTR_STATE, all registers | rst_ni, tl_socket, all interrupt ports | Positive |

---

## 4. Test Coverage Matrix

| Feature Category | Number of Tests | Coverage |
|-----------------|----------------|----------|
| Reset and Initialization | 2 | Reset mechanisms, power-on state |
| Register Access | 4 | Read-only, write-only, read-write, reserved fields |
| SHA-2 Modes | 4 | SHA-256, SHA-384, SHA-512, empty message |
| HMAC Modes | 5 | All key lengths (128, 256, 384, 512, 1024 bits) |
| Message Processing | 9 | Various lengths, subword writes, endian swap, FIFO management |
| Interrupts | 7 | All three interrupt types, masking, clearing, injection |
| Error Conditions | 9 | All six error codes, recovery, multiple errors |
| Context Switching | 5 | Save, restore, multi-stream, HMAC mode, context isolation |
| Corner Cases | 11 | Back-pressure, wraparound, deadlock, protection, boundary cases |
| Performance/Timing | 5 | Block latency, HMAC overhead, throughput measurement |
| Cryptographic Correctness | 6 | NIST vectors, RFC vectors, OpenSSL integration |
| Additional Functional | 8 | Address window, status monitoring, commands, special sequences |

**Total Test Cases: 75**

---

## 5. Test Execution Strategy

### 5.1 Test Phases

**Phase 1: Smoke Tests**
- Execute tests 1, 2, 3, 4, 6, 26 to verify basic functionality
- Ensure model compiles, resets correctly, and performs simple hash operation

**Phase 2: Functional Tests**
- Execute all SHA-2 mode tests (6-9, 15-21)
- Execute all HMAC mode tests (10-14)
- Execute message processing tests (16-25)

**Phase 3: Interrupt and Error Tests**
- Execute all interrupt tests (26-32)
- Execute all error condition tests (33-42)

**Phase 4: Context Switching Tests**
- Execute all context switching tests (43-47)

**Phase 5: Corner Case Tests**
- Execute all corner case tests (48-54)

**Phase 6: Performance Tests**
- Execute all timing and throughput tests (59-63)

**Phase 7: Cryptographic Validation**
- Execute all test vector validation tests (64-69)

**Phase 8: Additional Verification**
- Execute remaining functional tests (70-75)

### 5.2 Pass/Fail Criteria

**Test Pass Criteria:**
- All register values match expected values
- All interrupts assert and deassert as specified
- All error codes are correctly reported
- Digest results match expected values (for test vectors)
- Timing measurements fall within acceptable tolerance (±5%)
- No unexpected errors or assertions fired

**Test Fail Criteria:**
- Register value mismatch
- Missing or incorrect interrupt assertion
- Wrong error code reported
- Digest mismatch against expected value
- Timing outside tolerance range
- Model crashes or hangs
- Assertions fired

### 5.3 Test Logging and Reporting

Each test case shall log:
- Test case ID and name
- Test start timestamp
- All register writes and reads with addresses and data
- All interrupt assertions and deassertions
- All error codes observed
- Timing measurements where applicable
- Expected vs. actual results comparison
- Test verdict (PASS/FAIL)
- Test end timestamp
- Failure diagnostics if applicable

---

## 6. Test Environment Setup

### 6.1 Required Components

- SystemC TLM2.0 simulation environment
- HMAC IP SystemC model with OpenSSL library integration
- Testbench with TLM initiator for register access
- Interrupt monitoring logic for all three interrupt ports
- Timing measurement capability using SystemC simulation time
- Test vector libraries (NIST SHA-2, RFC 4231 HMAC)
- Logging and reporting infrastructure

### 6.2 Configuration Parameters

Set the following build-time parameters:
- MSG_FIFO_DEPTH_SHA256 = 16
- MSG_FIFO_DEPTH_SHA384_512 = 32
- MSG_FIFO_WINDOW_SIZE = 4096
- SHA256_BLOCK_CYCLES = 80
- SHA384_512_BLOCK_CYCLES = 96
- HMAC_EXTRA_CYCLES = 240
- NUM_DIGEST_REGS = 16
- NUM_KEY_REGS = 32

### 6.3 Simulation Settings

- Clock frequency: 100 MHz (10 ns period) for timing tests
- Simulation timeout: 10 ms per test (configurable)
- Verbosity level: INFO for normal execution, DEBUG for failures
- Random seed: Fixed for reproducibility, varied for coverage expansion

---

## 7. Conclusion

This test plan provides comprehensive coverage of the HMAC IP SystemC TLM2.0 model verification. The 75 test cases span all functional features, register access patterns, error conditions, interrupt behavior, context switching, corner cases, timing characteristics, and cryptographic correctness. Successful execution of this test plan validates the model's readiness for integration into virtual platform environments and early software development.

All test cases are designed as plain text narratives describing validation objectives, register programming sequences, and expected behaviors without code implementation, enabling clear communication of verification requirements to the verification team.

---

**End of Test Plan**
