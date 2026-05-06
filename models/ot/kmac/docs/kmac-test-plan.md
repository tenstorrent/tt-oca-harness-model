# KMAC SystemC TLM Model Test Plan

## 1. Test Plan Overview

This document provides a comprehensive test plan for the KMAC SystemC TLM model, covering all functional aspects including cryptographic operations, register access patterns, state machine transitions, application interfaces, entropy handling, key management, error injection, and corner cases.

### 1.1 Test Objectives

- Validate functional correctness of SHA3, SHAKE, cSHAKE, and KMAC cryptographic operations
- Verify register access patterns including reset values, access types, and protection mechanisms
- Validate state machine transitions (IDLE → ABSORB → SQUEEZE → IDLE)
- Test command validation including sparse encoding
- Verify application interface operations (KeyMgr, LC_CTRL, ROM_CTRL)
- Validate entropy handling in EDN mode, SW mode, and timeout conditions
- Test FIFO management including full/empty conditions and backpressure
- Verify key management including software keys, sideload, and zeroization
- Validate all error conditions and recovery mechanisms
- Test register callbacks and side effects
- Verify corner cases and boundary conditions
- Validate security features including escalation and key protection

### 1.2 Test Environment

- SystemC TLM2.0 testbench
- OpenSSL library for reference cryptographic calculations
- TLM target socket for MMIO register access
- Custom interfaces for KeyMgr and application exports

### 1.3 Port and Signal Names Reference

All test cases use exact port/signal names from `/home/shravanr/Documents/tvastaavp/kmac/docs/sections/kmac-port-interfaces.md`:

**Primary Ports:**
- `tl_socket` - TLM target socket for register access
- `keymgr_key_export` - Key Manager sideload interface
- `app_export[0]`, `app_export[1]`, `app_export[2]` - Application interfaces (KeyMgr, LC_CTRL, ROM_CTRL)
- `entropy_port` - Entropy request/response interface
- `idle_o` - Idle status output signal
- `lc_escalate_en_i` - Life cycle escalation input signal
- `clk_i` - Primary clock frequency input
- `clk_edn_i` - EDN interface clock frequency input

## 2. Test Plan Table

| Sl. No. | TestCase Name | Description | Registers Programmed | Ports/Signals Used | Test Type |
| ------- | ------------- | ----------- | -------------------- | ------------------ | --------- |
| **Register Access Tests** |
| 1 | test_reg_reset_values | Verify all registers reset to correct default values on initialization | All registers (INTR_STATE, INTR_ENABLE, INTR_TEST, ALERT_TEST, CFG_REGWEN, CFG_SHADOWED, CMD, STATUS, ENTROPY_PERIOD, ENTROPY_REFRESH_HASH_CNT, ENTROPY_REFRESH_THRESHOLD_SHADOWED, ENTROPY_SEED, KEY_SHARE0_*, KEY_SHARE1_*, KEY_LEN, PREFIX_*, ERR_CODE, STATE, MSG_FIFO) | tl_socket | Positive |
| 2 | test_reg_reserved_bits_read | Verify reserved bits in all registers read as zero | CFG_SHADOWED, CMD, STATUS, ENTROPY_PERIOD, ENTROPY_REFRESH_HASH_CNT, ENTROPY_REFRESH_THRESHOLD_SHADOWED, KEY_LEN | tl_socket | Positive |
| 3 | test_reg_reserved_bits_write | Verify writes to reserved bits are ignored without side effects | CFG_SHADOWED, CMD, STATUS, ENTROPY_PERIOD, ENTROPY_REFRESH_THRESHOLD_SHADOWED, KEY_LEN | tl_socket | Negative |
| 4 | test_reg_access_types_ro | Verify read-only registers reject write attempts | CFG_REGWEN, STATUS, ENTROPY_REFRESH_HASH_CNT, ERR_CODE, STATE | tl_socket | Negative |
| 5 | test_reg_access_types_wo | Verify write-only registers return zero on read | INTR_TEST, ALERT_TEST, KEY_SHARE0_*, KEY_SHARE1_*, KEY_LEN, ENTROPY_SEED, MSG_FIFO | tl_socket | Positive |
| 6 | test_reg_access_types_rw1c | Verify write-1-to-clear behavior for interrupt status bits | INTR_STATE | tl_socket | Positive |
| 7 | test_reg_access_types_r0w1c | Verify read-0-write-1-clear self-clearing behavior for command bits | CMD | tl_socket | Positive |
| 8 | test_cfg_regwen_protection_enable | Verify protected registers are writable when CFG_REGWEN.en = 1 | CFG_REGWEN, CFG_SHADOWED, ENTROPY_PERIOD, ENTROPY_REFRESH_THRESHOLD_SHADOWED, KEY_SHARE0_0, KEY_SHARE1_0, KEY_LEN, PREFIX_0 | tl_socket | Positive |
| 9 | test_cfg_regwen_protection_disable | Verify protected registers reject writes when CFG_REGWEN.en = 0 | CFG_REGWEN, CFG_SHADOWED, ENTROPY_PERIOD, KEY_SHARE0_0, KEY_LEN, PREFIX_0 | tl_socket | Negative |
| 10 | test_cfg_regwen_auto_clear_on_start | Verify CFG_REGWEN.en automatically clears to 0 on START command | CFG_REGWEN, CMD, STATUS | tl_socket | Positive |
| 11 | test_cfg_regwen_auto_set_on_done | Verify CFG_REGWEN.en returns to 1 on DONE command | CFG_REGWEN, CMD, STATUS | tl_socket | Positive |
| 12 | test_shadow_register_cfg_valid_sequence | Verify CFG_SHADOWED accepts two consecutive identical writes | CFG_SHADOWED, STATUS | tl_socket | Positive |
| 13 | test_shadow_register_cfg_mismatch | Verify CFG_SHADOWED mismatch triggers ALERT_RECOV_CTRL_UPDATE_ERR | CFG_SHADOWED, STATUS | tl_socket | Negative |
| 14 | test_shadow_register_threshold_valid_sequence | Verify ENTROPY_REFRESH_THRESHOLD_SHADOWED accepts two consecutive identical writes | ENTROPY_REFRESH_THRESHOLD_SHADOWED, STATUS | tl_socket | Positive |
| 15 | test_shadow_register_threshold_mismatch | Verify ENTROPY_REFRESH_THRESHOLD_SHADOWED mismatch triggers ALERT_RECOV_CTRL_UPDATE_ERR | ENTROPY_REFRESH_THRESHOLD_SHADOWED, STATUS | tl_socket | Negative |
| **SHA3 Hash Function Tests** |
| 16 | test_sha3_224_hash_operation | Verify SHA3-224 hash computation with known test vectors | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 17 | test_sha3_256_hash_operation | Verify SHA3-256 hash computation with known test vectors | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 18 | test_sha3_384_hash_operation | Verify SHA3-384 hash computation with known test vectors | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 19 | test_sha3_512_hash_operation | Verify SHA3-512 hash computation with known test vectors | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 20 | test_sha3_empty_message | Verify SHA3-256 hash of empty message (START immediately followed by PROCESS) | CFG_SHADOWED, CMD, STATUS, STATE | tl_socket | Positive |
| 21 | test_sha3_single_block_message | Verify SHA3-256 hash with message size less than block rate (136 bytes) | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 22 | test_sha3_multi_block_message | Verify SHA3-256 hash with message size greater than block rate requiring multiple Keccak rounds | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 23 | test_sha3_padding_mechanism | Verify SHA3 padding pattern (10 followed by pad10*1) is correctly applied | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 24 | test_sha3_msg_endianness_little | Verify SHA3 operation with msg_endianness = 0 (little-endian) | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 25 | test_sha3_msg_endianness_big | Verify SHA3 operation with msg_endianness = 1 (big-endian byte-swap) | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 26 | test_sha3_state_endianness_little | Verify STATE read with state_endianness = 0 (little-endian) | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 27 | test_sha3_state_endianness_big | Verify STATE read with state_endianness = 1 (big-endian byte-swap) | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 28 | test_sha3_invalid_strength_l128 | Verify UnexpectedModeStrength error when SHA3 mode configured with L128 strength | CFG_SHADOWED, CMD, STATUS, ERR_CODE | tl_socket | Negative |
| **SHAKE Extendable Output Tests** |
| 29 | test_shake128_fixed_output | Verify SHAKE128 operation with fixed 256-bit output | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 30 | test_shake256_fixed_output | Verify SHAKE256 operation with fixed 512-bit output | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 31 | test_shake128_extended_output | Verify SHAKE128 extended output using RUN command for multiple blocks | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 32 | test_shake256_extended_output | Verify SHAKE256 extended output using RUN command for multiple blocks | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 33 | test_shake_padding_mechanism | Verify SHAKE padding pattern (1111 followed by pad10*1) is correctly applied | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 34 | test_shake_run_command_squeeze_state | Verify RUN command executes 24 Keccak rounds in SQUEEZE state | CFG_SHADOWED, CMD, STATUS, STATE | tl_socket | Positive |
| 35 | test_shake_invalid_strength_l224 | Verify UnexpectedModeStrength error when SHAKE mode configured with L224 strength | CFG_SHADOWED, CMD, STATUS, ERR_CODE | tl_socket | Negative |
| 36 | test_shake_invalid_strength_l384 | Verify UnexpectedModeStrength error when SHAKE mode configured with L384 strength | CFG_SHADOWED, CMD, STATUS, ERR_CODE | tl_socket | Negative |
| 37 | test_shake_invalid_strength_l512 | Verify UnexpectedModeStrength error when SHAKE mode configured with L512 strength | CFG_SHADOWED, CMD, STATUS, ERR_CODE | tl_socket | Negative |
| **cSHAKE Customizable Function Tests** |
| 38 | test_cshake128_with_customization | Verify cSHAKE128 operation with function name N and customization string S | CFG_SHADOWED, CMD, STATUS, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 39 | test_cshake256_with_customization | Verify cSHAKE256 operation with function name N and customization string S | CFG_SHADOWED, CMD, STATUS, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 40 | test_cshake_empty_customization | Verify cSHAKE operation with empty N and S (functionally equivalent to SHAKE) | CFG_SHADOWED, CMD, STATUS, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 41 | test_cshake_prefix_expansion | Verify PREFIX registers are expanded to full block before message absorption | CFG_SHADOWED, CMD, STATUS, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 42 | test_cshake_padding_mechanism | Verify cSHAKE padding pattern (00 followed by pad10*1) is correctly applied | CFG_SHADOWED, CMD, STATUS, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 43 | test_cshake_extended_output | Verify cSHAKE extended output using RUN command for arbitrary length | CFG_SHADOWED, CMD, STATUS, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 44 | test_cshake_prefix_protection | Verify PREFIX registers are protected by CFG_REGWEN and cannot be modified during operation | CFG_REGWEN, CFG_SHADOWED, CMD, STATUS, PREFIX_0 | tl_socket | Negative |
| **KMAC Message Authentication Tests** |
| 45 | test_kmac128_with_128bit_key | Verify KMAC128 operation with 128-bit software key | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 46 | test_kmac128_with_256bit_key | Verify KMAC128 operation with 256-bit software key | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 47 | test_kmac256_with_256bit_key | Verify KMAC256 operation with 256-bit software key | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 48 | test_kmac256_with_512bit_key | Verify KMAC256 operation with 512-bit software key | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 49 | test_kmac_key_encoding | Verify key block construction with left_encode(key_length_bits) prepended to key | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 50 | test_kmac_prefix_validation_correct | Verify KMAC accepts PREFIX starting with encode_string("KMAC") = 0x4d4b2001 | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 51 | test_kmac_prefix_validation_incorrect | Verify IncorrectFunctionName error when PREFIX does not start with encode_string("KMAC") | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_LEN, PREFIX_*, ERR_CODE | tl_socket | Negative |
| 52 | test_kmac_customization_string | Verify KMAC operation with non-empty customization string S in PREFIX registers | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 53 | test_kmac_output_length_encoding | Verify right_encode(output_length) is correctly processed (software responsibility) | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 54 | test_kmac_extended_output | Verify KMAC extended output using RUN command for tags longer than rate | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 55 | test_kmac_empty_message | Verify KMAC with empty message (only right_encode(output_length) in MSG_FIFO) | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| **Key Management Tests** |
| 56 | test_key_sw_single_share_128bit | Verify 128-bit key input via KEY_SHARE0 registers (EnMasking=0, SwKeyMasked=0) | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 57 | test_key_sw_single_share_192bit | Verify 192-bit key input via KEY_SHARE0 registers | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 58 | test_key_sw_single_share_256bit | Verify 256-bit key input via KEY_SHARE0 registers | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 59 | test_key_sw_single_share_384bit | Verify 384-bit key input via KEY_SHARE0 registers | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 60 | test_key_sw_single_share_512bit | Verify 512-bit key input via KEY_SHARE0 registers | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 61 | test_key_sw_dual_share_masked | Verify two-share masked key input via KEY_SHARE0 and KEY_SHARE1 (EnMasking=1) | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_SHARE1_*, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 62 | test_key_sw_dual_share_unmasked_xor | Verify two-share XOR for unmasked key (EnMasking=0, SwKeyMasked=1) | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_SHARE1_*, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 63 | test_key_share1_ignored_when_disabled | Verify KEY_SHARE1 writes are ignored when EnMasking=0 and SwKeyMasked=0 | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_SHARE1_*, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 64 | test_key_sideload_from_keymgr | Verify sideloaded key usage from KeyMgr via keymgr_key_export when CFG_SHADOWED.sideload = 1 | CFG_SHADOWED, CMD, STATUS, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket, keymgr_key_export | Positive |
| 65 | test_key_sideload_not_valid_error | Verify KeyNotValid error (0x01) when sideloaded key requested but keymgr_key_export.is_key_valid() returns false | CFG_SHADOWED, CMD, STATUS, ERR_CODE | tl_socket, keymgr_key_export | Negative |
| 66 | test_key_persistence_across_operations | Verify KEY_SHARE registers retain values across multiple KMAC operations | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 67 | test_key_zeroization_on_reset | Verify KEY_SHARE registers are cleared to zero on hardware reset | KEY_SHARE0_*, KEY_SHARE1_* | tl_socket | Positive |
| 68 | test_key_zeroization_on_fatal_error | Verify KEY_SHARE registers are cleared to zero on fatal ALERT_FATAL_FAULT error | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_SHARE1_*, ERR_CODE | tl_socket | Positive |
| 69 | test_key_zeroization_on_escalation | Verify KEY_SHARE registers are immediately cleared to zero when lc_escalate_en_i asserts | KEY_SHARE0_*, KEY_SHARE1_*, STATUS | tl_socket, lc_escalate_en_i | Positive |
| 70 | test_key_protection_cfg_regwen | Verify KEY_SHARE registers are protected by CFG_REGWEN and cannot be modified during operation | CFG_REGWEN, CMD, STATUS, KEY_SHARE0_0, KEY_SHARE1_0 | tl_socket | Negative |
| **State Machine Transition Tests** |
| 71 | test_fsm_idle_state_initial | Verify FSM enters IDLE state on reset with STATUS.sha3_idle = 1 | STATUS | tl_socket | Positive |
| 72 | test_fsm_idle_to_absorb_on_start | Verify START command (0x1D) transitions FSM from IDLE to ABSORB state | CFG_SHADOWED, CMD, STATUS | tl_socket | Positive |
| 73 | test_fsm_absorb_to_squeeze_on_process | Verify PROCESS command (0x2E) transitions FSM from ABSORB to SQUEEZE state | CFG_SHADOWED, CMD, STATUS, MSG_FIFO | tl_socket | Positive |
| 74 | test_fsm_squeeze_to_idle_on_done | Verify DONE command (0x16) transitions FSM from SQUEEZE to IDLE state | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 75 | test_fsm_absorb_to_idle_on_done_error_recovery | Verify DONE command can transition from ABSORB to IDLE for error recovery | CFG_SHADOWED, CMD, STATUS, MSG_FIFO | tl_socket | Positive |
| 76 | test_fsm_run_command_in_squeeze | Verify RUN command (0x31) executes additional Keccak rounds while remaining in SQUEEZE state | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 77 | test_fsm_status_bit_mutual_exclusion | Verify sha3_idle, sha3_absorb, sha3_squeeze status bits are mutually exclusive | CFG_SHADOWED, CMD, STATUS, MSG_FIFO | tl_socket | Positive |
| 78 | test_fsm_idle_state_operations_permitted | Verify operations permitted in IDLE state (CFG_SHADOWED write, KEY write, START command) | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_0 | tl_socket | Positive |
| 79 | test_fsm_idle_state_operations_blocked | Verify operations blocked in IDLE state (PROCESS, RUN, DONE commands) | CMD, STATUS, ERR_CODE | tl_socket | Negative |
| 80 | test_fsm_absorb_state_operations_permitted | Verify operations permitted in ABSORB state (MSG_FIFO write, PROCESS command) | CFG_SHADOWED, CMD, STATUS, MSG_FIFO | tl_socket | Positive |
| 81 | test_fsm_absorb_state_operations_blocked | Verify operations blocked in ABSORB state (START, RUN, DONE commands, CFG_SHADOWED write) | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, ERR_CODE | tl_socket | Negative |
| 82 | test_fsm_squeeze_state_operations_permitted | Verify operations permitted in SQUEEZE state (STATE read, RUN command, DONE command) | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 83 | test_fsm_squeeze_state_operations_blocked | Verify operations blocked in SQUEEZE state (START, PROCESS commands) | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, ERR_CODE | tl_socket | Negative |
| **Command Validation Tests** |
| 84 | test_cmd_sparse_encoding_start_valid | Verify START command accepts valid sparse encoding 0x1D | CMD, STATUS | tl_socket | Positive |
| 85 | test_cmd_sparse_encoding_process_valid | Verify PROCESS command accepts valid sparse encoding 0x2E | CFG_SHADOWED, CMD, STATUS, MSG_FIFO | tl_socket | Positive |
| 86 | test_cmd_sparse_encoding_run_valid | Verify RUN command accepts valid sparse encoding 0x31 | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 87 | test_cmd_sparse_encoding_done_valid | Verify DONE command accepts valid sparse encoding 0x16 | CFG_SHADOWED, CMD, STATUS, MSG_FIFO | tl_socket | Positive |
| 88 | test_cmd_sparse_encoding_invalid | Verify invalid command encodings (not 0x1D, 0x2E, 0x31, 0x16) are rejected | CMD, STATUS, ERR_CODE | tl_socket | Negative |
| 89 | test_cmd_entropy_req_bit | Verify CMD.entropy_req bit (bit 8) triggers PRNG reseed in EDN mode | CFG_SHADOWED, CMD, STATUS, ENTROPY_REFRESH_HASH_CNT | tl_socket, entropy_port | Positive |
| 90 | test_cmd_hash_cnt_clr_bit | Verify CMD.hash_cnt_clr bit (bit 9) clears ENTROPY_REFRESH_HASH_CNT to 0 | CMD, ENTROPY_REFRESH_HASH_CNT | tl_socket | Positive |
| 91 | test_cmd_err_processed_bit | Verify CMD.err_processed bit (bit 10) recovers FSM from error state to IDLE | CFG_SHADOWED, CMD, STATUS, ERR_CODE | tl_socket | Positive |
| 92 | test_cmd_self_clearing_behavior | Verify all CMD register bits self-clear after action completes (read returns 0) | CMD | tl_socket | Positive |
| **Application Interface Tests** |
| 93 | test_app_keymgr_kmac_operation | Verify KeyMgr application interface (app_export[0]) performs KMAC operation with sideloaded key | CFG_SHADOWED, STATUS, STATE | tl_socket, app_export[0], keymgr_key_export | Positive |
| 94 | test_app_lc_ctrl_cshake128_operation | Verify LC_CTRL application interface (app_export[1]) performs cSHAKE128 with compile-time prefix "LC_CTRL" | STATUS, STATE | tl_socket, app_export[1] | Positive |
| 95 | test_app_rom_ctrl_cshake256_operation | Verify ROM_CTRL application interface (app_export[2]) performs cSHAKE256 with compile-time prefix "ROM_CTRL" | STATUS, STATE | tl_socket, app_export[2] | Positive |
| 96 | test_app_fixed_priority_arbitration | Verify fixed-priority arbitration when multiple applications request simultaneously (KeyMgr highest priority) | STATUS | app_export[0], app_export[1], app_export[2] | Positive |
| 97 | test_app_keymgr_data_interface | Verify KeyMgr application interface 64-bit data transfer with strobe and last beat indicator | STATUS, STATE | app_export[0], keymgr_key_export | Positive |
| 98 | test_app_digest_two_share_output | Verify application interface returns digest in two shares (share0, share1) | STATUS, STATE | app_export[0], keymgr_key_export | Positive |
| 99 | test_app_sw_lockout_during_app_active | Verify software MMIO access is blocked when application interface is active | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, ERR_CODE | tl_socket, app_export[0] | Negative |
| 100 | test_app_cmd_rejected_during_app_active | Verify SwIssuedCmdInAppActive error (0x03) when software writes CMD during application operation | CMD, STATUS, ERR_CODE | tl_socket, app_export[0] | Negative |
| 101 | test_app_state_read_blocked_during_app_active | Verify STATE window reads return 0 when application interface is active (key protection) | STATUS, STATE | tl_socket, app_export[0], keymgr_key_export | Positive |
| 102 | test_app_keymgr_automatic_output_length | Verify KeyMgr interface automatically appends right_encode(256) for KMAC mode | STATUS, STATE | app_export[0], keymgr_key_export | Positive |
| 103 | test_app_error_indication | Verify application interface error indication when KeyNotValid or other errors occur | STATUS, ERR_CODE | tl_socket, app_export[0], keymgr_key_export | Negative |
| **Entropy Handling Tests** |
| 104 | test_entropy_mode_idle | Verify entropy_mode = 0x0 (idle_mode) disables entropy generation | CFG_SHADOWED, STATUS | tl_socket | Positive |
| 105 | test_entropy_mode_edn_request | Verify entropy_mode = 0x1 (edn_mode) fetches entropy from EDN via entropy_port | CFG_SHADOWED, CMD, STATUS, ENTROPY_REFRESH_HASH_CNT | tl_socket, entropy_port | Positive |
| 106 | test_entropy_mode_sw_seed | Verify entropy_mode = 0x2 (sw_mode) uses software-provided seed via ENTROPY_SEED registers | CFG_SHADOWED, ENTROPY_SEED, CMD, STATUS | tl_socket | Positive |
| 107 | test_entropy_ready_assertion | Verify entropy_ready bit triggers entropy subsystem initialization and locks entropy_mode | CFG_SHADOWED, STATUS | tl_socket | Positive |
| 108 | test_entropy_mode_lock_after_ready | Verify entropy_mode configuration locks after entropy_ready asserted and first operation starts | CFG_SHADOWED, CMD, STATUS | tl_socket | Positive |
| 109 | test_entropy_timeout_edn_mode | Verify WaitTimerExpired error (0x04) when EDN does not respond within ENTROPY_PERIOD timeout | CFG_SHADOWED, CMD, STATUS, ENTROPY_PERIOD, ERR_CODE | tl_socket, entropy_port | Negative |
| 110 | test_entropy_timeout_recovery | Verify entropy timeout recovery by de-asserting entropy_ready and setting CMD.err_processed | CFG_SHADOWED, CMD, STATUS, ENTROPY_PERIOD, ERR_CODE | tl_socket, entropy_port | Positive |
| 111 | test_entropy_period_prescaler | Verify ENTROPY_PERIOD.prescaler controls timer pulse generation frequency | ENTROPY_PERIOD | tl_socket | Positive |
| 112 | test_entropy_period_wait_timer | Verify ENTROPY_PERIOD.wait_timer controls timeout duration in timer pulses | ENTROPY_PERIOD, ERR_CODE | tl_socket, entropy_port | Positive |
| 113 | test_entropy_refresh_hash_cnt | Verify ENTROPY_REFRESH_HASH_CNT increments on each KMAC operation completion | CFG_SHADOWED, CMD, STATUS, ENTROPY_REFRESH_HASH_CNT, KEY_SHARE0_*, KEY_LEN, PREFIX_*, MSG_FIFO | tl_socket | Positive |
| 114 | test_entropy_refresh_threshold_trigger | Verify automatic PRNG reseed when ENTROPY_REFRESH_HASH_CNT reaches ENTROPY_REFRESH_THRESHOLD_SHADOWED | CFG_SHADOWED, CMD, STATUS, ENTROPY_REFRESH_HASH_CNT, ENTROPY_REFRESH_THRESHOLD_SHADOWED, KEY_SHARE0_*, KEY_LEN, PREFIX_*, MSG_FIFO | tl_socket, entropy_port | Positive |
| 115 | test_entropy_refresh_threshold_zero_disable | Verify zero threshold in ENTROPY_REFRESH_THRESHOLD_SHADOWED disables automatic refresh | ENTROPY_REFRESH_THRESHOLD_SHADOWED, CFG_SHADOWED, CMD, STATUS, ENTROPY_REFRESH_HASH_CNT | tl_socket | Positive |
| 116 | test_entropy_incorrect_mode_error | Verify IncorrectEntropyMode error (0x05) when entropy_ready set but entropy_mode invalid | CFG_SHADOWED, STATUS, ERR_CODE | tl_socket | Negative |
| 117 | test_entropy_hashing_without_ready_error | Verify SwHashingWithoutEntropyReady error (0x09) when KMAC operation attempted with masking enabled but entropy not ready | CFG_SHADOWED, CMD, STATUS, ERR_CODE | tl_socket | Negative |
| 118 | test_entropy_fast_process_blocking | Verify entropy_fast_process = 0 blocks operations until entropy ready | CFG_SHADOWED, CMD, STATUS | tl_socket, entropy_port | Positive |
| 119 | test_entropy_fast_process_nonblocking | Verify entropy_fast_process = 1 allows operations to proceed without waiting for entropy | CFG_SHADOWED, CMD, STATUS | tl_socket | Positive |
| **FIFO Management Tests** |
| 120 | test_fifo_depth_tracking | Verify STATUS.fifo_depth accurately reflects occupied entries in MSG_FIFO | CFG_SHADOWED, CMD, STATUS, MSG_FIFO | tl_socket | Positive |
| 121 | test_fifo_empty_status_on_reset | Verify STATUS.fifo_empty = 1 on reset | STATUS | tl_socket | Positive |
| 122 | test_fifo_empty_to_nonempty_transition | Verify STATUS.fifo_empty transitions from 1 to 0 when MSG_FIFO receives data | CFG_SHADOWED, CMD, STATUS, MSG_FIFO | tl_socket | Positive |
| 123 | test_fifo_nonempty_to_empty_transition | Verify STATUS.fifo_empty transitions from 0 to 1 when MSG_FIFO is drained | CFG_SHADOWED, CMD, STATUS, MSG_FIFO | tl_socket | Positive |
| 124 | test_fifo_full_condition | Verify STATUS.fifo_full asserts when MSG_FIFO reaches maximum depth (MsgFifoDepth parameter) | CFG_SHADOWED, CMD, STATUS, MSG_FIFO | tl_socket | Positive |
| 125 | test_fifo_full_backpressure_blocking | Verify MSG_FIFO writes block (temporal decoupling wait) when FIFO is full until space available | CFG_SHADOWED, CMD, STATUS, MSG_FIFO | tl_socket | Positive |
| 126 | test_fifo_pass_through_mode | Verify MSG_FIFO pass-through behavior when SHA3 engine ready and FIFO empty | CFG_SHADOWED, CMD, STATUS, MSG_FIFO | tl_socket | Positive |
| 127 | test_fifo_address_window_abstraction | Verify any write to address range 0x800-0xFFC appends to MSG_FIFO regardless of specific address | CFG_SHADOWED, CMD, STATUS, MSG_FIFO | tl_socket | Positive |
| 128 | test_fifo_byte_write_support | Verify MSG_FIFO supports byte-granularity writes with internal packer | CFG_SHADOWED, CMD, STATUS, MSG_FIFO | tl_socket | Positive |
| 129 | test_fifo_halfword_write_support | Verify MSG_FIFO supports halfword (16-bit) writes with internal packer | CFG_SHADOWED, CMD, STATUS, MSG_FIFO | tl_socket | Positive |
| 130 | test_fifo_word_write_support | Verify MSG_FIFO supports word (32-bit) writes with internal packer | CFG_SHADOWED, CMD, STATUS, MSG_FIFO | tl_socket | Positive |
| 131 | test_fifo_packer_partial_entry_on_process | Verify internal packer flushes partial 64-bit entry with zero-padding on PROCESS command | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 132 | test_fifo_write_before_start_error | Verify SwPushedMsgFifo error (0x02) when MSG_FIFO written before START command | CMD, STATUS, MSG_FIFO, ERR_CODE | tl_socket | Negative |
| 133 | test_fifo_write_after_process_error | Verify SwPushedMsgFifo error (0x02) when MSG_FIFO written after PROCESS command | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, ERR_CODE | tl_socket | Negative |
| 134 | test_fifo_write_during_app_active_error | Verify SwPushedMsgFifo error (0x02) when MSG_FIFO written while application interface active | STATUS, MSG_FIFO, ERR_CODE | tl_socket, app_export[0] | Negative |
| **STATE Window Tests** |
| 135 | test_state_read_in_squeeze_state | Verify STATE window (0x400-0x5FC) contains valid digest when STATUS.sha3_squeeze = 1 | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 136 | test_state_read_in_idle_returns_zero | Verify STATE window returns 0 in IDLE state (key protection) | STATUS, STATE | tl_socket | Positive |
| 137 | test_state_read_in_absorb_returns_zero | Verify STATE window returns 0 in ABSORB state (key protection) | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 138 | test_state_single_share_unmasked | Verify STATE window returns single share at 0x400-0x4C7 when EnMasking = 0 | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 139 | test_state_two_share_masked | Verify STATE window returns two shares (0x400-0x4C7 state share, 0x500-0x5C7 mask share) when EnMasking = 1 | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 140 | test_state_share_xor_for_unmasked_digest | Verify software must XOR state share and mask share to obtain unmasked digest when EnMasking = 1 | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 141 | test_state_mask_region_zero_when_unmasked | Verify mask share region (0x500-0x5C7) reads as 0 when EnMasking = 0 | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 142 | test_state_reserved_regions_zero | Verify reserved regions (0x4C8-0x4FF, 0x5C8-0x5FF) read as 0 | STATE | tl_socket | Positive |
| 143 | test_state_endianness_application | Verify state_endianness byte-swap applied on 32-bit word granularity during STATE reads | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| **Error Detection and Reporting Tests** |
| 144 | test_err_code_keynotvalid_0x01 | Verify ERR_CODE = 0x01 (KeyNotValid) when application interface requests KMAC but sideloaded key not ready | CFG_SHADOWED, STATUS, ERR_CODE | tl_socket, app_export[0], keymgr_key_export | Negative |
| 145 | test_err_code_swpushedmsgfifo_0x02 | Verify ERR_CODE = 0x02 (SwPushedMsgFifo) when MSG_FIFO written outside ABSORB state | CMD, STATUS, MSG_FIFO, ERR_CODE | tl_socket | Negative |
| 146 | test_err_code_swissuedcmdinappactive_0x03 | Verify ERR_CODE = 0x03 (SwIssuedCmdInAppActive) when CMD written while application interface active | CMD, STATUS, ERR_CODE | tl_socket, app_export[0] | Negative |
| 147 | test_err_code_waittimerexpired_0x04 | Verify ERR_CODE = 0x04 (WaitTimerExpired) when EDN does not respond within ENTROPY_PERIOD timeout | CFG_SHADOWED, CMD, STATUS, ENTROPY_PERIOD, ERR_CODE | tl_socket, entropy_port | Negative |
| 148 | test_err_code_incorrectentropymode_0x05 | Verify ERR_CODE = 0x05 (IncorrectEntropyMode) when entropy_ready set but entropy_mode invalid | CFG_SHADOWED, STATUS, ERR_CODE | tl_socket | Negative |
| 149 | test_err_code_unexpectedmodestrength_0x06 | Verify ERR_CODE = 0x06 (UnexpectedModeStrength) when invalid mode/strength combination configured | CFG_SHADOWED, CMD, STATUS, ERR_CODE | tl_socket | Negative |
| 150 | test_err_code_incorrectfunctionname_0x07 | Verify ERR_CODE = 0x07 (IncorrectFunctionName) when KMAC PREFIX does not start with encode_string("KMAC") | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_LEN, PREFIX_*, ERR_CODE | tl_socket | Negative |
| 151 | test_err_code_swcmdsequence_0x08 | Verify ERR_CODE = 0x08 (SwCmdSequence) when commands issued out of required sequence | CFG_SHADOWED, CMD, STATUS, ERR_CODE | tl_socket | Negative |
| 152 | test_err_code_swhashingwithoutentropy_0x09 | Verify ERR_CODE = 0x09 (SwHashingWithoutEntropyReady) when KMAC operation with masking but entropy not ready | CFG_SHADOWED, CMD, STATUS, ERR_CODE | tl_socket | Negative |
| 153 | test_err_code_sha3control_0x80 | Verify ERR_CODE = 0x80 (Sha3Control) appears with SwCmdSequence error for internal FSM control error | CFG_SHADOWED, CMD, STATUS, ERR_CODE | tl_socket | Negative |
| 154 | test_err_code_persistence_across_interrupt_clear | Verify ERR_CODE persists after INTR_STATE.kmac_err cleared (allows software to re-read) | CMD, STATUS, INTR_STATE, ERR_CODE | tl_socket | Positive |
| 155 | test_err_code_cleared_on_done | Verify ERR_CODE cleared when DONE command successfully completes | CFG_SHADOWED, CMD, STATUS, ERR_CODE | tl_socket | Positive |
| 156 | test_error_recovery_sequence | Verify complete error recovery sequence: read ERR_CODE, clear INTR_STATE, set CMD.err_processed, wait sha3_idle | CFG_SHADOWED, CMD, STATUS, INTR_STATE, ERR_CODE | tl_socket | Positive |
| 157 | test_alert_fatal_fault_bit | Verify STATUS.ALERT_FATAL_FAULT indicates unrecoverable error (TL-UL integrity, shadow storage, counter, FSM, LFSR errors) | STATUS | tl_socket | Negative |
| 158 | test_alert_recov_ctrl_update_err_bit | Verify STATUS.ALERT_RECOV_CTRL_UPDATE_ERR indicates shadow register mismatch (recoverable) | CFG_SHADOWED, STATUS | tl_socket | Negative |
| **Register Callback Tests** |
| 159 | test_callback_cmd_write_start_side_effects | Verify handle_write_CMD for START command: transitions IDLE→ABSORB, clears CFG_REGWEN.en, processes PREFIX/key | CFG_REGWEN, CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_LEN, PREFIX_* | tl_socket | Positive |
| 160 | test_callback_cmd_write_process_side_effects | Verify handle_write_CMD for PROCESS command: applies padding, transitions ABSORB→SQUEEZE, updates STATUS.sha3_squeeze | CFG_SHADOWED, CMD, STATUS, MSG_FIFO | tl_socket | Positive |
| 161 | test_callback_cmd_write_run_side_effects | Verify handle_write_CMD for RUN command: executes 24 Keccak rounds, updates STATE window | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 162 | test_callback_cmd_write_done_side_effects | Verify handle_write_CMD for DONE command: zeros Keccak state, transitions to IDLE, sets CFG_REGWEN.en | CFG_REGWEN, CFG_SHADOWED, CMD, STATUS, MSG_FIFO | tl_socket | Positive |
| 163 | test_callback_cfg_shadowed_write_validation | Verify handle_write_CFG_SHADOWED: requires two identical writes, rejects during non-IDLE state, checks CFG_REGWEN | CFG_REGWEN, CFG_SHADOWED, CMD, STATUS | tl_socket | Positive |
| 164 | test_callback_cfg_shadowed_entropy_mode_lock | Verify handle_write_CFG_SHADOWED: entropy_mode locks after entropy_ready asserted and first operation | CFG_SHADOWED, CMD, STATUS | tl_socket | Positive |
| 165 | test_callback_entropy_refresh_threshold_validation | Verify handle_write_ENTROPY_REFRESH_THRESHOLD_SHADOWED: two identical writes required, mismatch triggers alert | ENTROPY_REFRESH_THRESHOLD_SHADOWED, STATUS | tl_socket | Positive |
| 166 | test_callback_msg_fifo_write_packing | Verify handle_write_MSG_FIFO: packs byte/halfword/word writes to 64-bit datapath, updates FIFO status | CFG_SHADOWED, CMD, STATUS, MSG_FIFO | tl_socket | Positive |
| 167 | test_callback_msg_fifo_write_backpressure | Verify handle_write_MSG_FIFO: temporal decoupling wait when FIFO full | CFG_SHADOWED, CMD, STATUS, MSG_FIFO | tl_socket | Positive |
| 168 | test_callback_intr_state_write_w1c | Verify handle_write_INTR_STATE: write-1-to-clear behavior for kmac_done and kmac_err bits | INTR_STATE | tl_socket | Positive |
| 169 | test_callback_status_read_dynamic | Verify handle_read_STATUS: dynamically constructs return value from FSM state and FIFO status | CFG_SHADOWED, CMD, STATUS, MSG_FIFO | tl_socket | Positive |
| 170 | test_callback_state_read_conditional_access | Verify handle_read_STATE: conditional access based on FSM state, returns 0 in IDLE/ABSORB, blocks during app active | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket, app_export[0] | Positive |
| **Corner Case and Boundary Tests** |
| 171 | test_corner_empty_message_sha3 | Verify SHA3 hash of empty message (START followed immediately by PROCESS) | CFG_SHADOWED, CMD, STATUS, STATE | tl_socket | Positive |
| 172 | test_corner_empty_message_kmac | Verify KMAC with empty message (only right_encode(output_length) written) | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 173 | test_corner_single_byte_message | Verify hash operation with 1-byte message | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 174 | test_corner_block_boundary_message | Verify hash operation with message exactly at block rate boundary (no partial block) | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 175 | test_corner_block_boundary_plus_one | Verify hash operation with message one byte beyond block rate boundary | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 176 | test_corner_large_message_multiple_blocks | Verify hash operation with large message requiring many Keccak rounds (e.g., 10 blocks) | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 177 | test_corner_maximum_key_length_512bit | Verify KMAC operation with maximum key length 512 bits | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 178 | test_corner_minimum_key_length_128bit | Verify KMAC operation with minimum key length 128 bits | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 179 | test_corner_maximum_prefix_length | Verify cSHAKE operation with maximum PREFIX length (352 bits across PREFIX_0 to PREFIX_10) | CFG_SHADOWED, CMD, STATUS, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 180 | test_corner_extended_output_maximum_runs | Verify SHAKE extended output with multiple RUN commands (e.g., 10 runs for 1680 bytes) | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 181 | test_corner_back_to_back_operations | Verify back-to-back hash operations without reconfiguration (reuse KEY, PREFIX, CFG) | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |
| 182 | test_corner_cfg_shadowed_write_interruption | Verify shadow register inconsistent state when second write interrupted by reset | CFG_SHADOWED, STATUS | tl_socket | Negative |
| 183 | test_corner_fifo_full_deadlock_prevention | Verify ENTROPY_PERIOD.wait_timer prevents deadlock when FIFO full during entropy request | CFG_SHADOWED, CMD, STATUS, ENTROPY_PERIOD, MSG_FIFO, ERR_CODE | tl_socket, entropy_port | Positive |
| 184 | test_corner_run_command_in_sha3_mode | Verify RUN command in SHA3 fixed-length mode has no effect beyond initial digest | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 185 | test_corner_escalation_during_operation | Verify lc_escalate_en_i assertion during hash operation immediately zeros state and blocks operations | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, MSG_FIFO, STATE | tl_socket, lc_escalate_en_i | Negative |
| 186 | test_corner_entropy_mode_change_after_lock | Verify entropy_mode writes after lock update storage but do not affect active entropy mode | CFG_SHADOWED, CMD, STATUS | tl_socket | Positive |
| 187 | test_corner_state_endianness_independent_computation | Verify state_endianness affects only STATE read format, not internal Keccak computation | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 188 | test_corner_msg_mask_with_prng | Verify msg_mask XORs message with PRNG output when entropy available | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket, entropy_port | Positive |
| 189 | test_corner_en_unsupported_modestrength | Verify en_unsupported_modestrength allows unsupported mode/strength combinations (development/test bit) | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| **Security Feature Tests** |
| 190 | test_security_key_protection_idle_absorb | Verify STATE window returns 0 during IDLE and ABSORB states to prevent key leakage | CFG_SHADOWED, CMD, STATUS, MSG_FIFO, STATE | tl_socket | Positive |
| 191 | test_security_key_protection_app_active | Verify STATE window access blocked when application interface active to prevent key observation | STATUS, STATE | tl_socket, app_export[0], keymgr_key_export | Positive |
| 192 | test_security_escalation_immediate_zeroization | Verify lc_escalate_en_i assertion immediately zeros all internal state and KEY registers | KEY_SHARE0_*, KEY_SHARE1_*, STATUS | tl_socket, lc_escalate_en_i | Positive |
| 193 | test_security_escalation_fsm_invalid_state | Verify lc_escalate_en_i assertion transitions all FSMs to invalid states | STATUS | tl_socket, lc_escalate_en_i | Positive |
| 194 | test_security_escalation_operation_blocking | Verify all operations blocked after lc_escalate_en_i assertion | CFG_SHADOWED, CMD, STATUS | tl_socket, lc_escalate_en_i | Negative |
| 195 | test_security_escalation_reset_recovery | Verify only reset can recover functionality after escalation condition | STATUS | tl_socket, lc_escalate_en_i | Positive |
| 196 | test_security_sideload_key_bypass_mmio | Verify sideloaded key from keymgr_key_export bypasses MMIO for secure key delivery | CFG_SHADOWED, CMD, STATUS, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket, keymgr_key_export | Positive |
| 197 | test_security_sparse_command_encoding | Verify sparse command encoding (0x1D, 0x2E, 0x31, 0x16) prevents glitch-induced command injection | CMD, STATUS, ERR_CODE | tl_socket | Negative |
| 198 | test_security_shadow_register_glitch_protection | Verify shadow registers (CFG_SHADOWED, ENTROPY_REFRESH_THRESHOLD_SHADOWED) protect against glitch attacks | CFG_SHADOWED, ENTROPY_REFRESH_THRESHOLD_SHADOWED, STATUS | tl_socket | Positive |
| 199 | test_security_idle_status_signaling | Verify idle_o signal accurately reflects KMAC idle state for external dependency checking | STATUS | tl_socket, idle_o | Positive |
| 200 | test_security_two_share_masking | Verify two-share boolean masking for keys and state when EnMasking = 1 | CFG_SHADOWED, CMD, STATUS, KEY_SHARE0_*, KEY_SHARE1_*, KEY_LEN, PREFIX_*, MSG_FIFO, STATE | tl_socket | Positive |

## 3. Test Execution Strategy

### 3.1 Test Grouping

Tests are organized into the following functional groups for efficient execution:

1. **Register Access Tests** (Tests 1-15): Basic register functionality
2. **Cryptographic Operation Tests** (Tests 16-55): SHA3, SHAKE, cSHAKE, KMAC modes
3. **Key Management Tests** (Tests 56-70): Software keys, sideload, zeroization
4. **State Machine Tests** (Tests 71-83): FSM transitions and state-dependent operations
5. **Command Validation Tests** (Tests 84-92): Sparse encoding and command bits
6. **Application Interface Tests** (Tests 93-103): KeyMgr, LC_CTRL, ROM_CTRL
7. **Entropy Handling Tests** (Tests 104-119): EDN mode, SW mode, timeout
8. **FIFO Management Tests** (Tests 120-134): Depth tracking, backpressure, errors
9. **STATE Window Tests** (Tests 135-143): Read access, masking, protection
10. **Error Detection Tests** (Tests 144-158): All 9 error codes and alerts
11. **Callback Verification Tests** (Tests 159-170): Register side effects
12. **Corner Case Tests** (Tests 171-189): Boundary conditions and edge cases
13. **Security Feature Tests** (Tests 190-200): Escalation, key protection, masking

### 3.2 Test Dependencies

- Cryptographic operation tests depend on register access tests
- Application interface tests depend on cryptographic operation tests
- Error detection tests can run independently
- Corner case tests should run after basic functional tests pass

### 3.3 Pass/Fail Criteria

Each test case has specific pass criteria:
- **Register Access**: Correct values read/written, protection enforced
- **Cryptographic Operations**: Output matches OpenSSL reference calculation
- **State Machine**: STATUS bits reflect correct FSM state
- **Errors**: Correct ERR_CODE value set, recovery mechanism works
- **Callbacks**: Expected side effects occur (FSM transition, register update)

### 3.4 Test Coverage Metrics

- **Register Coverage**: 100% of all registers accessed
- **Field Coverage**: All bitfields tested including reserved bits
- **Command Coverage**: All 4 sparse-encoded commands tested
- **Mode Coverage**: All cryptographic modes (SHA3-224/256/384/512, SHAKE128/256, cSHAKE128/256, KMAC128/256)
- **Error Coverage**: All 9 error codes triggered
- **State Coverage**: All FSM states (IDLE, ABSORB, SQUEEZE) and transitions
- **Configuration Coverage**: All CFG_SHADOWED field combinations
- **Key Length Coverage**: All supported key lengths (128/192/256/384/512 bits)

## 4. Test Infrastructure Requirements

### 4.1 Testbench Components

- SystemC TLM2.0 testbench with tlm_target_socket connection
- OpenSSL library integration for reference calculations
- Custom interface implementations (kmac_keymgr_if, kmac_app_if, kmac_entropy_if)
- Stimulus generators for register writes and application interface requests
- Response checkers for comparing actual vs expected results
- Coverage collectors for functional and code coverage

### 4.2 Reference Models

- OpenSSL EVP_sha3_* functions for SHA3 modes
- OpenSSL EVP_shake* functions for SHAKE/cSHAKE modes
- Custom KMAC implementation using OpenSSL primitives
- Padding calculators for SHA3/SHAKE/cSHAKE modes

### 4.3 Test Vectors

- NIST FIPS 202 SHA3 test vectors
- NIST FIPS 202 SHAKE test vectors
- NIST SP 800-185 cSHAKE test vectors
- NIST SP 800-185 KMAC test vectors
- Custom test vectors for corner cases

## 5. Notes

### 5.1 Important Notes

- **Interrupts**: Fully implemented via combined `intr_o` output port. Three sources: kmac_done (bit 0), fifo_empty (bit 1), kmac_err (bit 2). Signal asserts when (INTR_STATE & INTR_ENABLE) != 0.

### 5.2 Important Exclusions

Per the TLM guide and assumptions document, the following are NOT tested:

- **Cycle-Accurate Timing**: No cycle-level Keccak round timing validation.
- **Side-Channel Protections**: DOM multipliers, internal masking circuitry, SCA countermeasures not validated.
- **TL-UL Bus Protocol**: Replaced by TLM-2.0 target socket, no bus-level integrity checks.
- **Pin-Level Configurations**: Clock edges, reset signals, multi-bit encoding details not tested.
- **Context Switching**: Feature not supported in this KMAC version.

### 5.3 Test Plan Alignment

This test plan aligns with:
- **Features Document**: All "Is" features from kmac-features.md are covered
- **Register Map**: All registers from kmac-memory-map-registers.md are tested
- **Port Interfaces**: All ports from kmac-port-interfaces.md are used with exact names
- **Error Codes**: All 9 error codes from ERR_CODE register specification
- **State Machine**: All FSM states and transitions from detailed design

### 5.4 Configuration Parameters

Tests assume default configuration parameters:
- **EnMasking**: Tests cover both true (masked) and false (unmasked)
- **SwKeyMasked**: Tests cover both configurations when EnMasking=false
- **NumAppIntf**: Default 3 (KeyMgr, LC_CTRL, ROM_CTRL)
- **MsgFifoDepth**: Default 10 entries (64-bit)

## 6. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0 | 2025-12-26 | Claude Sonnet 4.5 | Initial comprehensive test plan for KMAC SystemC TLM model |
