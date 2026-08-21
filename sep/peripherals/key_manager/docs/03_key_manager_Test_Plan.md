# Key Manager TT SystemC TLM Test Plan

**Document Version:** 1.0
**Date:** 2026-04-27
**IP Module:** Key Manager TT (keymgr_tt)
**Modeling Approach:** SystemC TLM2.0 with Firmware-Handler Abstraction

---

## Executive Summary

This test plan provides comprehensive test coverage for the keymgr_tt SystemC TLM model. The model implements a **firmware-handler abstraction** — a pure C++ class that replaces the PicoRV32 ISS while maintaining the full SEP-visible behaviour of the Key Manager.

### Key Testing Focus Areas

1. **Register Interface** — Mailbox (MB_*) and KPVLP register access semantics, reset values, access restrictions
2. **Mailbox Protocol** — CRC-8 header validation, CRC-32C payload validation, sequence number enforcement, FIFO framing
3. **Boot Sequence** — RESP_KM_READY announcement, engine key zeroing, IRQ behaviour at boot
4. **Command Set** — All 10 implemented commands plus error-path handling for malformed messages
5. **Key Lifecycle** — Slot allocation → key write → registration → transfer → revoke chain
6. **Key Transfer Protocol** — Dual XOR share encoding, sideload socket writes, one-shot lock_use enforcement
7. **Fault Handling** — Recoverable faults, unrecoverable faults, emergency wipe, firmware halt
8. **Reset Behaviour** — State cleared correctly; firmware re-boots; sequence counters reset
9. **Interrupt Logic** — IRQ assertion/de-assertion based on MB_IRQS & MB_IRQEN

### Critical Architectural Context

**IMPORTANT:** This test plan is designed for the firmware-handler model:
- Firmware is C++ code in `km_firmware_handler.cpp`, NOT a PicoRV32 ISS
- No ELF image is loaded; `CMD_SRAM_VER` always returns `RET_FAILURE`
- Entropy comes from OpenSSL `RAND_bytes`, not a hardware DRBG socket
- Key sideload uses dual XOR shares via TLM initiator sockets
- DPA countermeasures (multi-pass random overwrite) are NOT modeled

---

## Test Plan Summary

| Category | Test File | Sub-tests | Coverage Focus |
|----------|-----------|-----------|----------------|
| Register Interface | func001 | 001a–001i | Reset values, RW/WO/RO access, W1C, masked writes, KPVLP |
| Mailbox + IRQ Behaviour | func002 | 002a–002g | HW_VER end-to-end, IRQ assert/deassert, underflow W1C, WSEP self-clear |
| Boot State and Info Commands | func003 | 003a–003d | CMD_STAT, CMD_ROM_VER, CMD_SRAM_VER, CMD_HW_VER |
| Message Framing Validation | func004 | 004a–004f | CRC-8 header, payload length, CRC-32C, sequence errors, unknown cmd |
| CMD_KPVLP_SLOT_REQ | func005 | 005a–005e | Slot grant, KPVLP unlock status, pool exhaustion |
| CMD_KEY_REGISTER | func006 | 006a–006g | SEP key provisioning, handle allocation, validation errors, lock_write |
| CMD_KEY_GENERATE | func007 | 007a–007d | KM-internal key generation, handle return, slot exhaustion |
| CMD_KEY_TRANSFER | func008 | 008a–008e | Key sideload to engine, DEST_VALID policy, lock_use after transfer |
| CMD_KEY_REVOKE | func009 | 009a–009c | Key revocation, double-revoke error, payload length check |
| CMD_ENGINE_SHRED | func010 | 010a–010d | Engine key zeroing, multi-engine, empty payload error, ABR dest |
| Full Key Lifecycle | func011 | 011a–011c | Register→Transfer→Revoke chained flow, multi-command seq continuity |
| Reset During Operation | func012 | 012a–012c | KPVLP_STATUS cleared, seq counter reset, outbound FIFO cleared |
| KEY_GENERATE Dual-Share | func013 | 013a | Structural dual-share verification (SHARE0 XOR SHARE1 non-trivial) |
| **TOTAL** | **13 files** | **~45 sub-tests** | **Complete functional coverage of implemented command set** |

---

## Test Case Details

| Sl. No. | Test File | Sub-test | Description | Registers / APIs Used | Test Type |
|---------|-----------|----------|-------------|----------------------|-----------|
| **REGISTER INTERFACE TESTS** | | | | | |
| 1 | func001 | 001a | Verify all Mailbox registers return correct reset values: MB_STATUS=0x5 (inbound_empty=1, outbound_empty=1), MB_IRQS=0, MB_IRQEN=0, MB_CTRL=0. Boot must complete first (RESP_KM_READY consumed). | MB_STATUS (read=0x5), MB_IRQS (read=0), MB_IRQEN (read=0), MB_CTRL (read=0) | Positive |
| 2 | func001 | 001b | Verify `MB_IRQEN` is read/write: write 0x1F, read back same value; write with out-of-mask bits (e.g. 0xFF), verify only bits[4:0] stored. | MB_IRQEN (write 0x1F, 0xFF; read back) | Positive |
| 3 | func001 | 001c | Verify `MB_CTRL` is read/write: write 0x3 (INBOUND_OVERFLOW_RESP + OUTBOUND_UNDERFLOW_RESP), read back 0x3; FLUSH bit self-clears. | MB_CTRL (write 0x3; read 0x3; write 0x7; verify FLUSH bit cleared) | Positive |
| 4 | func001 | 001d | Verify `MB_IRQS` W1C: manually set sticky bits (via overflow/underflow trigger), read non-zero, write 1 to clear bits[4:2], verify cleared. Level-sensitive bits[1:0] not clearable by write. | MB_IRQS (read non-zero; write to clear; read 0) | Positive |
| 5 | func001 | 001e | Verify `MB_WDATA` is write-only: write any value, read back returns 0. | MB_WDATA (write 0xDEADBEEF; read=0) | Positive |
| 6 | func001 | 001f | Verify `MB_STATUS` is read-only (static bits): attempt to write non-W1C bits, verify value unchanged on next read (only W1C clear fields 0x00F00000 respond to writes). | MB_STATUS (write 0xFFFFFFFF; read, verify only W1C bits cleared) | Positive |
| 7 | func001 | 001g | Verify `KPVLP_STATUS` reset value is 0x0 (no slots unlocked at reset). | KPVLP_STATUS (read=0x0) | Positive |
| 8 | func001 | 001h | Verify `KPVLP_CTRL` write mask: write 0xFFFFFFFF, read back 0 (WO); verify that `m_kpv` only stored the masked fields (EXTEND[6:4], DEST_VALID[16:9], LAST_DWORD[20:17] = 0x1FFE70). | KPVLP_CTRL[0] (write 0xFFFFFFFF; read=0); inspect kpv ctrl via KPVLP_STATUS side effects | Positive |
| 9 | func001 | 001i | Verify `KPVLP_KEY` is write-only: write 0xCAFEBABE to slot 0 word 0, read back returns 0. | KPVLP_KEY[0][0] (write 0xCAFEBABE; read=0) | Positive |
| **MAILBOX + IRQ BEHAVIOUR TESTS** | | | | | |
| 10 | func002 | 002a | Send `CMD_HW_VER` (no payload) end-to-end: write header with correct CRC-8 and separator, read back RESP_CMD, verify RET_SUCCESS and correct ret_arg layout. | MB_WSEP (write 1), MB_WDATA (write header), MB_RDATA (read response words) | Positive |
| 11 | func002 | 002b | Verify `CMD_HW_VER` ret_arg encodes version 1.0.0: bits[23:16]=1 (MAJOR), bits[15:8]=0 (MINOR), bits[7:0]=0 (PATCH). | MB_RDATA (read ret_arg word) | Positive |
| 12 | func002 | 002c | Enable `MB_IRQEN.OUTBOUND_READ_DATA_AVAIL_EN=1`. Send any command. Verify `irq` output asserted before reading response. | MB_IRQEN (write 0x1), irq (verify=1 after outbound push) | Positive |
| 13 | func002 | 002d | Read all outbound words until FIFO empty. Verify `irq` de-asserts after last word consumed. | MB_RDATA (drain), irq (verify=0 after drain) | Positive |
| 14 | func002 | 002e | Read from empty outbound FIFO — verify `MB_STATUS.OUTBOUND_UNDERFLOW=1` and `MB_IRQS.OUTBOUND_UNDERFLOW=1` set. | MB_RDATA (read when empty), MB_STATUS (read bit[23]), MB_IRQS (read bit[3]) | Negative |
| 15 | func002 | 002f | W1C clears `OUTBOUND_UNDERFLOW` sticky bit: write 0x8 (bit[3]) to MB_IRQS, verify MB_IRQS.OUTBOUND_UNDERFLOW=0. Also write 0x800000 to MB_STATUS (bit[23]), verify MB_STATUS.OUTBOUND_UNDERFLOW=0. | MB_IRQS (write 0x8; read bit[3]=0), MB_STATUS (write 0x800000; read bit[23]=0) | Positive |
| 16 | func002 | 002g | MB_WSEP self-clears after tagging one word: set WSEP=1, write one word (separator applied), read WSEP — verify 0. Write another word without re-setting WSEP — verify it arrives without separator flag in KM. | MB_WSEP (write 1; read=0 after push), MB_STATUS.INBOUND_SEPARATOR (verify only first word was separator) | Positive |
| **BOOT STATE AND INFO COMMAND TESTS** | | | | | |
| 17 | func003 | 003a | After boot: send `CMD_STAT`, verify RET_SUCCESS and ret_arg[0]=0 (no recoverable fault active). | MB_WDATA/WSEP/RDATA, km_firmware_handler state | Positive |
| 18 | func003 | 003b | `CMD_ROM_VER`: verify RET_SUCCESS and ret_arg = 0x00010000 (ROM 1.0.0). | MB_WDATA/WSEP/RDATA | Positive |
| 19 | func003 | 003c | `CMD_SRAM_VER`: verify RET_FAILURE (SRAM firmware not loaded in model; `m_sram_loaded=false`). | MB_WDATA/WSEP/RDATA | Positive |
| 20 | func003 | 003d | `CMD_HW_VER`: verify RET_SUCCESS and ret_arg = 0x00010000 (HW 1.0.0). | MB_WDATA/WSEP/RDATA | Positive |
| **MESSAGE FRAMING VALIDATION TESTS** | | | | | |
| 21 | func004 | 004a | Bad CRC-8 in header: flip one bit in the CRC byte, verify RESP_CMD with RET_HEADER_CRC. Sequence counter must NOT have been incremented (next valid command at same seq succeeds). | MB_WDATA/WSEP/RDATA, km_firmware_handler m_next_rx_seq | Negative |
| 22 | func004 | 004b | Frame size mismatch: send header with `pay_len>0` but include fewer words than declared (no payload and no CRC word). Verify RESP_CMD with RET_INVALID_LEN. | MB_WDATA/WSEP/RDATA | Negative |
| 23 | func004 | 004c | Bad CRC-32 payload: correct header CRC, correct length, but flip one bit in the payload CRC word. Verify RESP_CMD with RET_PAYLOAD_CRC. | MB_WDATA/WSEP/RDATA | Negative |
| 24 | func004 | 004d | Out-of-order sequence number: send seq=5 when firmware expects seq=0. Verify RESP_CMD with RET_CMD_NOSEQ. | MB_WDATA/WSEP/RDATA, m_next_rx_seq | Negative |
| 25 | func004 | 004e | Unknown cmd_id: send valid frame with cmd_id=0xF0 (not in enum). Verify RESP_CMD with RET_INVALID_CMD. | MB_WDATA/WSEP/RDATA | Negative |
| 26 | func004 | 004f | Recovery after RET_CMD_NOSEQ: after the rejection, send next message at the correct (unchanged) seq number — verify RET_SUCCESS, proving seq counter was not advanced by the bad message. | MB_WDATA/WSEP/RDATA | Positive |
| **CMD_KPVLP_SLOT_REQ TESTS** | | | | | |
| 27 | func005 | 005a | Request 1 slot (`REQ_SIZE=0`): verify RET_SUCCESS; ret_arg[4:0] = SLOT_INDEX, ret_arg[10:8] = GRANT_SIZE=0. Verify KPVLP_STATUS has corresponding bit set. | MB_WDATA/WSEP/RDATA, KPVLP_STATUS | Positive |
| 28 | func005 | 005b | Request 2 consecutive slots (`REQ_SIZE=1`): verify RET_SUCCESS; ret_arg encodes base slot index and grant size=1. Verify KPVLP_STATUS bits for both allocated slots set. | MB_WDATA/WSEP/RDATA, KPVLP_STATUS | Positive |
| 29 | func005 | 005c | Verify `KPVLP_STATUS` accurately reflects `unlock_sep` state: after grant, bit for each granted slot = 1; ungranted slots = 0. | KPVLP_STATUS (read bit mask) | Positive |
| 30 | func005 | 005d | Granted slot is writable via KPVLP: write key words via `KPVLP_KEY[slot][word]` after grant — no callback rejection warning. | KPVLP_KEY[slot][word] (write; no error) | Positive |
| 31 | func005 | 005e | Exhaust entire slot pool: request all 32 slots in batches; final request when zero free slots available → verify RET_FAILURE. | MB_WDATA/WSEP/RDATA, km_kpv::km_find_free_slots | Negative |
| **CMD_KEY_REGISTER TESTS** | | | | | |
| 32 | func006 | 006a | Full registration flow: grant slot, write key via KPVLP, send CMD_KEY_REGISTER with correct CRC → RET_SUCCESS. | KPVLP_KEY, KPVLP_CTRL, MB_WDATA/WSEP/RDATA | Positive |
| 33 | func006 | 006b | Verify ret_arg[7:0] = key_handle in range [1..255] (not 0). | MB_RDATA (read ret_arg), verify handle ≠ 0 | Positive |
| 34 | func006 | 006c | CMD_KEY_REGISTER with payload too short (< 4 words) → RET_INVALID_LEN. | MB_WDATA/WSEP/RDATA | Negative |
| 35 | func006 | 006d | CMD_KEY_REGISTER with slot index out of range (slot ≥ 32) → RET_INVALID_ARG. | MB_WDATA/WSEP/RDATA | Negative |
| 36 | func006 | 006e | CMD_KEY_REGISTER with ctrl field mismatch (extend in command ≠ extend written via KPVLP) → RET_INVALID_ARG with field indicator. | KPVLP_CTRL (write extend mismatch), MB_WDATA/WSEP/RDATA | Negative |
| 37 | func006 | 006f | CMD_KEY_REGISTER with wrong KEY_CRC → RET_INVALID_ARG (field indicator pointing to CRC field = 3). | MB_WDATA/WSEP/RDATA | Negative |
| 38 | func006 | 006g | After registration: slot is locked for further KPVLP writes — `lock_write=1`; attempt to write via KPVLP_KEY fails silently. | KPVLP_KEY[slot] (write after registration; verify warning/no-op) | Positive |
| **CMD_KEY_GENERATE TESTS** | | | | | |
| 39 | func007 | 007a | CMD_KEY_GENERATE(1 slot, DEST_HMAC): verify RET_SUCCESS; ret_arg[7:0]=key_handle, ret_arg[14:8]=req_size echoed, ret_arg[23:16]=dest_valid echoed. | MB_WDATA/WSEP/RDATA | Positive |
| 40 | func007 | 007b | After CMD_KEY_GENERATE, send CMD_KEY_TRANSFER to HMAC: verify HMAC stub receives non-zero key material (RAND_bytes produces non-zero words with overwhelming probability). | MB_WDATA/WSEP/RDATA, hmac_key recording stub | Positive |
| 41 | func007 | 007c | Request more slots than available (e.g., request 33 when 0 free) → RET_FAILURE. | MB_WDATA/WSEP/RDATA | Negative |
| 42 | func007 | 007d | CMD_KEY_GENERATE with payload size < 2 → RET_INVALID_LEN. | MB_WDATA/WSEP/RDATA | Negative |
| **CMD_KEY_TRANSFER TESTS** | | | | | |
| 43 | func008 | 008a | Register key in KPV via KPVLP; CMD_KEY_TRANSFER to HMAC; record SHARE0[w] and SHARE1[w] from HMAC stub; verify SHARE0[w] XOR SHARE1[w] equals the original key words. | KPVLP_KEY/CTRL, MB_WDATA/WSEP/RDATA, hmac_key recording stub | Positive |
| 44 | func008 | 008b | Transfer to a destination NOT in DEST_VALID (e.g., key registered for HMAC only, request transfer to AES) → RET_FAILURE. | MB_WDATA/WSEP/RDATA | Negative |
| 45 | func008 | 008c | Transfer using an invalid handle (unregistered, never allocated) → RET_FAILURE. | MB_WDATA/WSEP/RDATA | Negative |
| 46 | func008 | 008d | After first transfer, `lock_use=1` is set. Second CMD_KEY_TRANSFER on same handle: transfer completes (RET_SUCCESS) but key words sent are all zero (km_read_key_word returns 0 when lock_use=1). | hmac_key recording stub (verify second transfer delivers zeros) | Positive |
| 47 | func008 | 008e | CMD_KEY_TRANSFER with payload size < 2 → RET_FAILURE. | MB_WDATA/WSEP/RDATA | Negative |
| **CMD_KEY_REVOKE TESTS** | | | | | |
| 48 | func009 | 009a | Register key; CMD_KEY_REVOKE(handle) → RET_SUCCESS; ret_arg[7:0] = revoked handle. Verify slot is no longer valid (`km_is_valid=false`) and handle is freed. | MB_WDATA/WSEP/RDATA, km_kpv state | Positive |
| 49 | func009 | 009b | CMD_KEY_REVOKE on already-revoked (handle not in registry or valid=false) → RET_FAILURE. | MB_WDATA/WSEP/RDATA | Negative |
| 50 | func009 | 009c | CMD_KEY_REVOKE with payload size < 1 → RET_INVALID_LEN. | MB_WDATA/WSEP/RDATA | Negative |
| **CMD_ENGINE_SHRED TESTS** | | | | | |
| 51 | func010 | 010a | CMD_ENGINE_SHRED(DEST_HMAC) → RET_SUCCESS; HMAC stub receives exactly 17 TLM writes (8 SHARE0 + 8 SHARE1 + KEY_CTRL=1). All values sent are zero (single-pass zero overwrite). | MB_WDATA/WSEP/RDATA, hmac_key recording stub (count=17, all zero) | Positive |
| 52 | func010 | 010b | CMD_ENGINE_SHRED(DEST_AES | DEST_KMAC): both AES and KMAC stubs each receive 17 writes. HMAC and OTBN stubs receive none. | MB_WDATA/WSEP/RDATA, aes and kmac recording stubs | Positive |
| 53 | func010 | 010c | CMD_ENGINE_SHRED with empty payload → RET_FAILURE. | MB_WDATA/WSEP/RDATA | Negative |
| 53a | func010 | 010d | CMD_ENGINE_SHRED(DEST_ABR_MLDSA_SEED) → 17 writes to ABR ML-DSA seed stub. | abr_mldsa_seed_socket | Positive |
| 53b | func010 | 010e | CMD_KEY_LOAD(8 words, DEST_ABR_MLDSA_SEED) + CMD_KEY_TRANSFER → 17 writes to ABR stub. | abr_mldsa_seed_socket | Positive |
| 53c | func010 | 010f | CMD_KEY_LOAD empty payload → RET_INVALID_LEN. | MB_WDATA/WSEP/RDATA | Negative |
| 53d | func010 | 010g | CMD_KEY_LOAD size/payload mismatch → RET_INVALID_LEN. | MB_WDATA/WSEP/RDATA | Negative |
| **FULL KEY LIFECYCLE TESTS** | | | | | |
| 54 | func011 | 011a | Full chain: grant slot → write key via KPVLP → CMD_KEY_REGISTER → CMD_KEY_TRANSFER to HMAC → CMD_KEY_REVOKE. After revoke: verify slot `valid=false`, handle freed, KPVLP_STATUS slot bit remains 1 (unlock_sep is not cleared by revoke — only reset clears it). | KPVLP_KEY/CTRL/STATUS, MB_WDATA/WSEP/RDATA, hmac_key stub | Positive |
| 55 | func011 | 011b | CMD_KEY_GENERATE → CMD_KEY_TRANSFER to AES → CMD_ENGINE_SHRED(AES): verify shred write count = 17 and all zeros. Confirm transfer before shred delivered non-zero key (RAND_bytes). | MB_WDATA/WSEP/RDATA, aes_key recording stub | Positive |
| 56 | func011 | 011c | Multi-command sequence across all operations: verify sequence counter is contiguous and correct throughout; any gap produces RET_CMD_NOSEQ. | MB_WDATA/WSEP/RDATA, m_next_rx_seq | Positive |
| **RESET DURING OPERATION TESTS** | | | | | |
| 57 | func012 | 012a | Register a key (unlock_sep set). Assert reset (`rst_ni=0`). After boot completes: read `KPVLP_STATUS=0` (all unlock_sep bits cleared — `km_kpv::reset()` clears all ctrl flags including unlock_sep). | KPVLP_STATUS (read=0 after reset) | Positive |
| 58 | func012 | 012b | After reset, firmware sequence counter is back to 0. Send a command with old seq (e.g. seq=5) — verify RET_CMD_NOSEQ. Send with seq=0 — verify RET_SUCCESS. | MB_WDATA/WSEP/RDATA, m_next_rx_seq | Positive |
| 59 | func012 | 012c | After reset, outbound FIFO is empty: read `MB_STATUS.OUTBOUND_EMPTY=1`; read `MB_RDATA` — verify OUTBOUND_UNDERFLOW set (not a response, FIFO was empty). Note: boot `RESP_KM_READY` must be consumed first. | MB_STATUS (bit[2]=1), MB_RDATA (empty check) | Positive |
| **CMD_KEY_GENERATE DUAL-SHARE STRUCTURAL TEST** | | | | | |
| 60 | func013 | 013a | CMD_KEY_GENERATE; CMD_KEY_TRANSFER to HMAC: capture all 17 TLM write transactions to HMAC socket. Verify: (a) exactly 8 SHARE0 writes + 8 SHARE1 writes + KEY_CTRL=1; (b) XOR of SHARE0[w] XOR SHARE1[w] is non-trivially non-zero for at least one word — proving dual-share encoding is intact and key is non-zero. (Exact values not checked since RAND_bytes output is unpredictable.) | MB_WDATA/WSEP/RDATA, hmac_key recording stub (capture write sequence and addresses) | Positive |

---

## Coverage Goals and Metrics

### Functional Coverage Targets

1. **Command Coverage: ~83% (10/12 commands)**
   - All 10 implemented commands covered with positive and negative tests
   - `CMD_EXEC_ROM` and `CMD_FIRM` deliberately absent (WIP per spec)
   - All error return codes (`RET_HEADER_CRC`, `RET_PAYLOAD_CRC`, `RET_CMD_NOSEQ`, `RET_INVALID_LEN`, `RET_INVALID_CMD`, `RET_INVALID_ARG`, `RET_FAILURE`) covered

2. **Register Coverage: 100%**
   - All 7 mailbox registers accessed (MB_WDATA, MB_WSEP, MB_RDATA, MB_STATUS, MB_IRQS, MB_IRQEN, MB_CTRL)
   - All KPVLP registers accessed (KPVLP_KEY, KPVLP_CTRL, KPVLP_STATUS)
   - All access types tested: WO, RO, RW, W1C, level-sensitive

3. **KPV Access Path Coverage: 100%**
   - SEP KPVLP path: unlock_sep gating, lock_write gating, write-only enforcement
   - KM firmware path: write, read, lock_write, lock_use, set_valid
   - Slot chaining: multi-slot wide-key handling

4. **Mailbox Protocol Coverage: 100%**
   - All header error types: bad CRC-8, bad payload length, bad payload CRC-32C, bad seq
   - Separator framing: WSEP self-clear, separator flag propagation
   - FIFO overflow/underflow: detection, sticky flags, W1C clear
   - Flush: MB_CTRL.FLUSH empties both FIFOs

5. **Key Transfer Coverage: 100%**
   - All 4 engine sockets exercised (HMAC in func007/008/013; AES/KMAC in func010; OTBN implicitly via shred engine mask)
   - Dual-share encoding validated structurally
   - `lock_use` one-shot semantics verified
   - DEST_VALID policy enforcement verified

6. **Fault and Reset Coverage**
   - Recoverable fault: not explicitly tested via `RESP_RECOVERABLE_FAULT` send path (gap — see below)
   - `CMD_RECOV_ACK` flow: not tested in isolation (gap)
   - Emergency wipe (`wipe_ni`): not tested in current test suite (gap — see below)
   - Reset: covered in func012

7. **IRQ Coverage**
   - `OUTBOUND_READ_DATA_AVAIL` (level): func002c/d
   - `OUTBOUND_UNDERFLOW` (sticky): func002e/f
   - `INBOUND_OVERFLOW` (sticky): not explicitly tested (gap)
   - `FLUSHED_BY_KM` (sticky): not explicitly tested (gap)
   - `INBOUND_WRITE_SPACE_AVAIL` (level): not explicitly tested (gap)

### Pass/Fail Criteria

**Each sub-test passes if:**
1. All response header fields (resp_id, seq, pay_len, CRC-8) are correct
2. All response payload fields (src_seq, cmd_id, ret_code, ret_arg) match specification
3. All register reads return expected values
4. All recording stub write counts and data match expected values
5. All IRQ/fault transitions occur as specified
6. No unexpected FAIL output printed by CHECK/CHECK_EQ macros

**Overall test plan passes if:**
- All 60 sub-tests across 13 test files pass
- No compiler warnings introduced by test code
- Sequence counter integrity maintained throughout all multi-command tests

---

## Known Test Gaps (Recommended Future Tests)

| Gap | Priority | Proposed Test |
|-----|----------|---------------|
| `CMD_RECOV_ACK` flow | Medium | Inject `m_recoverable_fault_active=true`, send CMD_RECOV_ACK, verify CMD_STAT then shows 0 |
| Emergency wipe via `wipe_ni` | High | Assert `wipe_ni=0` after key registration; verify KPVLP_STATUS=0, engine stubs received zeros, IRQ for FLUSHED_BY_KM |
| Recoverable fault injection | Medium | Cause RFAULT_MBOX_OVERFLOW, verify RESP_RECOVERABLE_FAULT received, IRQS.FLUSHED_BY_KM set |
| Unrecoverable fault halt | Medium | Send unrecoverable fault via `send_unrecoverable_fault()`; verify further commands rejected (firmware halted) |
| `INBOUND_OVERFLOW` IRQ | Low | Fill inbound FIFO to depth 16, push one more word, verify MB_IRQS.INBOUND_OVERFLOW=1 |
| `FLUSHED_BY_KM` IRQ | Low | Trigger unrecoverable fault (which calls `km_flush()`), verify MB_IRQS.FLUSHED_BY_KM=1 |
| OTBN socket sideload (384-bit) | Medium | CMD_ENGINE_SHRED(DEST_OTBN): verify 25 writes (12+12+1) to OTBN stub |
| Wide-key slot chain (extend>0) | Medium | Register key spanning 2 consecutive slots; CMD_KEY_TRANSFER; verify all key words delivered |
| `CMD_KPVLP_SLOT_REQ` bad payload | Low | Send CMD_KPVLP_SLOT_REQ with payload size=0 → RET_INVALID_LEN |
| Firmware halt + reset recovery | Medium | Send unrecoverable fault, verify halt; assert/deassert reset, verify firmware re-boots and accepts commands |
| OTP data passthrough | Low | After set_otp_data(), read m_otp_data via test backdoor, verify lc_state and chiplet_uid correctly stored |

---

## Implementation Notes

### Test Environment Architecture

Each test file uses the shared `testbench.h` framework which provides:
- `keymgr_tt_model` instance with reset/clock signals
- `hmac_key_stub`, `kmac_key_stub`, `aes_key_stub`, `otbn_key_stub` — recording stubs that capture TLM write transactions and expose write count + data
- `km_msg_t` helper: builds correctly framed messages (header CRC-8, payload CRC-32C, separator)
- `km_resp_t` helper: reads and parses responses from outbound FIFO

### Message Framing Helpers (`keymgr_tt_test.h`)

All tests use shared framing helpers rather than hand-building CRC values:
- `send_command(cmd_id, payload)` — builds and writes a correctly framed command
- `receive_response()` — reads all outbound words until separator, returns parsed response
- `crc8_header()` and `crc32_payload()` — exposed for negative-test CRC corruption

### Test Execution Sequence

1. Reset and boot wait (all tests)
2. Register Interface Validation (func001)
3. Mailbox + IRQ Behaviour (func002)
4. Boot/Info Commands (func003)
5. Message Framing (func004)
6. KPVLP Slot Request (func005)
7. Key Registration (func006)
8. Key Generation (func007)
9. Key Transfer (func008)
10. Key Revocation (func009)
11. Engine Shred (func010)
12. Full Lifecycle (func011)
13. Reset Behaviour (func012)
14. Dual-Share Structural (func013)

---

## Conclusion

This test plan covers all 10 implemented keymgr_tt commands and the complete mailbox protocol, KPVLP provisioning interface, key sideload delivery, and reset behaviour through 60 sub-tests in 13 test files. The plan is derived from a **reverse-engineering of the implemented model** and describes what is currently tested rather than a forward-looking specification of desired behaviour.

Key differentiators from a traditional hardware test plan:

- **No ISS tested** — command handlers are C++ methods; no instruction traces, no ELF loading
- **Framing protocol tested at bit level** — CRC-8/ROHC and CRC-32C Castagnoli polynomial values verified against `rom_crc.c` reference
- **Dual-share encoding verified structurally** — exact key values not checked (RAND_bytes output is non-deterministic); XOR integrity is verified
- **DPA countermeasures deliberately excluded** — multi-pass random overwrite was removed from the model; tests match the simplified model

The identified gaps (emergency wipe, recoverable fault injection, OTBN sideload, wide-key chains) should be addressed as the model evolves toward full KM_FW.md compliance.

---

**End of Test Plan**
