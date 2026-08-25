# mailbox

SystemC TLM2.0 model of `axi_lite_mailbox_unit.sv`. The unit is a set of independent
mailbox channels behind one AXI-Lite slave port; each channel is a pair of FIFOs giving
bidirectional message passing between two agents, with threshold and error interrupts per
direction. SEP instantiates eight channels at `0x10A0_0000`.

Two classes matter, and the distinction is worth getting right before reading further:

- `mailbox_ip` — one channel. Two ports (outbound and inbound), each a target socket onto
  the same register block, and a FIFO in each direction.
- `mailbox_unit_t<N>` — the unit. Owns `N` channels, decodes the aperture, routes each
  transaction to the addressed channel and port, and exposes the interrupt vectors the
  subsystem wires up. `mailbox_unit` is the `N = 8` typedef SEP uses; SMC's 32-channel
  wrapper is the same template with a different `N`.

## Files

```
include/mailbox_register.h    Register type definitions
include/mailbox_base.h        TLM socket base
include/mailbox.h             mailbox_ip — one channel
include/mailbox_unit.h        mailbox_unit_t<N> — the multi-channel unit
src/mailbox_base.cpp          Base construction and register binding
src/mailbox.cpp               FIFO logic and b_transport handler

test/inc/testbench.h          Testbench module header
test/inc/mailbox_basetest.h   Base test class
test/inc/mailbox_test.h       Test case declarations
test/src/testbench.cpp        sc_main entry
test/src/mailbox_basetest.cpp Common test infrastructure
test/src/mailbox_test.cpp     Test orchestration
test/src/test_mailbox_func001.cpp  }
  ...                              } Functional test cases (7 total)
test/src/test_mailbox_func007.cpp  }
```

## Address map

SEP maps the unit at `0x10A0_0000 – 0x10A0_784F` (`0x7850` bytes, AXIL_MAILBOX). The aperture
is a flat array of `2 × N` register blocks of `0x800` bytes each, alternating outbound and
inbound:

```
channel m outbound block @ m * 0x1000 + 0x000
channel m inbound  block @ m * 0x1000 + 0x800
```

Within a block only offsets `0x00..0x4F` are mapped. The channel rejects the rest, and the
unit rejects anything past the aperture, both with an error response.

The mapped window stops at `0x784F` rather than at `0x8000` because it ends after the last
register of channel 7's inbound block — `7 × 0x1000 + 0x800 + 0x50`. The unit's own
`APERTURE_SIZE` is the full `0x8000`, so the trailing gap is unmapped at the platform bus and
never reaches the unit.

Outbound is port 0 and inbound is port 1, matching the RTL's
`slv_reqs_i({inbound_req, outbound_req})` ordering. The interrupt vectors follow the same
convention: `irq_o[0]` of channel *m* drives `outbound_irq_o[m]`, `irq_o[1]` drives
`inbound_irq_o[m]`.

## Interface

`mailbox_unit_t<N>`:

| Port / Socket | Direction | Description |
|---|---|---|
| `target_socket` | target | One AXI-Lite slave port for the whole unit |
| `outbound_irq_o[N]` | `sc_out<bool>` | Per-channel outbound interrupt |
| `inbound_irq_o[N]` | `sc_out<bool>` | Per-channel inbound interrupt |
| `rst_ni` | `sc_in<bool>` | Active-low reset, fanned out to every channel |
| `clk_i` | `sc_in<double>` | Abstract clock frequency, fanned out to every channel |

`mailbox_ip`:

| Port / Socket | Direction | Description |
|---|---|---|
| `socket0` | target | Outbound port register block |
| `socket1` | target | Inbound port register block |
| `irq_o[2]` | `sc_out<bool>` | Per-port interrupt; index 0 outbound, 1 inbound |
| `rst_ni` | `sc_in<bool>` | Active-low reset |
| `clk_i` | `sc_in<double>` | Abstract clock frequency |

Both ports are real bus targets. Neither is a loopback stub — an agent on the inbound side
reaches the same register block over the bus that the outbound side does, which is what
makes SEP → SMC delivery work.

## Behaviour worth knowing

**Error responses.** Unmapped offsets within a block, accesses past the aperture, and
writes to read-only registers all return an error response rather than being silently
absorbed.

**Sentinels.** Two reads return fixed values instead of erroring, matching RTL:
reading the write-data register returns `0xFEEDC0DE`, and reading from an empty FIFO
returns `0xFEEDDEAD` alongside an error response.

**FIFO depth** is 8 per direction, and the threshold saturates at depth − 1.

**Interrupts** are driven by a single SystemC process per port (`irq_driver`), which is the
only writer to `irq_o`. Everything else notifies it through an event, which is what keeps
the model free of SystemC multiple-driver errors.

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
./bin/mailbox_test
```

## Documentation

- [High-Level Design](docs/02_mailbox_HighLevel_Design.md)
- [Test Plan](docs/03_mailbox_Test_Plan.md)
- RTL comparison: `md_files/MAILBOX_RTL_VS_VP.md`
