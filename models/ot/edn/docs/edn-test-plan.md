# EDN (Entropy Distribution Network) Test Plan

## Overview

This test plan defines comprehensive test cases for the SystemC/TLM2.0 verification of the EDN (Entropy Distribution Network) IP model. The EDN serves as an entropy distribution bridge between CSRNG and hardware peripherals, supporting three operating modes: boot-time request mode, auto request mode, and software port mode.

## Test Coverage Scope

### Functional Areas Covered
- Register access patterns (read, write, reset values, reserved fields)
- All three operating modes (boot-time, auto request, software port)
- State machine transitions and sequences
- CSRNG interface and command handling (instantiate, generate, reseed, uninstantiate)
- Endpoint interfaces and handshake protocol (8 endpoints)
- Interrupt generation and clearing (edn_cmd_req_done, edn_fatal_err)
- Alert generation (recoverable and fatal)
- Error conditions and corner cases
- FIFO overflow/underflow scenarios
- Multi-bit encoding validation
- Register write protection (REGWEN)
- Command sequencing (NIST SP 800-90A compliance)
- Reset behavior
- Data width conversion (128-bit to 32-bit)
- FIPS compliance indicator propagation

---

# Test Plan

| Sl. No. | TestCase Name | Description | Registers Programmed | Ports/Signals Used | Test Type |
| ------- | ------------- | ----------- | -------------------- | ------------------ | --------- |
| 1 | test_register_reset_values | Verify all registers return correct reset values after system reset. | INTR_STATE, INTR_ENABLE, INTR_TEST, ALERT_TEST, REGWEN, CTRL, BOOT_INS_CMD, BOOT_GEN_CMD, SW_CMD_STS, HW_CMD_STS, MAX_NUM_REQS_BETWEEN_RESEEDS, RECOV_ALERT_STS, ERR_CODE, MAIN_SM_STATE | rst_ni | Positive |
| 2 | test_intr_state_rw1c | Verify INTR_STATE register Write-1-to-Clear mechanism for edn_cmd_req_done and edn_fatal_err bits. | INTR_STATE | intr_edn_cmd_req_done, intr_edn_fatal_err | Positive |
| 3 | test_intr_enable_rw | Verify INTR_ENABLE register read/write functionality for enabling edn_cmd_req_done and edn_fatal_err interrupts. | INTR_ENABLE | intr_edn_cmd_req_done, intr_edn_fatal_err | Positive |
| 4 | test_intr_test_wo | Verify INTR_TEST register forces interrupt status bits when written with 1 to edn_cmd_req_done and edn_fatal_err fields. | INTR_TEST, INTR_STATE, INTR_ENABLE | intr_edn_cmd_req_done, intr_edn_fatal_err | Positive |
| 5 | test_alert_test_wo | Verify ALERT_TEST register triggers alert signals when written with 1 to recov_alert and fatal_alert fields. | ALERT_TEST | alert_recov_alert, alert_fatal_alert | Positive |
| 6 | test_regwen_write_protection | Verify REGWEN register Write-0-to-Clear mechanism permanently locks CTRL register until reset. | REGWEN, CTRL | tlm_reg_target_socket | Positive |
| 7 | test_regwen_lock_enforcement | Verify writes to CTRL register are blocked when REGWEN bit is cleared to 0. | REGWEN, CTRL | tlm_reg_target_socket | Negative |
| 8 | test_ctrl_edn_enable_valid | Verify CTRL.EDN_ENABLE field accepts valid multi-bit encoded values 0x6 (enable) and 0x9 (disable). | CTRL | tlm_reg_target_socket | Positive |
| 9 | test_ctrl_edn_enable_invalid | Verify CTRL.EDN_ENABLE field with invalid multi-bit encoded values triggers EDN_ENABLE_FIELD_ALERT in RECOV_ALERT_STS and asserts recoverable alert. | CTRL, RECOV_ALERT_STS | alert_recov_alert | Negative |
| 10 | test_ctrl_boot_req_mode_valid | Verify CTRL.BOOT_REQ_MODE field accepts valid multi-bit encoded values 0x6 (enable) and 0x9 (disable). | CTRL | tlm_reg_target_socket | Positive |
| 11 | test_ctrl_boot_req_mode_invalid | Verify CTRL.BOOT_REQ_MODE field with invalid values triggers BOOT_REQ_MODE_FIELD_ALERT in RECOV_ALERT_STS. | CTRL, RECOV_ALERT_STS | alert_recov_alert | Negative |
| 12 | test_ctrl_auto_req_mode_valid | Verify CTRL.AUTO_REQ_MODE field accepts valid multi-bit encoded values 0x6 (enable) and 0x9 (disable). | CTRL | tlm_reg_target_socket | Positive |
| 13 | test_ctrl_auto_req_mode_invalid | Verify CTRL.AUTO_REQ_MODE field with invalid values triggers AUTO_REQ_MODE_FIELD_ALERT in RECOV_ALERT_STS. | CTRL, RECOV_ALERT_STS | alert_recov_alert | Negative |
| 14 | test_ctrl_cmd_fifo_rst_valid | Verify CTRL.CMD_FIFO_RST field clears RESEED_CMD and GENERATE_CMD FIFOs when set to 0x6. | CTRL, RESEED_CMD, GENERATE_CMD | tlm_reg_target_socket | Positive |
| 15 | test_ctrl_cmd_fifo_rst_invalid | Verify CTRL.CMD_FIFO_RST field with invalid values triggers CMD_FIFO_RST_FIELD_ALERT in RECOV_ALERT_STS. | CTRL, RECOV_ALERT_STS | alert_recov_alert | Negative |
| 16 | test_boot_ins_cmd_rw | Verify BOOT_INS_CMD register read/write functionality stores full 32-bit instantiate command word. | BOOT_INS_CMD | tlm_reg_target_socket | Positive |
| 17 | test_boot_gen_cmd_rw | Verify BOOT_GEN_CMD register read/write functionality stores full 32-bit generate command word. | BOOT_GEN_CMD | tlm_reg_target_socket | Positive |
| 18 | test_sw_cmd_req_wo | Verify SW_CMD_REQ register accepts command words and forwards to CSRNG interface in software port mode. | SW_CMD_REQ, SW_CMD_STS, CTRL | csrng_cmd_initiator_socket, tlm_reg_target_socket | Positive |
| 19 | test_sw_cmd_sts_cmd_reg_rdy | Verify SW_CMD_STS.CMD_REG_RDY bit indicates readiness to accept next command word before SW_CMD_REQ writes. | SW_CMD_STS, SW_CMD_REQ | tlm_reg_target_socket | Positive |
| 20 | test_sw_cmd_sts_cmd_rdy | Verify SW_CMD_STS.CMD_RDY bit indicates EDN readiness for new multi-word command sequence. | SW_CMD_STS | tlm_reg_target_socket | Positive |
| 21 | test_sw_cmd_sts_cmd_ack | Verify SW_CMD_STS.CMD_ACK bit is set when CSRNG acknowledges software command completion. | SW_CMD_STS, SW_CMD_REQ, INTR_STATE | csrng_cmd_initiator_socket, intr_edn_cmd_req_done | Positive |
| 22 | test_sw_cmd_sts_cmd_status | Verify SW_CMD_STS.CMD_STS field reflects CSRNG command status code on acknowledgment. | SW_CMD_STS, SW_CMD_REQ | csrng_cmd_initiator_socket | Positive |
| 23 | test_hw_cmd_sts_boot_mode | Verify HW_CMD_STS.BOOT_MODE bit reflects boot-time request mode active status. | HW_CMD_STS, CTRL | tlm_reg_target_socket | Positive |
| 24 | test_hw_cmd_sts_auto_mode | Verify HW_CMD_STS.AUTO_MODE bit reflects auto request mode active status. | HW_CMD_STS, CTRL | tlm_reg_target_socket | Positive |
| 25 | test_hw_cmd_sts_cmd_type | Verify HW_CMD_STS.CMD_TYPE field reflects last hardware-issued command type encoding. | HW_CMD_STS, CTRL | csrng_cmd_initiator_socket | Positive |
| 26 | test_hw_cmd_sts_cmd_ack_status | Verify HW_CMD_STS.CMD_ACK and CMD_STS fields reflect CSRNG acknowledgment for hardware commands. | HW_CMD_STS, CTRL | csrng_cmd_initiator_socket | Positive |
| 27 | test_reseed_cmd_fifo_wo | Verify RESEED_CMD register accepts up to 13 32-bit command words for auto request mode reseed FIFO. | RESEED_CMD, CTRL | tlm_reg_target_socket | Positive |
| 28 | test_generate_cmd_fifo_wo | Verify GENERATE_CMD register accepts up to 13 32-bit command words for auto request mode generate FIFO. | GENERATE_CMD, CTRL | tlm_reg_target_socket | Positive |
| 29 | test_max_num_reqs_between_reseeds_rw | Verify MAX_NUM_REQS_BETWEEN_RESEEDS register configures auto mode reseed interval counter. | MAX_NUM_REQS_BETWEEN_RESEEDS | tlm_reg_target_socket | Positive |
| 30 | test_recov_alert_sts_rw0c | Verify RECOV_ALERT_STS register Write-0-to-Clear mechanism for all alert status bits. | RECOV_ALERT_STS | alert_recov_alert | Positive |
| 31 | test_err_code_sticky_ro | Verify ERR_CODE register is read-only and sticky until system reset for all fatal error bits. | ERR_CODE | alert_fatal_alert | Positive |
| 32 | test_err_code_test_injection | Verify ERR_CODE_TEST register forces ERR_CODE bits for testing error handling paths. | ERR_CODE_TEST, ERR_CODE, INTR_STATE | alert_fatal_alert, intr_edn_fatal_err | Positive |
| 33 | test_main_sm_state_visibility | Verify MAIN_SM_STATE register exposes current state machine state for debug visibility. | MAIN_SM_STATE, CTRL | tlm_reg_target_socket | Positive |
| 34 | test_reserved_bits_read_zero | Verify reserved bits in all registers read as 0 and writes are ignored. | INTR_STATE, INTR_ENABLE, INTR_TEST, ALERT_TEST, REGWEN, CTRL, SW_CMD_STS, HW_CMD_STS, RECOV_ALERT_STS, ERR_CODE, MAIN_SM_STATE | tlm_reg_target_socket | Positive |
| 35 | test_boot_mode_enable_sequence | Verify boot-time request mode activation by setting EDN_ENABLE=0x6 and BOOT_REQ_MODE=0x6 in CTRL register. | CTRL, BOOT_INS_CMD, BOOT_GEN_CMD, HW_CMD_STS, MAIN_SM_STATE | csrng_cmd_initiator_socket, csrng_genbits_target_socket | Positive |
| 36 | test_boot_mode_instantiate_command | Verify hardware automatically issues instantiate command using BOOT_INS_CMD configuration in boot-time mode. | CTRL, BOOT_INS_CMD, HW_CMD_STS | csrng_cmd_initiator_socket | Positive |
| 37 | test_boot_mode_generate_command | Verify hardware automatically issues generate command using BOOT_GEN_CMD configuration after instantiate completes in boot-time mode. | CTRL, BOOT_GEN_CMD, HW_CMD_STS | csrng_cmd_initiator_socket, csrng_genbits_target_socket | Positive |
| 38 | test_boot_mode_entropy_distribution | Verify boot-time mode distributes entropy to endpoint interfaces when peripherals assert edn_req signals. | CTRL, BOOT_INS_CMD, BOOT_GEN_CMD | edn_req[0:7], edn_ack[0:7], edn_bus[0:7], edn_fips[0:7], csrng_genbits_target_socket | Positive |
| 39 | test_boot_mode_pre_fips_indicator | Verify edn_fips signals are de-asserted (pre-FIPS entropy) during boot-time request mode operation. | CTRL | edn_fips[0:7], csrng_genbits_target_socket | Positive |
| 40 | test_boot_mode_exit_sequence | Verify clearing BOOT_REQ_MODE field transitions state machine to SWPortMode and issues automatic uninstantiate command. | CTRL, MAIN_SM_STATE, HW_CMD_STS | csrng_cmd_initiator_socket | Positive |
| 41 | test_boot_mode_state_transitions | Verify state machine transitions through Idle, BootInsAckWait, BootGenAckWait states during boot-time mode. | CTRL, MAIN_SM_STATE | csrng_cmd_initiator_socket, csrng_genbits_target_socket | Positive |
| 42 | test_auto_mode_prerequisite_config | Verify auto request mode requires GENERATE_CMD, RESEED_CMD FIFOs and MAX_NUM_REQS_BETWEEN_RESEEDS configured before enabling. | GENERATE_CMD, RESEED_CMD, MAX_NUM_REQS_BETWEEN_RESEEDS, CTRL | tlm_reg_target_socket | Positive |
| 43 | test_auto_mode_enable_sequence | Verify auto request mode activation by setting EDN_ENABLE=0x6 and AUTO_REQ_MODE=0x6 after prerequisite configuration. | CTRL, HW_CMD_STS, MAIN_SM_STATE | tlm_reg_target_socket | Positive |
| 44 | test_auto_mode_manual_instantiate | Verify firmware must manually issue instantiate command via SW_CMD_REQ after enabling auto request mode. | CTRL, SW_CMD_REQ, SW_CMD_STS, HW_CMD_STS | csrng_cmd_initiator_socket, intr_edn_cmd_req_done | Positive |
| 45 | test_auto_mode_generate_command | Verify hardware automatically issues generate command from GENERATE_CMD FIFO when endpoints request entropy. | CTRL, GENERATE_CMD, HW_CMD_STS | edn_req[0:7], csrng_cmd_initiator_socket, csrng_genbits_target_socket | Positive |
| 46 | test_auto_mode_reseed_interval | Verify hardware automatically issues reseed command from RESEED_CMD FIFO after MAX_NUM_REQS_BETWEEN_RESEEDS generate commands. | CTRL, RESEED_CMD, GENERATE_CMD, MAX_NUM_REQS_BETWEEN_RESEEDS, HW_CMD_STS | csrng_cmd_initiator_socket | Positive |
| 47 | test_auto_mode_entropy_distribution | Verify auto request mode distributes entropy to endpoint interfaces with FIPS indicator propagation. | CTRL, GENERATE_CMD | edn_req[0:7], edn_ack[0:7], edn_bus[0:7], edn_fips[0:7], csrng_genbits_target_socket | Positive |
| 48 | test_auto_mode_exit_sequence | Verify clearing AUTO_REQ_MODE field waits for current command completion and transitions to SWPortMode. | CTRL, MAIN_SM_STATE, HW_CMD_STS | csrng_cmd_initiator_socket | Positive |
| 49 | test_auto_mode_state_transitions | Verify state machine transitions through AutoLoadIns, AutoFirstAckWait, AutoDispatch, AutoGenAckWait, AutoReseedAckWait states. | CTRL, MAIN_SM_STATE | csrng_cmd_initiator_socket, csrng_genbits_target_socket | Positive |
| 50 | test_sw_port_mode_enable | Verify software port mode activation by setting EDN_ENABLE=0x6 without BOOT_REQ_MODE or AUTO_REQ_MODE. | CTRL, MAIN_SM_STATE | tlm_reg_target_socket | Positive |
| 51 | test_sw_port_instantiate_command | Verify firmware issues instantiate command via SW_CMD_REQ in software port mode with full parameter control. | CTRL, SW_CMD_REQ, SW_CMD_STS, INTR_STATE | csrng_cmd_initiator_socket, intr_edn_cmd_req_done | Positive |
| 52 | test_sw_port_generate_command | Verify firmware issues generate command via SW_CMD_REQ with configurable glen parameter in software port mode. | CTRL, SW_CMD_REQ, SW_CMD_STS, INTR_STATE | csrng_cmd_initiator_socket, csrng_genbits_target_socket, intr_edn_cmd_req_done | Positive |
| 53 | test_sw_port_reseed_command | Verify firmware issues reseed command via SW_CMD_REQ with additional data in software port mode. | CTRL, SW_CMD_REQ, SW_CMD_STS, INTR_STATE | csrng_cmd_initiator_socket, intr_edn_cmd_req_done | Positive |
| 54 | test_sw_port_uninstantiate_command | Verify firmware issues uninstantiate command via SW_CMD_REQ before disabling EDN in software port mode. | CTRL, SW_CMD_REQ, SW_CMD_STS, INTR_STATE | csrng_cmd_initiator_socket, intr_edn_cmd_req_done | Positive |
| 55 | test_sw_port_multiword_command | Verify firmware issues multi-word commands (header plus up to 12 data words) via SW_CMD_REQ polling CMD_REG_RDY. | SW_CMD_REQ, SW_CMD_STS | csrng_cmd_initiator_socket | Positive |
| 56 | test_sw_port_cmd_completion_interrupt | Verify edn_cmd_req_done interrupt asserts when software command completes and INTR_ENABLE is set. | SW_CMD_REQ, INTR_STATE, INTR_ENABLE | csrng_cmd_initiator_socket, intr_edn_cmd_req_done | Positive |
| 57 | test_sw_port_entropy_distribution | Verify software port mode distributes entropy to endpoints with FIPS indicator after generate command. | CTRL, SW_CMD_REQ | edn_req[0:7], edn_ack[0:7], edn_bus[0:7], edn_fips[0:7], csrng_genbits_target_socket | Positive |
| 58 | test_mode_priority_boot_over_auto | Verify BOOT_REQ_MODE takes precedence when both BOOT_REQ_MODE and AUTO_REQ_MODE are set to 0x6. | CTRL, MAIN_SM_STATE, HW_CMD_STS | csrng_cmd_initiator_socket | Positive |
| 59 | test_csrng_instantiate_sequence | Verify instantiate command format (command type, clen, flags) sent to CSRNG with personalization string support. | SW_CMD_REQ, SW_CMD_STS | csrng_cmd_initiator_socket | Positive |
| 60 | test_csrng_generate_sequence | Verify generate command format with glen parameter sent to CSRNG and entropy data returned via genbits interface. | SW_CMD_REQ, SW_CMD_STS | csrng_cmd_initiator_socket, csrng_genbits_target_socket | Positive |
| 61 | test_csrng_reseed_sequence | Verify reseed command format with additional data sent to CSRNG for refreshing entropy seed. | SW_CMD_REQ, SW_CMD_STS | csrng_cmd_initiator_socket | Positive |
| 62 | test_csrng_uninstantiate_sequence | Verify uninstantiate command destroys CSRNG instance before EDN reconfiguration. | SW_CMD_REQ, SW_CMD_STS | csrng_cmd_initiator_socket | Positive |
| 63 | test_csrng_command_ordering_nist | Verify command sequencing per NIST SP 800-90A requires instantiate before generate or reseed commands. | SW_CMD_REQ, SW_CMD_STS | csrng_cmd_initiator_socket | Positive |
| 64 | test_csrng_acknowledgment_success | Verify CSRNG acknowledgment with status code 0 indicates successful command completion. | SW_CMD_STS, HW_CMD_STS | csrng_cmd_initiator_socket | Positive |
| 65 | test_csrng_acknowledgment_error | Verify CSRNG acknowledgment with non-zero status code triggers CSRNG_CMD_STS_ALERT in RECOV_ALERT_STS and sets ERR_CODE.SFIFO_ESRNG_ERR. | SW_CMD_STS, HW_CMD_STS, RECOV_ALERT_STS, ERR_CODE, INTR_STATE | csrng_cmd_initiator_socket, alert_recov_alert, alert_fatal_alert, intr_edn_fatal_err | Negative |
| 66 | test_endpoint0_request_acknowledge | Verify endpoint 0 request via edn_req[0] triggers acknowledge on edn_ack[0] and entropy on edn_bus[0]. | CTRL, SW_CMD_REQ | edn_req[0], edn_ack[0], edn_bus[0], edn_fips[0], csrng_genbits_target_socket | Positive |
| 67 | test_endpoint1_request_acknowledge | Verify endpoint 1 request via edn_req[1] triggers acknowledge on edn_ack[1] and entropy on edn_bus[1]. | CTRL, SW_CMD_REQ | edn_req[1], edn_ack[1], edn_bus[1], edn_fips[1], csrng_genbits_target_socket | Positive |
| 68 | test_endpoint2_request_acknowledge | Verify endpoint 2 request via edn_req[2] triggers acknowledge on edn_ack[2] and entropy on edn_bus[2]. | CTRL, SW_CMD_REQ | edn_req[2], edn_ack[2], edn_bus[2], edn_fips[2], csrng_genbits_target_socket | Positive |
| 69 | test_endpoint3_request_acknowledge | Verify endpoint 3 request via edn_req[3] triggers acknowledge on edn_ack[3] and entropy on edn_bus[3]. | CTRL, SW_CMD_REQ | edn_req[3], edn_ack[3], edn_bus[3], edn_fips[3], csrng_genbits_target_socket | Positive |
| 70 | test_endpoint4_request_acknowledge | Verify endpoint 4 request via edn_req[4] triggers acknowledge on edn_ack[4] and entropy on edn_bus[4]. | CTRL, SW_CMD_REQ | edn_req[4], edn_ack[4], edn_bus[4], edn_fips[4], csrng_genbits_target_socket | Positive |
| 71 | test_endpoint5_request_acknowledge | Verify endpoint 5 request via edn_req[5] triggers acknowledge on edn_ack[5] and entropy on edn_bus[5]. | CTRL, SW_CMD_REQ | edn_req[5], edn_ack[5], edn_bus[5], edn_fips[5], csrng_genbits_target_socket | Positive |
| 72 | test_endpoint6_request_acknowledge | Verify endpoint 6 request via edn_req[6] triggers acknowledge on edn_ack[6] and entropy on edn_bus[6]. | CTRL, SW_CMD_REQ | edn_req[6], edn_ack[6], edn_bus[6], edn_fips[6], csrng_genbits_target_socket | Positive |
| 73 | test_endpoint7_request_acknowledge | Verify endpoint 7 request via edn_req[7] triggers acknowledge on edn_ack[7] and entropy on edn_bus[7]. | CTRL, SW_CMD_REQ | edn_req[7], edn_ack[7], edn_bus[7], edn_fips[7], csrng_genbits_target_socket | Positive |
| 74 | test_multiple_endpoints_simultaneous | Verify simultaneous requests from multiple endpoints are arbitrated and all receive entropy data. | CTRL, SW_CMD_REQ | edn_req[0:7], edn_ack[0:7], edn_bus[0:7], edn_fips[0:7], csrng_genbits_target_socket | Positive |
| 75 | test_endpoint_data_persistence | Verify entropy data persists on edn_bus until next request to support asynchronous peripheral consumption. | CTRL, SW_CMD_REQ | edn_req[0], edn_ack[0], edn_bus[0], csrng_genbits_target_socket | Positive |
| 76 | test_data_width_conversion_128_to_32 | Verify 128-bit CSRNG genbits data is converted and buffered into four 32-bit endpoint transactions. | CTRL, SW_CMD_REQ | edn_req[0], edn_ack[0], edn_bus[0], csrng_genbits_target_socket | Positive |
| 77 | test_fips_indicator_propagation_true | Verify edn_fips signals are asserted when CSRNG provides FIPS-approved entropy seed. | CTRL, SW_CMD_REQ | edn_req[0:7], edn_ack[0:7], edn_fips[0:7], csrng_genbits_target_socket | Positive |
| 78 | test_fips_indicator_propagation_false | Verify edn_fips signals are de-asserted when CSRNG provides pre-FIPS entropy seed. | CTRL | edn_req[0:7], edn_ack[0:7], edn_fips[0:7], csrng_genbits_target_socket | Positive |
| 79 | test_interrupt_edn_cmd_req_done_assertion | Verify edn_cmd_req_done interrupt asserts when software command completes and INTR_ENABLE.edn_cmd_req_done is set. | SW_CMD_REQ, INTR_STATE, INTR_ENABLE | csrng_cmd_initiator_socket, intr_edn_cmd_req_done | Positive |
| 80 | test_interrupt_edn_cmd_req_done_clearing | Verify writing 1 to INTR_STATE.edn_cmd_req_done clears interrupt status and de-asserts interrupt signal. | INTR_STATE, INTR_ENABLE | intr_edn_cmd_req_done | Positive |
| 81 | test_interrupt_edn_fatal_err_fifo_overflow | Verify edn_fatal_err interrupt asserts on RESEED_CMD or GENERATE_CMD FIFO overflow with INTR_ENABLE.edn_fatal_err set. | RESEED_CMD, GENERATE_CMD, INTR_STATE, INTR_ENABLE, ERR_CODE | intr_edn_fatal_err, alert_fatal_alert | Negative |
| 82 | test_interrupt_edn_fatal_err_clearing | Verify writing 1 to INTR_STATE.edn_fatal_err clears interrupt status but ERR_CODE remains sticky until reset. | INTR_STATE, INTR_ENABLE, ERR_CODE | intr_edn_fatal_err, alert_fatal_alert | Positive |
| 83 | test_alert_recov_edn_enable_field | Verify recoverable alert asserts when CTRL.EDN_ENABLE contains invalid multi-bit encoded value and RECOV_ALERT_STS.EDN_ENABLE_FIELD_ALERT is set. | CTRL, RECOV_ALERT_STS | alert_recov_alert | Negative |
| 84 | test_alert_recov_boot_req_mode_field | Verify recoverable alert asserts when CTRL.BOOT_REQ_MODE contains invalid value and RECOV_ALERT_STS.BOOT_REQ_MODE_FIELD_ALERT is set. | CTRL, RECOV_ALERT_STS | alert_recov_alert | Negative |
| 85 | test_alert_recov_auto_req_mode_field | Verify recoverable alert asserts when CTRL.AUTO_REQ_MODE contains invalid value and RECOV_ALERT_STS.AUTO_REQ_MODE_FIELD_ALERT is set. | CTRL, RECOV_ALERT_STS | alert_recov_alert | Negative |
| 86 | test_alert_recov_cmd_fifo_rst_field | Verify recoverable alert asserts when CTRL.CMD_FIFO_RST contains invalid value and RECOV_ALERT_STS.CMD_FIFO_RST_FIELD_ALERT is set. | CTRL, RECOV_ALERT_STS | alert_recov_alert | Negative |
| 87 | test_alert_recov_entropy_bus_cmp | Verify recoverable alert asserts when consecutive entropy bus values match and RECOV_ALERT_STS.ENTROPY_BUS_CMP_ALERT is set. | RECOV_ALERT_STS | alert_recov_alert, csrng_genbits_target_socket | Negative |
| 88 | test_alert_recov_csrng_cmd_sts | Verify recoverable alert asserts when CSRNG returns non-zero status and RECOV_ALERT_STS.CSRNG_CMD_STS_ALERT is set. | SW_CMD_REQ, RECOV_ALERT_STS, ERR_CODE | csrng_cmd_initiator_socket, alert_recov_alert, alert_fatal_alert | Negative |
| 89 | test_alert_recov_clearing_w0c | Verify recoverable alert status bits are cleared by writing 0 to corresponding RECOV_ALERT_STS bit positions. | RECOV_ALERT_STS | alert_recov_alert | Positive |
| 90 | test_alert_fatal_main_sm_illegal_state | Verify fatal alert asserts when EDN_MAIN_SM enters illegal state and ERR_CODE.EDN_MAIN_SM_ERR is set. | ERR_CODE, INTR_STATE | alert_fatal_alert, intr_edn_fatal_err | Negative |
| 91 | test_alert_fatal_ack_sm_illegal_state | Verify fatal alert asserts when EDN_ACK_SM enters illegal state and ERR_CODE.EDN_ACK_SM_ERR is set. | ERR_CODE, INTR_STATE | alert_fatal_alert, intr_edn_fatal_err | Negative |
| 92 | test_alert_fatal_reseed_fifo_overflow | Verify fatal alert asserts when RESEED_CMD FIFO exceeds 13 words and ERR_CODE.SFIFO_RESCMD_ERR is set. | RESEED_CMD, ERR_CODE, INTR_STATE | alert_fatal_alert, intr_edn_fatal_err | Negative |
| 93 | test_alert_fatal_generate_fifo_overflow | Verify fatal alert asserts when GENERATE_CMD FIFO exceeds 13 words and ERR_CODE.SFIFO_GENCMD_ERR is set. | GENERATE_CMD, ERR_CODE, INTR_STATE | alert_fatal_alert, intr_edn_fatal_err | Negative |
| 94 | test_alert_fatal_fifo_write_error | Verify fatal alert asserts on internal FIFO write error and ERR_CODE.FIFO_WRITE_ERR is set. | ERR_CODE, INTR_STATE | alert_fatal_alert, intr_edn_fatal_err | Negative |
| 95 | test_alert_fatal_fifo_read_error | Verify fatal alert asserts on internal FIFO read error and ERR_CODE.FIFO_READ_ERR is set. | ERR_CODE, INTR_STATE | alert_fatal_alert, intr_edn_fatal_err | Negative |
| 96 | test_alert_fatal_fifo_state_error | Verify fatal alert asserts on internal FIFO state error and ERR_CODE.FIFO_STATE_ERR is set. | ERR_CODE, INTR_STATE | alert_fatal_alert, intr_edn_fatal_err | Negative |
| 97 | test_alert_fatal_counter_error | Verify fatal alert asserts on hardened counter error and ERR_CODE.EDN_CNTR_ERR is set. | ERR_CODE, INTR_STATE | alert_fatal_alert, intr_edn_fatal_err | Negative |
| 98 | test_alert_fatal_sticky_until_reset | Verify fatal alert remains asserted until system reset when ERR_CODE bits are set and cannot be cleared by firmware. | ERR_CODE, INTR_STATE | alert_fatal_alert, rst_ni | Negative |
| 99 | test_error_reseed_fifo_13word_boundary | Verify RESEED_CMD FIFO accepts exactly 13 words without overflow error. | RESEED_CMD, ERR_CODE | tlm_reg_target_socket | Positive |
| 100 | test_error_reseed_fifo_14word_overflow | Verify RESEED_CMD FIFO overflow error triggers on 14th word write setting ERR_CODE.SFIFO_RESCMD_ERR. | RESEED_CMD, ERR_CODE, INTR_STATE | alert_fatal_alert, intr_edn_fatal_err | Negative |
| 101 | test_error_generate_fifo_13word_boundary | Verify GENERATE_CMD FIFO accepts exactly 13 words without overflow error. | GENERATE_CMD, ERR_CODE | tlm_reg_target_socket | Positive |
| 102 | test_error_generate_fifo_14word_overflow | Verify GENERATE_CMD FIFO overflow error triggers on 14th word write setting ERR_CODE.SFIFO_GENCMD_ERR. | GENERATE_CMD, ERR_CODE, INTR_STATE | alert_fatal_alert, intr_edn_fatal_err | Negative |
| 103 | test_error_sw_cmd_without_instantiate | Verify issuing generate or reseed command without prior instantiate returns CSRNG error status. | SW_CMD_REQ, SW_CMD_STS, RECOV_ALERT_STS, ERR_CODE | csrng_cmd_initiator_socket, alert_recov_alert, alert_fatal_alert | Negative |
| 104 | test_error_auto_mode_max_reqs_zero | Verify setting MAX_NUM_REQS_BETWEEN_RESEEDS to 0 in auto mode disables automatic generate commands. | CTRL, MAX_NUM_REQS_BETWEEN_RESEEDS, GENERATE_CMD, MAIN_SM_STATE | edn_req[0], csrng_cmd_initiator_socket | Negative |
| 105 | test_error_auto_mode_max_reqs_exceeds_csrng | Verify MAX_NUM_REQS_BETWEEN_RESEEDS exceeding CSRNG RESEED_INTERVAL causes CSRNG to reject commands. | MAX_NUM_REQS_BETWEEN_RESEEDS, CTRL, SW_CMD_STS, ERR_CODE | csrng_cmd_initiator_socket, alert_fatal_alert | Negative |
| 106 | test_error_uninstantiate_without_complete | Verify disabling EDN without uninstantiate command causes desynchronization with CSRNG. | CTRL, SW_CMD_REQ | csrng_cmd_initiator_socket | Negative |
| 107 | test_corner_boot_mode_csrng_error | Verify CSRNG error during boot-time instantiate or generate transitions state machine to Error state. | CTRL, BOOT_INS_CMD, HW_CMD_STS, ERR_CODE, MAIN_SM_STATE | csrng_cmd_initiator_socket, alert_fatal_alert | Negative |
| 108 | test_corner_auto_mode_exit_during_command | Verify clearing AUTO_REQ_MODE during active generate or reseed command waits for completion before transitioning to SWPortMode. | CTRL, MAIN_SM_STATE, HW_CMD_STS | csrng_cmd_initiator_socket | Positive |
| 109 | test_corner_sw_cmd_req_without_cmd_reg_rdy | Verify writing to SW_CMD_REQ without polling CMD_REG_RDY causes command word loss or protocol error. | SW_CMD_REQ, SW_CMD_STS | csrng_cmd_initiator_socket | Negative |
| 110 | test_corner_multiple_mode_disable | Verify clearing both BOOT_REQ_MODE and AUTO_REQ_MODE simultaneously transitions to SWPortMode correctly. | CTRL, MAIN_SM_STATE | csrng_cmd_initiator_socket | Positive |
| 111 | test_corner_ctrl_write_when_regwen_locked | Verify attempting to write CTRL register when REGWEN=0 has no effect and configuration remains unchanged. | REGWEN, CTRL | tlm_reg_target_socket | Negative |
| 112 | test_corner_endpoint_request_no_entropy | Verify endpoint request when no entropy available in buffer triggers CSRNG generate command. | CTRL, SW_CMD_REQ | edn_req[0], edn_ack[0], edn_bus[0], csrng_cmd_initiator_socket, csrng_genbits_target_socket | Positive |
| 113 | test_corner_endpoint_burst_requests | Verify multiple consecutive requests from single endpoint are serviced with fresh entropy data. | CTRL, SW_CMD_REQ | edn_req[0], edn_ack[0], edn_bus[0], csrng_genbits_target_socket | Positive |
| 114 | test_corner_cmd_fifo_rst_during_auto_mode | Verify setting CTRL.CMD_FIFO_RST=0x6 during auto mode clears command FIFOs and requires reconfiguration. | CTRL, RESEED_CMD, GENERATE_CMD | tlm_reg_target_socket | Positive |
| 115 | test_corner_intr_enable_after_intr_state | Verify enabling interrupt via INTR_ENABLE after INTR_STATE bit already set immediately asserts interrupt signal. | INTR_STATE, INTR_ENABLE | intr_edn_cmd_req_done, intr_edn_fatal_err | Positive |
| 116 | test_corner_alert_test_no_status_change | Verify ALERT_TEST register pulses alert signals without modifying RECOV_ALERT_STS or ERR_CODE registers. | ALERT_TEST, RECOV_ALERT_STS, ERR_CODE | alert_recov_alert, alert_fatal_alert | Positive |
| 117 | test_reset_during_boot_mode | Verify asserting reset during boot-time mode operation immediately disables EDN and clears state machine. | CTRL, MAIN_SM_STATE | rst_ni, csrng_cmd_initiator_socket | Positive |
| 118 | test_reset_during_auto_mode | Verify asserting reset during auto request mode operation immediately disables EDN and clears state machine. | CTRL, MAIN_SM_STATE | rst_ni, csrng_cmd_initiator_socket | Positive |
| 119 | test_reset_during_sw_port_command | Verify asserting reset during software port command execution aborts command and resets EDN to idle state. | CTRL, SW_CMD_REQ, MAIN_SM_STATE | rst_ni, csrng_cmd_initiator_socket | Positive |
| 120 | test_reset_clears_err_code | Verify system reset clears sticky ERR_CODE register bits and de-asserts fatal alert. | ERR_CODE, INTR_STATE | rst_ni, alert_fatal_alert | Positive |
| 121 | test_reset_clears_recov_alert_sts | Verify system reset clears RECOV_ALERT_STS register bits and de-asserts recoverable alert. | RECOV_ALERT_STS | rst_ni, alert_recov_alert | Positive |
| 122 | test_reset_restores_regwen | Verify system reset restores REGWEN register to reset value 0x1 enabling CTRL writes. | REGWEN, CTRL | rst_ni, tlm_reg_target_socket | Positive |
| 123 | test_state_idle_to_boot_transition | Verify state machine transitions from Idle to BootInsAckWait when CTRL enables boot-time mode. | CTRL, MAIN_SM_STATE | csrng_cmd_initiator_socket | Positive |
| 124 | test_state_boot_to_swport_transition | Verify state machine transitions from boot-time states to SWPortMode when BOOT_REQ_MODE is cleared. | CTRL, MAIN_SM_STATE | csrng_cmd_initiator_socket | Positive |
| 125 | test_state_idle_to_auto_transition | Verify state machine transitions from Idle to AutoLoadIns when CTRL enables auto request mode. | CTRL, MAIN_SM_STATE | csrng_cmd_initiator_socket | Positive |
| 126 | test_state_auto_to_swport_transition | Verify state machine transitions from auto request states to SWPortMode when AUTO_REQ_MODE is cleared. | CTRL, MAIN_SM_STATE | csrng_cmd_initiator_socket | Positive |
| 127 | test_state_swport_stable | Verify state machine remains in SWPortMode during software command sequences until mode change or disable. | CTRL, SW_CMD_REQ, MAIN_SM_STATE | csrng_cmd_initiator_socket | Positive |
| 128 | test_state_error_on_illegal_state | Verify state machine transitions to Error state when illegal state is detected in EDN_MAIN_SM or EDN_ACK_SM. | MAIN_SM_STATE, ERR_CODE | alert_fatal_alert | Negative |
| 129 | test_initialization_sequence | Verify correct initialization sequence: enable ENTROPY_SRC, then CSRNG, then EDN per recommended flow. | CTRL | csrng_cmd_initiator_socket | Positive |
| 130 | test_disable_sequence_with_uninstantiate | Verify correct shutdown sequence: issue uninstantiate command, wait for completion, then clear EDN_ENABLE. | CTRL, SW_CMD_REQ, SW_CMD_STS | csrng_cmd_initiator_socket | Positive |
| 131 | test_reconfiguration_sequence | Verify reconfiguring EDN operating mode requires proper uninstantiate, mode change, and re-instantiate sequence. | CTRL, SW_CMD_REQ, MAIN_SM_STATE | csrng_cmd_initiator_socket | Positive |
| 132 | test_boot_to_auto_mode_transition | Verify transitioning from boot-time mode to auto request mode requires exit to SWPortMode first. | CTRL, MAIN_SM_STATE | csrng_cmd_initiator_socket | Positive |
| 133 | test_auto_to_boot_mode_transition | Verify transitioning from auto request mode to boot-time mode requires full disable and re-enable sequence. | CTRL, MAIN_SM_STATE, SW_CMD_REQ | csrng_cmd_initiator_socket | Positive |
| 134 | test_command_header_parsing | Verify SW_CMD_REQ correctly parses command header fields including command type, clen, flags, and glen. | SW_CMD_REQ, SW_CMD_STS | csrng_cmd_initiator_socket | Positive |
| 135 | test_command_clen_validation | Verify clen field in command header matches actual number of additional data words written to SW_CMD_REQ. | SW_CMD_REQ, SW_CMD_STS | csrng_cmd_initiator_socket | Positive |
| 136 | test_command_clen_mismatch | Verify mismatched clen field and actual data word count causes CSRNG protocol error. | SW_CMD_REQ, SW_CMD_STS, ERR_CODE | csrng_cmd_initiator_socket, alert_fatal_alert | Negative |
| 137 | test_personalization_string_support | Verify instantiate and reseed commands support personalization string via additional data words in SW_CMD_REQ. | SW_CMD_REQ, SW_CMD_STS | csrng_cmd_initiator_socket | Positive |
| 138 | test_generate_glen_parameter | Verify generate command glen parameter controls amount of entropy requested from CSRNG. | SW_CMD_REQ, SW_CMD_STS | csrng_cmd_initiator_socket, csrng_genbits_target_socket | Positive |
| 139 | test_generate_glen_maximum | Verify generate command with maximum glen value (0xFFF for 4096 blocks) per NIST SP 800-90A. | SW_CMD_REQ, SW_CMD_STS | csrng_cmd_initiator_socket, csrng_genbits_target_socket | Positive |
| 140 | test_arbiter_round_robin | Verify endpoint arbiter uses round-robin or priority scheme for fair access when multiple endpoints request simultaneously. | CTRL, SW_CMD_REQ | edn_req[0:7], edn_ack[0:7], edn_bus[0:7], csrng_genbits_target_socket | Positive |
| 141 | test_buffer_multiple_endpoint_requests | Verify internal entropy buffer satisfies multiple endpoint requests from single CSRNG generate command. | CTRL, SW_CMD_REQ | edn_req[0:3], edn_ack[0:3], edn_bus[0:3], csrng_genbits_target_socket | Positive |
| 142 | test_buffer_depletion_triggers_generate | Verify entropy buffer depletion triggers automatic generate command to CSRNG in auto request mode. | CTRL, GENERATE_CMD | edn_req[0], csrng_cmd_initiator_socket, csrng_genbits_target_socket | Positive |
| 143 | test_concurrent_register_endpoint_access | Verify concurrent register writes and endpoint entropy requests are handled correctly without interference. | CTRL, SW_CMD_REQ | edn_req[0:7], edn_ack[0:7], tlm_reg_target_socket | Positive |
| 144 | test_entropy_freshness | Verify each endpoint request receives fresh entropy data not reused from previous requests. | CTRL, SW_CMD_REQ | edn_req[0], edn_ack[0], edn_bus[0], csrng_genbits_target_socket | Positive |
| 145 | test_fips_transition_pre_to_approved | Verify edn_fips signals transition from de-asserted to asserted when CSRNG seed becomes FIPS-approved. | CTRL, SW_CMD_REQ | edn_req[0:7], edn_fips[0:7], csrng_genbits_target_socket | Positive |

---

## Test Coverage Summary

### Register Coverage
- **Interrupt Registers**: INTR_STATE, INTR_ENABLE, INTR_TEST (Tests 1-4)
- **Alert Registers**: ALERT_TEST (Test 5)
- **Control Registers**: REGWEN, CTRL (Tests 6-15)
- **Boot Configuration**: BOOT_INS_CMD, BOOT_GEN_CMD (Tests 16-17)
- **Software Command Interface**: SW_CMD_REQ, SW_CMD_STS (Tests 18-22)
- **Hardware Status**: HW_CMD_STS (Tests 23-26)
- **Auto Mode Configuration**: RESEED_CMD, GENERATE_CMD, MAX_NUM_REQS_BETWEEN_RESEEDS (Tests 27-29)
- **Error Status**: RECOV_ALERT_STS, ERR_CODE, ERR_CODE_TEST (Tests 30-32)
- **State Visibility**: MAIN_SM_STATE (Test 33)
- **Reserved Bits**: All registers (Test 34)

### Operating Mode Coverage
- **Boot-Time Request Mode**: Tests 35-41 (7 tests)
- **Auto Request Mode**: Tests 42-49 (8 tests)
- **Software Port Mode**: Tests 50-57 (8 tests)
- **Mode Transitions**: Tests 58, 110, 123-127, 132-133 (10 tests)

### CSRNG Command Coverage
- **Instantiate**: Tests 51, 59, 63
- **Generate**: Tests 52, 60, 138-139
- **Reseed**: Tests 53, 61
- **Uninstantiate**: Tests 54, 62
- **Command Sequencing**: Tests 63-65, 103
- **Command Format**: Tests 134-137

### Endpoint Interface Coverage
- **Individual Endpoints**: Tests 66-73 (8 tests, one per endpoint)
- **Multiple Endpoints**: Tests 74, 140-141
- **Data Handling**: Tests 75-76, 112-113, 144
- **FIPS Indicator**: Tests 77-78, 145

### Interrupt Coverage
- **edn_cmd_req_done**: Tests 79-80
- **edn_fatal_err**: Tests 81-82

### Alert Coverage
- **Recoverable Alerts**: Tests 83-89 (7 tests covering all 6 alert sources)
- **Fatal Alerts**: Tests 90-98 (9 tests covering all error conditions)

### Error Condition Coverage
- **FIFO Overflow**: Tests 92-93, 99-102
- **State Machine Errors**: Tests 90-91, 128
- **CSRNG Errors**: Tests 65, 88, 103, 107
- **Configuration Errors**: Tests 83-86, 104-106
- **Corner Cases**: Tests 108-116

### Reset Coverage
- **Reset During Operations**: Tests 117-119
- **Reset Effects**: Tests 120-122

### Additional Coverage
- **Multi-bit Encoding**: Tests 8-15, 83-86
- **Register Protection**: Tests 6-7, 111
- **Data Width Conversion**: Test 76
- **Arbitration**: Tests 74, 140
- **Buffer Management**: Tests 141-142
- **Initialization/Shutdown**: Tests 129-131

## Total Test Cases: 145

### Test Type Distribution
- **Positive Tests**: 115 tests (79.3%)
- **Negative Tests**: 30 tests (20.7%)

## Notes

1. All port and signal names are referenced exactly as defined in `/home/shravanr/Documents/tvastaavp/edn/docs/sections/edn-port-interfaces.md`.

2. All register names are referenced exactly as defined in `/home/shravanr/Documents/tvastaavp/edn/docs/sections/edn-memory-map-registers.md`.

3. Each interrupt source (edn_cmd_req_done, edn_fatal_err) has dedicated test cases for assertion and clearing.

4. Each alert type (recoverable and fatal) has test cases covering all triggering conditions per the detailed design specification.

5. All 8 peripheral endpoint interfaces have individual test cases plus combined arbitration tests.

6. NIST SP 800-90A command sequencing requirements are validated through dedicated test cases.

7. Corner cases include boundary conditions, error injection, concurrent operations, and mode transition scenarios.

8. The test plan excludes pin-level configurations, electrical timing, cycle-accurate behavior, and items listed in "Is Not Modeled" per the EDN features specification.
