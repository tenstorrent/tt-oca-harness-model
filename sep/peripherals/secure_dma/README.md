# secure_dma

SystemC TLM2.0 model of the OpenTitan Secure DMA as SEP integrates it (`sep_dma_wrap.sv`).
Transfers data between memory regions under firmware control while enforcing the address
range, lock and ASID checks the hardware applies before a transfer starts.

## Files

```
include/secure_dma_base.h          Register map and TLM socket base
include/secure_dma_register.h      Register type definitions
include/secure_dma.h               secure_dma_model class declaration
src/secure_dma_base.cpp            Base construction and register binding
src/secure_dma.cpp                 Transfer engine and b_transport handler

test/inc/testbench.h               Testbench module header
test/inc/secure_dma_basetest.h     Base test class
test/inc/secure_dma_test.h         Test case declarations
test/src/testbench.cpp             sc_main entry
test/src/secure_dma_basetest.cpp   Common test infrastructure
test/src/secure_dma_test.cpp       Test orchestration
test/src/test_dma_func_001.cpp     }
  ...                              } Functional test cases (12 total)
test/src/test_dma_func_012.cpp     }
```

## Address

`0x10800000 – 0x1080014F`  (0x150 bytes, SECURE_DMA)

## Class

```cpp
class secure_dma_model : public secure_dma_base
```

## Interface

| Port / Socket | Direction | Description |
|---|---|---|
| `target_socket` | target | TLM-2.0 32-bit register bus |
| `ot_initiator_socket` | initiator | 32-bit OpenTitan-internal bus (ASID `0x7`) |
| `ctn_initiator_socket` | initiator | 32-bit CTN bus (ASID `0xA`) — tied off in SEP |
| `sys_initiator_socket` | initiator | 64-bit system bus (ASID `0x9`) — tied off in SEP |
| `dma_done_intr` | `sc_out<bool>` | Transfer complete (PIC 9) |
| `dma_chunk_done_intr` | `sc_out<bool>` | Chunk complete (PIC 10) |
| `dma_error_intr` | `sc_out<bool>` | Transfer error (PIC 11) |
| `alert_fatal_fault` | `sc_out<bool>` | Fatal fault alert |
| `lsio_trigger[11]` | `sc_in<bool>` | Hardware-handshake triggers |
| `clk_i` | `sc_in<sc_time>` | Clock period |
| `rst_ni` | `sc_in<bool>` | Active-low reset |

## Behaviour notes

**Only the OT bus works on a platform.** `sep_dma_wrap.sv` grounds `sys_i` and stubs the CTN
leg with `d_valid` held low, so in silicon a transfer on either of those ASIDs is accepted
and then never completes — the DMA stalls forever. The SEP platform binds both sockets to
`dead_manager_port_stub`, which errors instead of hanging, because `b_transport` cannot
express "never responds" without wedging the SystemC kernel. The transfer still fails; it
fails visibly rather than by deadlock. Standalone, the unit testbench binds its own memories
to all three, so FUNC-009's CTN and SYS cases exercise the datapath.

**Address remapping on the OT path.** `sep_dma_wrap.sv` instantiates an `axi_window_remap`
on the DMA's OT initiator, so addresses in `[SEP_LOCAL_BASE_ADDR, +0x3000_0000)` become
`addr - SEP_LOCAL_BASE_ADDR + 0x1000_0000`. The platform reproduces this with
`dma_alias_remap_adapter` spliced into the same path. Without it the DMA's view of the
address map differs from the CPU's.

**Locks use MuBi4, not a flag.** `CFG_REGWEN` and `RANGE_REGWEN` are multi-bit encoded:
`0x6` is unlocked and `0x9` is locked, and *any* value that is not `0x6` locks. Reading a
locked register returns `0x9`, not `0x0` — the most common source of surprise when writing
tests against this block.

**`RANGE_VALID` is checked on every transfer**, not only on transfers that cross a range
boundary. A transfer configured without it fails with `ERROR_CODE.range_valid_error`, which
matches the `DmaAddrSetup` check in RTL.

**Address registers are written back only in fixed mode.** In increment or wrap mode the
CSRs keep their programmed value while the transfer advances internally; in fixed mode
`chunk_data_size` is added at chunk end. This mirrors the RTL's `update_src_addr_reg` /
`update_dst_addr_reg` conditions, and it means a test that expects `SRC_ADDR_LO` to track an
incrementing transfer is testing the wrong thing.

**A fixed-address FIFO endpoint needs `SRC_CONFIG = 0x2`** (wrap set, increment clear), not
`0x0`. This is what the harness firmware uses for hardware-handshake sources.

**Bus errors halt the transfer.** That includes a failure of the interrupt-clearing write
during hardware handshake: `ERROR_CODE.bus_error` is set, `STATUS.error` is raised, `busy`
and `CONTROL.go` are cleared. The RTL takes the same path into `DmaError`.

**Hardware-handshake RX drain (peripheral-to-memory).** When
`CONTROL.hardware_handshake_enable` (bit 4) is set, arming the transfer (`CONTROL.go`) does
**not** move any data immediately. The engine waits for the first RX-watermark trigger on
`lsio_trigger` before draining the first chunk, then drains one chunk per trigger until
`TOTAL_DATA_SIZE` is reached. This matches hardware: the DMA is typically armed before the
peripheral read is issued, so draining on arm would read an empty peripheral FIFO.
Non-handshake (memory-to-memory) transfers still start immediately on `go`.

## Building and Testing

```bash
# Using run_tests.sh (recommended)
./run_tests.sh              # Release build + run
./run_tests.sh --debug      # Debug build
./run_tests.sh --asan       # AddressSanitizer
./run_tests.sh --coverage   # lcov coverage report
./run_tests.sh --ctest      # via CTest (verbose)
./run_tests.sh --clean      # clean build dir first
./run_tests.sh --cppcheck   # for static analysis

# Manual CMake
mkdir -p build/debug && cd build/debug
cmake ../.. -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
./bin/secure_dma_test
```

## Documentation

- [High-Level Design](docs/02_secure_dma_HighLevel_Design.md)
- [Test Plan](docs/03_secure_dma_Test_Plan.md)
- RTL comparison: `md_files/SECURE_DMA_RTL_VS_VP.md`
