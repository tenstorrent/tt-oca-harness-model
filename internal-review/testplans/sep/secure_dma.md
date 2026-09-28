# secure_dma Test Audit and Plan

## Audited files

- Model: `include/secure_dma.h`, `secure_dma_base.h`, `secure_dma_register.h`, `src/secure_dma.cpp`, `src/secure_dma_base.cpp`.
- Tests: all files under `test/inc` and `test/src`, including FUNC-001 through FUNC-012, `test_dma_coverage.cpp`, and `coverage_tests.cpp`.
- Build/coverage: `CMakeLists.txt`, `run_tests.sh`.

## Behavior inventory

- 32-bit CSR target with interrupt, range/security, transfer, status/error, digest, handshake, and per-trigger source registers.
- Three DMA initiator sockets route OT, CTN, and system accesses based on source/destination ASIDs. Outgoing transfers set length, streaming width, byte enables, delay, and a SEP AXI extension source ID.
- Go/abort control, busy/config lock, validation, chunked transfer engine, fixed/increment/wrap addressing, visible address writeback, bus-error termination, reset interruption.
- Widths 1/2/4 bytes, alignment checks, subword extraction/replication, and byte-enable generation.
- Security range lock and source/destination policy across OT private, OT DMA-enabled, CTN, and system spaces.
- Sticky done/chunk/error status and independently gated interrupts; alert test.
- SHA-256/384/512 inline hashing, continuation/init, digest swap, digest-valid lifetime, abort/reset cleanup.
- Eleven handshake inputs, trigger enable, one chunk per trigger, optional interrupt-clear write, go persistence, chunk interrupt suppression, and bus-error halt.
- The target-side test memories add 10 ns and support one-shot OT read/write error injection.

## Existing tests matrix

### Genuine

- FUNC-001/002 and active FUNC-006-012 tests cover many register semantics, validation failures, interrupts, transfers, bus routing, data movement, chunking, abort/reset, security policies, hashes, and handshakes through actual sockets.
- Transfer tests generally compare destination bytes with separately initialized source bytes.
- Hash tests compare complete digests against reference computations and inspect validity/status.
- Bus error injection reaches real initiator socket response handling.
- Release/ASan/Coverage run the same binary and the final process exit reflects `m_tests_failed`.

### Partial

- Test harness target sockets ignore byte-enable semantics and treat unknown commands as successful no-ops (`test/src/secure_dma_test.cpp:257-373`), so generated protocol metadata is not independently validated.
- Reference SHA uses the same OpenSSL EVP implementation as the DUT (`test/src/test_dma_func_010.cpp:61-136`; model `src/secure_dma.cpp:3097-3299`). It checks integration and layout, but not an independent algorithm oracle.
- Test memory uses modulo address mapping (`test/src/secure_dma_test.cpp:277-286,312-321,347-356`), hiding alias/address decode defects.
- Many register helpers log response errors but return no status; callers can continue after failed CSR transport.
- Some test names overstate scope (for example “maximum transfer size 4GB” cannot actually transfer 4GB in the finite test memory).

### Coverage-only

- Production class declares `friend class testbench` twice (`include/secure_dma.h:35,116`).
- Both coverage files directly call private helpers and mutate private fields:
  - helper/hash/transaction calls and `m_hash_algorithm` poke (`test/src/coverage_tests.cpp:10-90`);
  - direct hash context/state inspection (`coverage_tests.cpp:150-172`);
  - direct helper calls, `m_hash_algorithm`, and current-address pokes (`test/src/test_dma_coverage.cpp:22-126,188-209`).
- FUNC-004 and FUNC-005 contain many helper-focused tests, but both entire suites are disabled in the actual runner (`test/src/testbench.cpp:215-220`).
- FUNC-003 invokes only two misalignment tests while 22 other cases are commented out (`test/src/test_dma_func_003.cpp:42-87`).
- FUNC-006 comments out most opcode, size, pre-validation, bus-response, status, interrupt, and accumulation cases (`test/src/test_dma_func_006.cpp:50-118`).
- Byte-enable helper tests exist but are commented out (`test/src/test_dma_func_003.cpp:63-66`); payload creation tests live in the wholly disabled FUNC-005 suite.

## Shortcut findings

- **Critical — compiled tests are not executed:** entire FUNC-004/005 suites and most FUNC-003/006 cases are disabled. Their presence inflates perceived test breadth without runtime verification.
- **High — friend/private coverage bypass:** `include/secure_dma.h:35,116`, `test/src/coverage_tests.cpp`, and `test/src/test_dma_coverage.cpp`. These tests do not prove socket-reachable behavior.
- **High — AXI extension is unverified:** model allocates/stamps `sep_axi_extension` (`src/secure_dma.cpp:1988-1998`), but no target captures/asserts it.
- **High — target does not honor byte enables:** despite extensive claims, test memories memcpy `len` bytes and ignore `byte_enable_ptr` (`test/src/secure_dma_test.cpp:290-338,360-373`).
- **High — outbound extension lifetime is suspicious and untested:** extension allocated with `new` and attached via `set_extension`, with no explicit clear/free in the transfer path (`src/secure_dma.cpp:1991-1997`); the handshake clear path similarly allocates one (`src/secure_dma.cpp:3452-3458`).
- **Medium — modulo memory aliases invalid addresses:** prevents realistic address-error and width-boundary testing.
- **Medium — same-library hash oracle:** reference and DUT both call OpenSSL EVP.
- **Medium — raw logger verbosity is forced to 3 in all builds** to execute logging-only expressions (`test/src/testbench.cpp:33-38,190-191`), indicating coverage depends on diagnostic code execution rather than semantics.
- No private-public preprocessor macro was found; friend access is the equivalent shortcut.

## Missing scenarios

- CSR TLM2: bad/ignore command, invalid/unaligned offset, null pointer, zero/short/oversized length, byte enable, streaming width, DMI/debug, delay/response assertions.
- DMA outbound payload: exact command/address/data/length/BE/streaming/DMI, SEP extension source/security fields, extension lifetime, accumulated delay, all response statuses on all three sockets.
- Address decode without modulo aliasing, first/last byte of each memory, 32-bit maximum/carry, 64-bit boundaries, crossing test-memory end.
- Partial final beat where total/chunk is not divisible by width; current loop subtracts width from unsigned counters and needs an explicit contract/test.
- Abort/reset at every point: before source read, between read/write, after write, at chunk boundary, during hash finalization, during interrupt-clear write.
- Simultaneous/multiple handshake triggers, held-high behavior, disabled trigger mixed with enabled trigger, trigger during reset/abort, repeated trigger after completion.
- Range/security exact base/limit inclusivity, wrap overflow, locked range reset, illegal unlock encodings.
- Interrupt combinations and pending event races; alert pulse/deassertion.
- Independent known SHA vectors, empty message if legal, odd chunking, continuation across distinct go operations, digest registers beyond active algorithm guaranteed zero/preserved policy.
- `transport_dbg` and DMI behavior for CSR and DMA paths.

## Proposed cases

1. **Enable-suite audit gate:** execute every declared FUNC test or delete/mark it intentionally unsupported. CI should compare declared runner lists against compiled test functions to prevent silent commenting-out.
2. **Raw CSR protocol suite:** use a helper returning status/delay/data unchanged; cover command, address, length, pointer, BE, streaming, debug, and DMI. Assert exact state preservation on rejection.
3. **Protocol-aware bus monitor:** replace memcpy-only stubs with monitors that capture every payload field, honor byte enables, verify `OTHERS_SOURCE_ID`, add deterministic delay, and return programmable statuses.
4. **Width/lane end-to-end:** for every 1-byte lane and 2-byte halfword, transfer distinct source words and assert destination enabled bytes changed while disabled bytes retain sentinels. This covers generation, payload, and target semantics together.
5. **Three-bus routing matrix:** source/destination combinations OT/CTN/SYS with unique memories and hard-coded expected socket counts; assert no other socket receives traffic.
6. **Malformed downstream responses:** inject ADDRESS, COMMAND, GENERIC, INCOMPLETE on source and destination for each bus; assert immediate halt, status/error/interrupt, no extra transactions, go/busy clear.
7. **Extension ownership test:** target records extension values during call; sanitizer verifies no leak; assert extension pointer lifecycle after payload destruction/reuse.
8. **Remainder-size boundaries:** totals/chunks `{1,2,3,4,5,7,8,9}` under widths `{1,2,4}`. Assert either explicit pre-transfer size error or exact partial-beat behavior with no counter underflow.
9. **Abort/reset phase injection:** monitored target blocks at selected beat, test issues abort/reset, releases target, and checks transaction drain contract, state, address writeback, hash cleanup, and interrupts.
10. **Handshake concurrency:** assert multiple inputs same delta, held-high and pulse inputs, disabled sources, clear-write failure, and post-completion triggers; check exact chunk count and source selection.
11. **Independent hash vectors:** use hard-coded NIST SHA-2 expected words for empty/short/multi-block messages, both swap modes, and continuation; do not compute expected values with EVP in test.
12. **Remove direct helper coverage:** exercise branches through CSR/go/socket stimulus. Any impossible defensive branch should be documented and narrowly excluded instead of reached via friend state pokes.

## Verdict

The repository contains a large amount of DMA test code, but the executed suite is materially smaller than it appears. Confidence is **medium for common full-word transfers and hashing**, and **low for subword TLM correctness, protocol metadata, extension ownership, and helper branches**. Disabled suites plus friend-based coverage are the dominant fake-coverage risks.
