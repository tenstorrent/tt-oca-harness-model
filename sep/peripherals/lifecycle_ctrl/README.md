# lifecycle_ctrl

SystemC TLM2.0 model of the Lifecycle Controller. It maps the device lifecycle state
(TEST_DEV, PROD, PROD_END, RMA_SIP, RMA_CHIPLET) onto the 64-bit `FEAT_CTRL` feature
vector, and lets firmware demote into PROD_DBG through the two `DEMOTE` registers.

The block holds no lifecycle state of its own, and neither does the RTL:
`sep_lifecycle_ctrl.sv` is combinational on three inputs from the eFuse wrapper — the
shadow registers, `security_disable` and `secure_tm` — plus the two demote registers. The
model takes the same inputs through `set_inputs()` and recomputes `FEAT_CTRL` on every
call, which is what the platform does whenever the eFuse shadow changes. Only a standalone
testbench, which has no eFuse to ask, falls back on the CCI parameters.

Two details of the encoding are worth knowing before reading the code:

- `LC_STATE` arrives as the 8-bit differential code `{~raw, raw}`, not as a state. A pair
  that is not a legal encoding raises `lc_sigint_err` and forces `FEAT_CTRL` to zero. That
  fail-safe is why the state is encoded this way, so it is the response to a tampered or
  blank lifecycle state. `security_disable` still overrides it, as it does in the RTL.
- The demote state handed to the key manager is differentially encoded per rail, so an
  undemoted part reads `0xA` rather than `0x0`.

## Files

```
model/inc/lifecycle_ctrl_register.h   Register type definitions
model/inc/lifecycle_ctrl_base.h       TLM socket base
model/inc/lifecycle_ctrl.h            lifecycle_ctrl_model class declaration
model/src/lifecycle_ctrl_base.cpp     Base construction
model/src/lifecycle_ctrl.cpp          State read-back and b_transport handler

test/inc/testbench.h                  Testbench module header
test/inc/lifecycle_ctrl_basetest.h    Base test class
test/inc/lifecycle_ctrl_test.h        Test case declarations
test/src/testbench.cpp                sc_main entry
test/src/lifecycle_ctrl_basetest.cpp  Common test infrastructure
test/src/lifecycle_ctrl_test.cpp      Test orchestration
```

## Address

`0x10918000 – 0x10918017`  (0x18 bytes, SEP_LIFECYCLE_CTRL)

## Class

```cpp
class lifecycle_ctrl_model : public lifecycle_ctrl_base
```

## Registers

| Offset | Register | Access | Notes |
|---|---|---|---|
| `0x00` / `0x04` | `FEAT_CTRL_LO` / `_HI` | RO | The 64-bit feature vector: debug `[31:0]`, test `[47:32]`, func `[63:48]` |
| `0x08` / `0x0C` | `DEMOTE_1` / `_HI` | W1S | `demote[0]`, `lock[1]`, `rsvd[63:2]`; written by BL1 |
| `0x10` / `0x14` | `DEMOTE_2` / `_HI` | W1S | Same layout, written by BL2 |

Every `DEMOTE` field is `onwrite = woset`, so bits only ever set and a reset is the only
thing that clears them. `swwe = ~lock` covers the `demote` bit alone — `lock` and `rsvd`
stay writable once locked.

## Interface

| Port / Socket | Direction | Description |
|---|---|---|
| `target_socket` | target | TLM-2.0 32-bit register bus |

The block has no signal ports. Its inputs arrive as a bundle through
`set_inputs(const lc_inputs&)`, and `get_demote_state()` / `get_lc_sigint_err()` stand in
for its outputs.

## CCI Configuration

The parameters (`lc_state`, `sip_dis_lo/hi`, `sys_dis_lo/hi`, `security_disable`,
`secure_tm`) seed the input bundle at `end_of_elaboration`. On a platform the eFuse model
overrides all but one of them, so `vp/platform/sep/config/lifecycle_ctrl_vp.ini` documents
rather than controls them — change the fuse image in `efuse_vp.ini` instead.

`secure_tm` is the exception and stays live, because it is not a fuse value: it is the
`test_en` strap, which `sep_efuse_wrapper.sv` latches when fuse sense completes. It
defaults deasserted, which gates off the test section of `FEAT_CTRL`, as on a functional
part.

`lc_state` is the raw 4-bit value in configuration, and is encoded to the differential code
when it seeds the bundle, so configs stay readable.

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
./bin/lifecycle_ctrl_test
```

Tests 1-12 read `FEAT_CTRL` as the configuration leaves it, so each covers one state arm
and skips otherwise; the combos in `config/accellera_config.ini` select between them. Tests
13-19 drive the input bundle themselves and cover every arm, the `lc_sigint_err` fail-safe,
the demote encoding and the lock scope in a single run.

## Documentation

- [High-Level Design](docs/02_lifecycle_ctrl_HighLevel_Design.md)
- [Test Plan](docs/03_lifecycle_ctrl_Test_Plan.md)
