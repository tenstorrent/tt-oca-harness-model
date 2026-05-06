# AES SystemC/TLM2.0 Model Test Plan

## Document Information

**IP Name**: AES (Advanced Encryption Standard)
**Document Version**: 1.0
**Generated Date**: 2026-02-18
**Target Audience**: Verification Engineers, Model Developers, Test Engineers
**Purpose**: Comprehensive unit test plan for SystemC/TLM2.0 AES model verification

---

## Test Implementation Status

> **NOTE**: All 99 test cases are implemented and active in the testbench. Tests are organized into port binding tests (inline in `testbench.cpp`) and functional test suites (separate source files).

### Test Organization

The test suite is organized into the following categories:

- **Port Binding and Interface Tests**: 6 tests (inline in `testbench.cpp`)
- **FUNC-AES-001**: Block Cipher Cryptographic Operations — OpenSSL Equivalence (30 tests, `aes_func001_test.cpp`)
- **FUNC-AES-002**: Sideload Key Management, Write Protection, Invalid Key Length (6 tests, `aes_func002_test.cpp`)
- **FUNC-AES-003**: Initialization Vector Management (10 tests, `aes_func003_test.cpp`)
- **FUNC-AES-004**: Automatic Operation Mode (9 tests, `aes_func004_test.cpp`)
- **FUNC-AES-005**: Manual Operation Mode (8 tests, `aes_func005_test.cpp`)
- **FUNC-AES-006**: Shadowed Register Fault Detection (11 tests, `aes_func006_test.cpp`)
- **FUNC-AES-007**: Register Interface Tests for Security Features (3 tests, `aes_func007_test.cpp`)
- **FUNC-AES-008**: Alert Generation and Error Reporting (12 tests, `aes_func008_test.cpp`)
- **FUNC-AES-009**: Functional Timing and Temporal Decoupling (4 tests, `aes_func009_test.cpp`)

**Total**: 99 test cases

---

## Test Strategy and Objectives

### Primary Objectives
1. Verify port bindings and basic connectivity for all 6 system interfaces
2. Validate all cryptographic operations via OpenSSL equivalence (AES-128/192/256 in ECB/CBC/CFB/OFB/CTR modes, both encryption and decryption)
3. Verify sideload key interface, write protection when non-idle, and invalid key length handling
4. Test IV management including auto-update behavior for CBC, CFB, OFB, CTR modes
5. Validate automatic and manual operation modes including back-pressure, OUTPUT_LOST, and precondition checks
6. Verify shadowed register two-write protocol, mismatch detection, and REGWEN locking
7. Validate security feature register interfaces (KEY_TOUCH_FORCES_RESEED, TRIGGER clearing)
8. Test alert generation (recoverable and fatal), life cycle escalation, and error state behavior
9. Verify functional timing: 12/14/16 cycles for AES-128/192/256 at 100MHz (TLM fixed unmasked timing)

### Test Coverage Requirements
- **Port Interfaces**: All 6 interfaces tested (TL-UL, clock, reset, keymgr sideload, alerts, idle, LC escalation)
- **Cryptographic Operations**: All 5 modes × 3 key lengths × 2 directions = 30 OpenSSL equivalence tests
- **Sideload Key**: AES-128 ECB, AES-256 CBC, and write-ignore behavior
- **IV Management**: CBC/CFB/OFB auto-update, CTR counter increment/overflow, ECB IV-ignore
- **Automatic Mode**: Auto-start, INPUT_READY, OUTPUT_VALID, back-pressure, pipelining
- **Manual Mode**: Explicit start, no back-pressure, OUTPUT_LOST, preconditions (data/key/IV)
- **Shadowed Registers**: CTRL_SHADOWED and CTRL_AUX_SHADOWED two-write protocol, read-reset, REGWEN
- **Alerts**: ALERT_TEST register, fatal/recoverable alert behavior, LC escalation, error state persistence
- **Timing**: AES-128/192/256 block latency and cross-mode timing consistency

### Exclusions (Per "Is Not" Scope)
- Gate-level timing and cycle-accurate delays (TLM abstraction)
- Physical implementation details (S-Box structures, gate counts)
- Side-channel analysis resistance measurements (functional modeling only)
- GCM/GHASH operations (delegated to software)
- DMA engine testing (not part of AES IP)
- Interrupt testing (AES does not generate interrupts)
- Compile-time parameters (SecMasking, SecSBoxImpl, AES192Enable — not present in TLM model)

---

## Test Cases

### Port Binding and Interface Tests

Implemented inline in `testbench.cpp` `run_tests()`. These run first before any functional test suite.

| Sl. No. | TestCase Name | Description | Ports/Signals Used | Test Type |
| ------- | ------------- | ----------- | ------------------ | --------- |
| 1 | Reset and Port Binding Verification | Trigger reset, verify AES enters IDLE state and no alerts are active after reset | rst_ni, idle_o, alert_recov_ctrl_update_err, alert_fatal_fault | Positive |
| 2 | Idle Status Interface Test | Read idle_signal after reset, verify it reflects AES idle state (HIGH) | idle_o | Positive |
| 3 | Clock Interface Test | Read clk_signal state, verify clock is connected and readable | clk_i | Positive |
| 4 | Key Manager Sideload Interface Test | Set test key via keymgr_impl, retrieve it back, verify key_share0[0] matches and valid flag is set | keymgr_key_port | Positive |
| 6 | Life Cycle Escalation Interface Test | Assert lc_escalate_en, verify alert_fatal_fault asserted and idle_o goes LOW (AES enters error state) | lc_escalate_en, alert_fatal_fault, idle_o | Negative |
| 7 | Reset Recovery from Error State | De-assert escalation, trigger reset, verify both alert signals cleared and idle_o returns HIGH | rst_ni, lc_escalate_en, alert_fatal_fault, alert_recov_ctrl_update_err, idle_o | Positive |

---

## Comprehensive Functional Test Cases

### Test Implementation Files
- `aes_func001_test.cpp`: Block Cipher Cryptographic Operations — OpenSSL Equivalence (30 tests)
- `aes_func002_test.cpp`: Sideload Key Management, Write Protection, Invalid Key Length (6 tests)
- `aes_func003_test.cpp`: Initialization Vector Management (10 tests)
- `aes_func004_test.cpp`: Automatic Operation Mode (9 tests)
- `aes_func005_test.cpp`: Manual Operation Mode (8 tests)
- `aes_func006_test.cpp`: Shadowed Register Fault Detection (11 tests)
- `aes_func007_test.cpp`: Register Interface Tests for Security Features (3 tests)
- `aes_func008_test.cpp`: Alert Generation and Error Reporting (12 tests)
- `aes_func009_test.cpp`: Functional Timing and Temporal Decoupling (4 tests)

### Detailed Test Listing

| Sl. No. | TestCase Name | Description | Registers Programmed | Ports/Signals Used | Test Type |
| ------- | ------------- | ----------- | -------------------- | ------------------ | --------- |
| **FUNC-AES-001: Block Cipher Cryptographic Operations — Encryption Equivalence** |
| 8 | test_openssl_aes128_ecb_equivalence | Generate random plaintext/key, encrypt with OpenSSL AES-128-ECB and AES model, verify outputs match | CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 9 | test_openssl_aes192_ecb_equivalence | Generate random plaintext/key, encrypt with OpenSSL AES-192-ECB and AES model, verify outputs match | CTRL_SHADOWED, KEY_SHARE0_0–5, KEY_SHARE1_0–5, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 10 | test_openssl_aes256_ecb_equivalence | Generate random plaintext/key, encrypt with OpenSSL AES-256-ECB and AES model, verify outputs match | CTRL_SHADOWED, KEY_SHARE0_0–7, KEY_SHARE1_0–7, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 11 | test_openssl_aes128_cbc_equivalence | Generate random plaintext/key/IV, encrypt with OpenSSL AES-128-CBC and AES model, verify outputs match | CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 12 | test_openssl_aes192_cbc_equivalence | Generate random plaintext/key/IV, encrypt with OpenSSL AES-192-CBC and AES model, verify outputs match | CTRL_SHADOWED, KEY_SHARE0_0–5, KEY_SHARE1_0–5, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 13 | test_openssl_aes256_cbc_equivalence | Generate random plaintext/key/IV, encrypt with OpenSSL AES-256-CBC and AES model, verify outputs match | CTRL_SHADOWED, KEY_SHARE0_0–7, KEY_SHARE1_0–7, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 14 | test_openssl_aes128_cfb_equivalence | Generate random plaintext/key/IV, encrypt with OpenSSL AES-128-CFB128 and AES model, verify outputs match | CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 15 | test_openssl_aes192_cfb_equivalence | Generate random plaintext/key/IV, encrypt with OpenSSL AES-192-CFB128 and AES model, verify outputs match | CTRL_SHADOWED, KEY_SHARE0_0–5, KEY_SHARE1_0–5, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 16 | test_openssl_aes256_cfb_equivalence | Generate random plaintext/key/IV, encrypt with OpenSSL AES-256-CFB128 and AES model, verify outputs match | CTRL_SHADOWED, KEY_SHARE0_0–7, KEY_SHARE1_0–7, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 17 | test_openssl_aes128_ofb_equivalence | Generate random plaintext/key/IV, encrypt with OpenSSL AES-128-OFB and AES model, verify outputs match | CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 18 | test_openssl_aes192_ofb_equivalence | Generate random plaintext/key/IV, encrypt with OpenSSL AES-192-OFB and AES model, verify outputs match | CTRL_SHADOWED, KEY_SHARE0_0–5, KEY_SHARE1_0–5, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 19 | test_openssl_aes256_ofb_equivalence | Generate random plaintext/key/IV, encrypt with OpenSSL AES-256-OFB and AES model, verify outputs match | CTRL_SHADOWED, KEY_SHARE0_0–7, KEY_SHARE1_0–7, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 20 | test_openssl_aes128_ctr_equivalence | Generate random plaintext/key/IV, encrypt with OpenSSL AES-128-CTR and AES model, verify outputs match | CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 21 | test_openssl_aes192_ctr_equivalence | Generate random plaintext/key/IV, encrypt with OpenSSL AES-192-CTR and AES model, verify outputs match | CTRL_SHADOWED, KEY_SHARE0_0–5, KEY_SHARE1_0–5, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 22 | test_openssl_aes256_ctr_equivalence | Generate random plaintext/key/IV, encrypt with OpenSSL AES-256-CTR and AES model, verify outputs match | CTRL_SHADOWED, KEY_SHARE0_0–7, KEY_SHARE1_0–7, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| **FUNC-AES-001: Block Cipher Cryptographic Operations — Decryption Equivalence** |
| 23 | test_openssl_aes128_ecb_decryption_equivalence | Encrypt with OpenSSL AES-128-ECB, decrypt with AES model, verify plaintext matches original | CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 24 | test_openssl_aes192_ecb_decryption_equivalence | Encrypt with OpenSSL AES-192-ECB, decrypt with AES model, verify plaintext matches original | CTRL_SHADOWED, KEY_SHARE0_0–5, KEY_SHARE1_0–5, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 25 | test_openssl_aes256_ecb_decryption_equivalence | Encrypt with OpenSSL AES-256-ECB, decrypt with AES model, verify plaintext matches original | CTRL_SHADOWED, KEY_SHARE0_0–7, KEY_SHARE1_0–7, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 26 | test_openssl_aes128_cbc_decryption_equivalence | Encrypt with OpenSSL AES-128-CBC, decrypt with AES model using same IV, verify plaintext matches | CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 27 | test_openssl_aes192_cbc_decryption_equivalence | Encrypt with OpenSSL AES-192-CBC, decrypt with AES model using same IV, verify plaintext matches | CTRL_SHADOWED, KEY_SHARE0_0–5, KEY_SHARE1_0–5, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 28 | test_openssl_aes256_cbc_decryption_equivalence | Encrypt with OpenSSL AES-256-CBC, decrypt with AES model using same IV, verify plaintext matches | CTRL_SHADOWED, KEY_SHARE0_0–7, KEY_SHARE1_0–7, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 29 | test_openssl_aes128_cfb_decryption_equivalence | Encrypt with OpenSSL AES-128-CFB128, decrypt with AES model, verify plaintext matches | CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 30 | test_openssl_aes192_cfb_decryption_equivalence | Encrypt with OpenSSL AES-192-CFB128, decrypt with AES model, verify plaintext matches | CTRL_SHADOWED, KEY_SHARE0_0–5, KEY_SHARE1_0–5, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 31 | test_openssl_aes256_cfb_decryption_equivalence | Encrypt with OpenSSL AES-256-CFB128, decrypt with AES model, verify plaintext matches | CTRL_SHADOWED, KEY_SHARE0_0–7, KEY_SHARE1_0–7, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 32 | test_openssl_aes128_ofb_decryption_equivalence | Encrypt with OpenSSL AES-128-OFB, decrypt with AES model, verify plaintext matches | CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 33 | test_openssl_aes192_ofb_decryption_equivalence | Encrypt with OpenSSL AES-192-OFB, decrypt with AES model, verify plaintext matches | CTRL_SHADOWED, KEY_SHARE0_0–5, KEY_SHARE1_0–5, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 34 | test_openssl_aes256_ofb_decryption_equivalence | Encrypt with OpenSSL AES-256-OFB, decrypt with AES model, verify plaintext matches | CTRL_SHADOWED, KEY_SHARE0_0–7, KEY_SHARE1_0–7, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 35 | test_openssl_aes128_ctr_decryption_equivalence | Encrypt with OpenSSL AES-128-CTR, decrypt with AES model (same operation), verify plaintext matches | CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 36 | test_openssl_aes192_ctr_decryption_equivalence | Encrypt with OpenSSL AES-192-CTR, decrypt with AES model, verify plaintext matches | CTRL_SHADOWED, KEY_SHARE0_0–5, KEY_SHARE1_0–5, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 37 | test_openssl_aes256_ctr_decryption_equivalence | Encrypt with OpenSSL AES-256-CTR, decrypt with AES model, verify plaintext matches | CTRL_SHADOWED, KEY_SHARE0_0–7, KEY_SHARE1_0–7, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| **FUNC-AES-002: Sideload Key Management, Write Protection, Invalid Key Length** |
| 38 | test_openssl_sideload_aes128_ecb_equivalence | Set AES-128 key via keymgr sideload interface (valid=true), configure SIDELOAD=1, encrypt, verify output matches OpenSSL with same key | CTRL_SHADOWED, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket, keymgr_key_port | Positive |
| 39 | test_openssl_sideload_aes256_cbc_equivalence | Set AES-256 key via keymgr sideload interface, configure SIDELOAD=1 CBC, write IV, encrypt, verify output matches OpenSSL | CTRL_SHADOWED, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket, keymgr_key_port | Positive |
| 40 | test_openssl_sideload_ignores_key_share_writes | Configure SIDELOAD=1 with sideload key K1, attempt to write different KEY_SHARE key K2, verify encryption uses K1 (KEY_SHARE writes ignored) | CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket, keymgr_key_port | Positive |
| 41 | test_ctrl_shadowed_write_protection_when_non_idle | Configure automatic mode, start operation, verify CTRL_SHADOWED write is rejected while STATUS.IDLE=0 (output valid but not yet read) | CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, STATUS | tlul_target_socket | Negative |
| 42 | test_iv_write_protection_when_non_idle | Configure CBC automatic mode, start operation, attempt to write IV while STATUS.IDLE=0, verify IV values unchanged (write rejected) | IV_0–3, CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, STATUS | tlul_target_socket | Negative |
| 43 | test_openssl_invalid_key_length_defaults_to_aes256 | Write CTRL_SHADOWED.KEY_LEN=0x3 (invalid encoding), write 256-bit key, encrypt, verify output matches OpenSSL AES-256 (invalid maps to AES-256) | CTRL_SHADOWED, KEY_SHARE0_0–7, KEY_SHARE1_0–7, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Negative |
| **FUNC-AES-003: Initialization Vector Management** |
| 44 | test_iv_register_read_write | Write known values {0x12345678, 0x9ABCDEF0, 0xFEDCBA98, 0x76543210} to IV_0–3, read back and verify all four registers match | IV_0–3 | tlul_target_socket | Positive |
| 45 | test_cbc_encryption_iv_auto_update | Configure AES-128 CBC, write initial IV, encrypt first block, read IV registers and verify they equal the first ciphertext block | IV_0–3, CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 46 | test_cbc_decryption_iv_auto_update | Configure AES-128 CBC decryption, write initial IV, decrypt block, verify IV auto-updates to the input ciphertext block | IV_0–3, CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 47 | test_cfb_mode_iv_auto_update | Configure AES-128 CFB, write initial IV, encrypt block, verify IV auto-updates to ciphertext output | IV_0–3, CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 48 | test_ofb_mode_iv_auto_update | Configure AES-128 OFB, write initial IV, encrypt block, verify IV auto-updates to cipher output (keystream block) | IV_0–3, CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 49 | test_ctr_mode_counter_increment | Configure AES-128 CTR with known counter value, process multiple blocks, verify IV registers reflect big-endian counter increment per block | IV_0–3, CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 50 | test_ctr_counter_overflow | Configure CTR mode with counter near 128-bit maximum, process blocks to cross wraparound boundary, verify correct counter overflow behavior | IV_0–3, CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 51 | test_ecb_mode_ignores_iv | Configure AES-128 ECB, do not write IV, write DATA_IN, verify auto-start occurs and operation completes (ECB does not require IV) | CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 52 | test_iv_required_for_non_ecb_auto_start | Configure AES-128 CBC automatic mode, write key and DATA_IN but not IV, verify auto-start does NOT occur (IV required) | IV_0–3, CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, STATUS | tlul_target_socket | Negative |
| 53 | test_multi_block_cbc_chaining | Configure AES-128 CBC, process 3 blocks sequentially, verify each block's ciphertext is correctly chained (IV updated to previous ciphertext) | IV_0–3, CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| **FUNC-AES-004: Automatic Operation Mode** |
| 54 | test_auto_start_on_data_in_complete | Configure AES-128 ECB automatic mode, verify STATUS.INPUT_READY=1 before write, write all 4 DATA_IN registers, verify operation auto-starts and OUTPUT_VALID=1 | CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 55 | test_input_ready_status_behavior | Configure automatic mode, monitor STATUS.INPUT_READY transitions: verify HIGH before write, LOW after writing DATA_IN_0, HIGH again after reading DATA_OUT | CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 56 | test_output_valid_status_behavior | Configure automatic mode, write all inputs, wait for operation, verify STATUS.OUTPUT_VALID=1 when complete and clears to 0 after reading all DATA_OUT | CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 57 | test_output_protection_prevents_overwrite | Configure automatic mode, complete operation without reading DATA_OUT, write new DATA_IN, verify STATUS.STALL=1 and new operation blocked | CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Negative |
| 58 | test_multi_block_pipelining | Configure automatic mode, process multiple blocks by reading DATA_OUT before writing next DATA_IN, verify continuous operation without stalls | CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 59 | test_back_to_back_processing | Configure automatic mode, process two blocks back-to-back (read output immediately before writing next input), verify no stall and correct outputs | CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 60 | test_auto_start_requires_all_conditions | Configure automatic mode, write key and IV but only 3 of 4 DATA_IN registers, verify auto-start does NOT occur (all 4 DATA_IN required) | CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, IV_0–3, DATA_IN_0–2, STATUS | tlul_target_socket | Negative |
| 61 | test_automatic_vs_manual_mode | Configure automatic mode, verify auto-start occurs; reconfigure to manual mode, verify same inputs do NOT auto-start without TRIGGER.START | CTRL_SHADOWED, TRIGGER, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, STATUS | tlul_target_socket | Positive |
| 62 | test_status_transitions_during_operation | Monitor STATUS register through full operation cycle: verify IDLE→not-IDLE on start, OUTPUT_VALID=1 on completion, IDLE again after read | CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| **FUNC-AES-005: Manual Operation Mode** |
| 63 | test_manual_mode_explicit_start | Configure AES-128 ECB manual mode (MANUAL_OPERATION=1), write key and all DATA_IN, verify OUTPUT_VALID=0 and IDLE=1 (no auto-start), then write TRIGGER.START=1 and verify operation completes | CTRL_SHADOWED, TRIGGER, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 64 | test_manual_mode_no_back_pressure | Configure AES-128 ECB manual mode, complete first operation without reading DATA_OUT, immediately trigger second operation, verify STATUS.STALL=0 (no back-pressure in manual mode) | CTRL_SHADOWED, TRIGGER, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 65 | test_output_lost_flag_on_overwrite | Configure manual mode, complete first operation without reading output, trigger second operation, verify STATUS.OUTPUT_LOST=1 (sticky) even after reading second output | CTRL_SHADOWED, TRIGGER, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Negative |
| 66 | test_output_lost_cleared_by_ctrl_write | Create OUTPUT_LOST condition in manual mode, read output, write CTRL_SHADOWED (new message signal), verify STATUS.OUTPUT_LOST clears to 0 | CTRL_SHADOWED, TRIGGER, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Positive |
| 67 | test_manual_mode_precondition_data_in | Configure AES-128 ECB manual mode, write key and only 3 of 4 DATA_IN registers, write TRIGGER.START=1, verify operation does NOT start (IDLE=1, OUTPUT_VALID=0); complete DATA_IN and verify start succeeds | CTRL_SHADOWED, TRIGGER, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, STATUS | tlul_target_socket | Negative |
| 68 | test_manual_mode_precondition_key | Configure AES-128 ECB manual mode WITHOUT writing key, write DATA_IN, write TRIGGER.START=1, verify operation does NOT start; write key and verify start succeeds | CTRL_SHADOWED, TRIGGER, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, STATUS | tlul_target_socket | Negative |
| 69 | test_manual_mode_precondition_iv | Configure AES-128 CBC manual mode, write key and DATA_IN but NOT IV, write TRIGGER.START=1, verify operation does NOT start; write IV and verify start succeeds | CTRL_SHADOWED, TRIGGER, IV_0–3, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, STATUS | tlul_target_socket | Negative |
| 70 | test_manual_mode_multi_block_output_overwrite | Configure AES-128 CBC manual mode, process 3 blocks consecutively without reading first two outputs, verify STATUS.OUTPUT_LOST=1 and STATUS.STALL=0 | CTRL_SHADOWED, TRIGGER, IV_0–3, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Negative |
| **FUNC-AES-006: Shadowed Register Fault Detection** |
| 71 | test_ctrl_shadowed_two_write_matching | Write CTRL_SHADOWED twice with identical value (AES-128 ECB ENC), verify register updated to new value and STATUS.ALERT_RECOV_CTRL_UPDATE_ERR=0, alert_recov signal LOW | CTRL_SHADOWED, STATUS | tlul_target_socket, alert_recov_ctrl_update_err | Positive |
| 72 | test_ctrl_shadowed_two_write_mismatch | Write CTRL_SHADOWED first with AES-128-ECB then AES-256-CBC (mismatch), verify STATUS.ALERT_RECOV_CTRL_UPDATE_ERR=1, alert_recov signal HIGH, register NOT updated | CTRL_SHADOWED, STATUS | tlul_target_socket, alert_recov_ctrl_update_err | Negative |
| 73 | test_ctrl_aux_shadowed_two_write_matching | Write CTRL_AUX_SHADOWED twice with KEY_TOUCH_FORCES_RESEED=1, verify register updated and no alert triggered | CTRL_AUX_SHADOWED, CTRL_AUX_REGWEN, STATUS | tlul_target_socket, alert_recov_ctrl_update_err | Positive |
| 74 | test_ctrl_aux_shadowed_two_write_mismatch | Write CTRL_AUX_SHADOWED first with KEY_TOUCH_FORCES_RESEED=1 then =0 (mismatch), verify STATUS.ALERT_RECOV_CTRL_UPDATE_ERR=1, alert_recov signal HIGH, register NOT updated | CTRL_AUX_SHADOWED, CTRL_AUX_REGWEN, STATUS | tlul_target_socket, alert_recov_ctrl_update_err | Negative |
| 75 | test_shadowed_read_resets_sequence | Write CTRL_SHADOWED once, read it (resets write sequence), write different value, verify no mismatch alert triggered (read treated as new first write) | CTRL_SHADOWED, STATUS | tlul_target_socket, alert_recov_ctrl_update_err | Positive |
| 76 | test_alert_cleared_by_successful_write | Trigger mismatch alert on CTRL_SHADOWED, verify alert set and signal asserted; perform successful matching write, verify STATUS.ALERT_RECOV_CTRL_UPDATE_ERR=0 and signal deasserted | CTRL_SHADOWED, STATUS | tlul_target_socket, alert_recov_ctrl_update_err | Positive |
| 77 | test_multiple_consecutive_mismatches | Perform 3 consecutive CTRL_SHADOWED mismatches without clearing, verify alert remains set (sticky) and alert_recov signal stays asserted throughout | CTRL_SHADOWED, STATUS | tlul_target_socket, alert_recov_ctrl_update_err | Negative |
| 78 | test_shadowed_write_rejected_when_busy | Configure automatic mode, start operation (IDLE=0), attempt CTRL_SHADOWED two-write protocol, wait for completion, verify CTRL_SHADOWED value unchanged (busy write rejected) | CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket | Negative |
| 79 | test_ctrl_aux_write_protection_when_locked | Write CTRL_AUX_REGWEN=0 to lock, verify readback=0, attempt CTRL_AUX_SHADOWED two-write, verify CTRL_AUX_SHADOWED value unchanged (locked) | CTRL_AUX_REGWEN, CTRL_AUX_SHADOWED | tlul_target_socket | Positive |
| 80 | test_ctrl_aux_regwen_cannot_unlock | Write CTRL_AUX_REGWEN=0 to lock, attempt to write CTRL_AUX_REGWEN=1 to unlock, verify CTRL_AUX_REGWEN remains 0 (write-once-to-clear, cannot re-lock) | CTRL_AUX_REGWEN | tlul_target_socket | Positive |
| 81 | test_ctrl_aux_shadowed_read_resets_sequence | After reset (to unlock REGWEN), write CTRL_AUX_SHADOWED once, read it (resets sequence), write different value twice (matching), verify register updated to new value without alert | CTRL_AUX_SHADOWED, CTRL_AUX_REGWEN | tlul_target_socket, rst_ni | Positive |
| **FUNC-AES-007: Register Interface Tests for Security Features** |
| 82 | test_ctrl_aux_key_touch_forces_reseed_register | Write CTRL_AUX_SHADOWED with KEY_TOUCH_FORCES_RESEED=1, read back and verify bit=1; write =0, read back and verify bit=0 | CTRL_AUX_SHADOWED | tlul_target_socket | Positive |
| 83 | test_trigger_key_iv_data_in_clear | Write known values to KEY_SHARE, IV, DATA_IN registers; write TRIGGER.KEY_IV_DATA_IN_CLEAR=1 (bit 1); wait for idle; verify IV and DATA_IN registers changed from original values | TRIGGER, KEY_SHARE0_0–3, KEY_SHARE1_0–3, IV_0–3, DATA_IN_0–3, STATUS | tlul_target_socket | Positive |
| 84 | test_trigger_data_out_clear | Perform encryption to populate DATA_OUT, read DATA_OUT values; write TRIGGER.DATA_OUT_CLEAR=1 (bit 2); wait for idle; verify DATA_OUT registers changed from ciphertext values | TRIGGER, DATA_OUT_0–3, CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, STATUS | tlul_target_socket | Positive |
| **FUNC-AES-008: Alert Generation and Error Reporting** |
| 85 | test_alert_test_register_recoverable_trigger | Write ALERT_TEST bit 0=1 (recov_ctrl_update_err), verify alert_recov_ctrl_update_err signal asserted and STATUS.ALERT_RECOV_CTRL_UPDATE_ERR=1 | ALERT_TEST, STATUS | tlul_target_socket, alert_recov_ctrl_update_err | Positive |
| 86 | test_alert_test_register_fatal_trigger | Write ALERT_TEST bit 1=1 (fatal_fault), verify alert_fatal_fault signal asserted, STATUS.ALERT_FATAL_FAULT=1, STATUS.IDLE=0, and idle_o=false (AES enters ERROR state) | ALERT_TEST, STATUS | tlul_target_socket, alert_fatal_fault, idle_o | Negative |
| 87 | test_fatal_alert_terminal_error_state | Enter ERROR state via ALERT_TEST, verify alert_fatal_fault asserted; attempt to write CTRL_SHADOWED, KEY_SHARE0, DATA_IN — verify all writes ignored (register values unchanged) | ALERT_TEST, CTRL_SHADOWED, KEY_SHARE0_0, DATA_IN_0, STATUS | tlul_target_socket, alert_fatal_fault | Negative |
| 88 | test_fatal_alert_recovery_requires_reset | Enter ERROR state, verify register writes ignored; assert reset, verify AES recovers to IDLE, alerts cleared, and registers writable again | ALERT_TEST, CTRL_SHADOWED, STATUS | rst_ni, tlul_target_socket, alert_fatal_fault | Positive |
| 89 | test_life_cycle_escalation_fatal_alert | Assert lc_escalate_en during normal operation, verify AES immediately asserts alert_fatal_fault, STATUS.ALERT_FATAL_FAULT=1, and STATUS.IDLE=0 | STATUS | lc_escalate_en, alert_fatal_fault, tlul_target_socket | Negative |
| 90 | test_life_cycle_escalation_register_writes_ignored | Assert lc_escalate_en to trigger lockup, attempt to write CTRL_SHADOWED, KEY_SHARE0_0, DATA_IN_0, verify all writes ignored | CTRL_SHADOWED, KEY_SHARE0_0, DATA_IN_0, STATUS | lc_escalate_en, tlul_target_socket | Negative |
| 91 | test_register_clearing_on_fatal_alert | Trigger fatal alert, verify DATA_OUT and other sensitive registers are cleared (changed from pre-alert values) | ALERT_TEST, DATA_OUT_0–3, STATUS | tlul_target_socket, alert_fatal_fault | Negative |
| 92 | test_status_register_readable_in_error_state | Enter ERROR state via fatal alert, verify STATUS register is still readable and correctly reflects ALERT_FATAL_FAULT=1, IDLE=0 | ALERT_TEST, STATUS | tlul_target_socket, alert_fatal_fault | Positive |
| 93 | test_multiple_fatal_alert_triggers | Trigger fatal alert, reset to recover, trigger fatal alert again, verify behavior is consistent across multiple fatal alert cycles | ALERT_TEST, STATUS | rst_ni, tlul_target_socket, alert_fatal_fault | Negative |
| 94 | test_persistent_error_state | Trigger fatal alert, verify AES remains in ERROR state (IDLE=0, alert asserted) without reset — state persists indefinitely | ALERT_TEST, STATUS | tlul_target_socket, alert_fatal_fault, idle_o | Negative |
| 95 | test_recoverable_alert_no_error_state | Trigger recoverable alert via CTRL_SHADOWED mismatch, verify STATUS.ALERT_RECOV_CTRL_UPDATE_ERR=1 but STATUS.IDLE=1 (AES remains operational, no ERROR state) | CTRL_SHADOWED, STATUS | tlul_target_socket, alert_recov_ctrl_update_err | Positive |
| 96 | test_alert_outputs_match_status_register | Verify alert_recov_ctrl_update_err port matches STATUS.ALERT_RECOV_CTRL_UPDATE_ERR bit, and alert_fatal_fault port matches STATUS.ALERT_FATAL_FAULT bit at all times | STATUS | tlul_target_socket, alert_recov_ctrl_update_err, alert_fatal_fault | Positive |
| **FUNC-AES-009: Functional Timing and Temporal Decoupling** |
| 97 | test_func009_aes128_block_latency | Configure AES-128 ECB encryption at 100MHz, measure time from DATA_IN write to OUTPUT_VALID, verify elapsed time is ~120ns (12 cycles ± 50ns tolerance) | CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket, clk_i | Positive |
| 98 | test_func009_aes192_block_latency | Configure AES-192 ECB encryption at 100MHz, measure block processing time, verify elapsed time is ~140ns (14 cycles ± 50ns tolerance) | CTRL_SHADOWED, KEY_SHARE0_0–5, KEY_SHARE1_0–5, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket, clk_i | Positive |
| 99 | test_func009_aes256_block_latency | Configure AES-256 ECB encryption at 100MHz, measure block processing time, verify elapsed time is ~160ns (16 cycles ± 50ns tolerance) | CTRL_SHADOWED, KEY_SHARE0_0–7, KEY_SHARE1_0–7, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket, clk_i | Positive |
| 100 | test_func009_timing_across_modes | Configure AES-128 in each of 5 modes (ECB/CBC/CFB/OFB/CTR), measure block latency per mode, verify all modes produce ~120ns latency (same key length = same cipher rounds) | CTRL_SHADOWED, KEY_SHARE0_0–3, KEY_SHARE1_0–3, IV_0–3, DATA_IN_0–3, DATA_OUT_0–3, STATUS | tlul_target_socket, clk_i | Positive |

---

## Test Execution Guidance

### Test Environment Setup
1. **SystemC TLM Platform**: AES model integrated with TLM-2.0 bus infrastructure
2. **Port Bindings**: All 6 system interfaces properly connected (TL-UL, clock, reset, keymgr, alerts, idle, LC escalation)
3. **OpenSSL Library**: Version 1.1.1 or 3.0 for cryptographic reference operations (EVP_EncryptUpdate, EVP_DecryptUpdate)
4. **Clock Modeling**: Fixed 100MHz default (configurable via CCI parameter `m_clk_freq_hz`)
5. **Key Manager Stub**: `keymgr_impl` in `aes_test` provides mock sideload key interface
7. **Alert Monitor**: `alert_recov_ctrl_update_err_i` and `alert_fatal_fault_i` signals monitored by test module
8. **Temporal Decoupling**: Global quantum set to 100ns in `run_tests()`

### Running Tests

```bash
cd /home/abhishekc/sep/tenstorrent_sep/models/ot/aes
make run_test
```

Tests run in the following order as defined in `testbench.cpp::run_tests()`:
1. Port Binding Tests (Tests 1–6, inline)
2. FUNC-AES-001 (Tests 8–37)
3. FUNC-AES-002 (Tests 38–43)
4. FUNC-AES-003 (Tests 44–53)
5. FUNC-AES-004 (Tests 54–62)
6. FUNC-AES-005 (Tests 63–70)
7. FUNC-AES-006 partial (Tests 71–78, excluding REGWEN locking)
8. FUNC-AES-007 (Tests 82–84)
9. FUNC-AES-008 (Tests 85–96)
10. FUNC-AES-009 (Tests 97–100)
11. FUNC-AES-006 REGWEN locking (Tests 79–81, run last — permanently locks CTRL_AUX_REGWEN)

> **Note**: FUNC-AES-006 REGWEN locking tests (79–81) run last because writing `CTRL_AUX_REGWEN=0` permanently locks the register until reset. A reset is performed before these tests to ensure REGWEN is unlocked.

### Test Execution Priority

#### Priority 1 (Must Pass)
- Tests 1–6 (port binding and interface connectivity)
- Tests 8–37 (OpenSSL cryptographic equivalence — all modes and key lengths)
- Tests 85–96 (alert generation and error reporting)

#### Priority 2 (High Importance)
- Tests 38–43 (sideload key, write protection, invalid key length)
- Tests 44–53 (IV management)
- Tests 54–62 (automatic mode)
- Tests 63–70 (manual mode)
- Tests 71–81 (shadowed register fault detection)

#### Priority 3 (Coverage)
- Tests 82–84 (security feature register interfaces)
- Tests 97–100 (functional timing)

### Success Criteria
- **100% pass rate** for Priority 1 tests
- **95% pass rate** for Priority 2 tests
- **90% pass rate** for Priority 3 tests
- **Zero functional deviations** from OpenSSL reference implementation
- **All alert sources** verified with at least one test case each

### Coverage Metrics
- **Cryptographic Coverage**: 5 modes × 3 key lengths × 2 directions = 30 combinations (100%)
- **Interface Coverage**: All 6 port interfaces exercised
- **Register Coverage**: CTRL_SHADOWED, CTRL_AUX_SHADOWED, CTRL_AUX_REGWEN, TRIGGER, STATUS, ALERT_TEST, KEY_SHARE0/1, IV, DATA_IN, DATA_OUT
- **Alert Coverage**: Recoverable (shadowed mismatch) and fatal (LC escalation, ALERT_TEST) — both types tested
- **Mode Coverage**: Automatic and manual operation modes both fully tested
- **Timing Coverage**: AES-128/192/256 block latency and cross-mode consistency

---

## Notes

1. **TLM Model Timing Characteristics**:
   - Fixed unmasked timing: 12 cycles (AES-128), 14 cycles (AES-192), 16 cycles (AES-256)
   - At default 100MHz: 120ns / 140ns / 160ns respectively
   - Timing tests use ±50ns tolerance to accommodate TLM temporal decoupling
   - No compile-time masking parameters (SecMasking, SecSBoxImpl, AES192Enable) — not present in TLM model

2. **Two-Share Key Mechanism**:
   - All tests use `generate_two_share_key()` to split actual key into KEY_SHARE0 XOR KEY_SHARE1
   - AES model internally computes: actual_key = KEY_SHARE0 XOR KEY_SHARE1
   - OpenSSL is called with the actual key for reference comparison

3. **Pseudo-Random Data Verification**:
   - Tests verify registers are cleared with non-zero data after TRIGGER clearing operations
   - Actual pseudo-random values are not predictable (implementation-dependent)
   - Tests check that cleared registers differ from original values

4. **CTRL_AUX_REGWEN Ordering**:
   - CTRL_AUX_REGWEN is a write-once-to-clear (RW0C) register
   - Tests 79–81 permanently lock CTRL_AUX_REGWEN and must run last
   - A reset before tests 79–81 restores REGWEN to unlocked state

5. **No Interrupt/DMA Testing**:
   - AES IP does not generate interrupts (polling-based via STATUS register)
   - AES IP does not include DMA engine (register-based data transfer only)

6. **Excluded from Testing** (per "Is Not" scope):
   - Gate-level timing, cycle-accurate simulation
   - Physical implementation details (S-Box structures, gate counts)
   - Side-channel analysis resistance measurements
   - GCM/GHASH operations (not implemented in AES IP)
   - Compile-time parameters (SecMasking, SecSBoxImpl, AES192Enable)

---

**End of Test Plan Document**
