# uart_16550

SystemC TLM2.0 model of a 16550-compatible UART.  Implements the standard 16550 register set (THR, RHR, IER, IIR, FCR, LCR, MCR, LSR, MSR, DLL, DLH).  Supports an optional terminal UI for interactive console output during simulation.

## Files

```
model/inc/uart_base.h                  Register map and TLM socket base (UART_base)
model/inc/uart.h                       UART_IP class declaration
model/inc/uart_with_terminal.h         UART variant with terminal UI attached
model/inc/uart_terminal_ui.h           Optional terminal UI module
model/inc/terminal_if.h                Terminal interface
model/inc/uart_register.h              Register field definitions
model/inc/uart_log_adapter.h           Logging adapter
model/inc/thread_safe_queue_channel.h  Thread-safe TX/RX queue channel
model/src/uart_base.cpp                Base construction and register binding
model/src/uart.cpp                     TX/RX logic and b_transport handler
model/src/uart_terminal_ui.cpp         Terminal UI implementation
model/src/thread_safe_queue_channel.cpp  Queue channel implementation
model/src/uart_terminal_client.py      Python terminal client for interactive use

tests/inc/uart_basetest.h              Base test class
tests/inc/uart_test.h                  UART test case declarations
tests/inc/terminal_test.h             Terminal test case declarations
tests/src/top.cpp                      sc_main entry
tests/src/uart_basetest.cpp            Common test infrastructure
tests/src/uart_test.cpp                UART register and TX/RX test cases
tests/src/terminal_test.cpp            Terminal UI test cases
```

## Address

`0x44000000 – 0x4400FFFF`

## Class

```cpp
class UART_IP : public UART_base, public terminal_if
```

## Interface

| Port / Socket | Direction | Description |
|---|---|---|
| `target_socket` | target | TLM-2.0 32-bit register bus |
| `intr_o` | `sc_out<bool>` | UART interrupt output |
| `rst_ni` | `sc_in<bool>` | Active-low reset |

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
./bin/uart_test         # UART register and TX/RX tests
./bin/terminal_test     # terminal UI tests
```
