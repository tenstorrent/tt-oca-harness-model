# SEP VP Software Tests

This directory contains bare-metal software tests for the SEP Virtual Platform peripherals.

## Available Tests

- `sep-gpio-test/` - GPIO peripheral test
- `sep-hmac-test/` - HMAC peripheral test
- `hmac-dv-test/` - HMAC design verification test
- `sep-otbn-test/` - OTBN (OpenTitan Big Number) accelerator test
- `sep-spi-test/` - SPI controller test
- `sep-i2c-test/` - I2C controller test
- `otbn-dv-test/` - OTBN design verification test
- `otbn-dv-p256-verify-test/` - OTBN P256 ECDSA verification test
- `uart_16550_test/` - UART 16550 test
- `rom_test/` - ROM test

## Building and Running Tests

### Prerequisites

- RISC-V GNU toolchain (`riscv32-unknown-elf-gcc`) in your `$PATH`
- SEP VP built at `../../vp/build/bin/sep-vp`

### Build a Test

Navigate to any test directory and run `make`:

```bash
cd sep-gpio-test
make
```

This produces an ELF binary (e.g., `sep_gpio_test`) that can run on the VP.

### Run the Test

```bash
make sim
```

This executes the test on the SEP VP with syscall interception enabled.

### Generate Disassembly

```bash
make dump
```

This creates `.dump` (disassembly) and `.sym` (symbol table) files for analysis.

### Clean Build Artifacts

```bash
make clean
```

## Debugging Software with GDB

The SEP VP supports debugging RISC-V software using GDB through a remote debugging interface. This requires two terminal windows.

### Prerequisites

Install the RISC-V GDB debugger:

```bash
sudo apt-get install gdb-multiarch
```

### Debug Workflow

**Step 1: Start VP in Debug Mode (Terminal 1)**

Navigate to a test directory and start the VP as a GDB server:

```bash
cd sep-gpio-test
make debug
```

You should see:
```
Starting sep-vp in debug mode on port 5005...
Connect with GDB using: make gdb
```

The VP will wait for a debugger connection before executing any code.

**Step 2: Connect GDB Client (Terminal 2)**

In a **new terminal**, navigate to the same test directory:

```bash
cd sep-gpio-test
make gdb
```

This launches `gdb-multiarch` and automatically connects to the VP on port 5005. You should see the GDB prompt:

```
(gdb)
```

**Step 3: Debug with GDB Commands**

Once connected, use standard GDB commands:

```gdb
(gdb) break main              # Set breakpoint at main function
(gdb) continue                # Run until breakpoint
(gdb) step                    # Step into functions (source level)
(gdb) stepi                   # Step one instruction
(gdb) next                    # Step over functions
(gdb) nexti                   # Next instruction
(gdb) print variable_name     # Inspect variables
(gdb) print/x $pc             # Print program counter in hex
(gdb) info registers          # View all RISC-V registers
(gdb) info registers a0 a1    # View specific registers (e.g., a0, a1)
(gdb) x/10i $pc               # Disassemble 10 instructions at PC
(gdb) x/4xw 0x10000000        # Examine 4 words of memory in hex
(gdb) backtrace               # View call stack
(gdb) list                    # Show source code around current location
(gdb) quit                    # Exit GDB (will terminate VP)
```

### Using a Custom Debug Port

If port 5005 is already in use, specify a different port:

```bash
# Terminal 1
DEBUG_PORT=5006 make debug

# Terminal 2
DEBUG_PORT=5006 make gdb
```

### Common Debugging Scenarios

**Setting Breakpoints:**

```gdb
(gdb) break main                    # Break at function name
(gdb) break main.c:42               # Break at source line
(gdb) break *0x10000100             # Break at specific address
(gdb) info breakpoints              # List all breakpoints
(gdb) delete 1                      # Delete breakpoint 1
```

**Examining Memory-Mapped Peripherals:**

```gdb
# Example: Read GPIO output register at address 0x40010000
(gdb) x/1xw 0x40010000

# Example: Watch for writes to UART data register
(gdb) watch *0x40030000
```

**Inspecting RISC-V Registers:**

```gdb
(gdb) info registers              # All registers
(gdb) print/x $pc                 # Program counter
(gdb) print/x $sp                 # Stack pointer
(gdb) print/x $ra                 # Return address
(gdb) print/x $a0                 # Function argument 0 / return value
```

**Single-Stepping Assembly:**

```gdb
(gdb) stepi                       # Execute one instruction
(gdb) x/10i $pc                   # Show next 10 instructions
(gdb) display/i $pc               # Auto-display current instruction
```

### Debugging Tips

1. **Compile with Debug Symbols**: The Makefile.common already includes `-g` flag by default
2. **Use Disassembly Files**: Run `make dump` to generate `.dump` files for reference alongside GDB
3. **Set Breakpoints Early**: Set breakpoints before running `continue` to catch execution at key points
4. **Watch Peripheral Registers**: Use `watch` command to break when memory-mapped registers change
5. **Save GDB Commands**: Create a `.gdbinit` file in the test directory with frequently-used commands
6. **RISC-V Calling Convention**:
   - `a0-a7` (x10-x17): Function arguments
   - `a0-a1`: Function return values
   - `ra` (x1): Return address
   - `sp` (x2): Stack pointer

### Exiting Debug Session

- **In GDB**: Type `quit` or press `Ctrl+D` to exit GDB (this will also terminate the VP)
- **In VP Terminal**: Press `Ctrl+C` to stop the VP if GDB disconnects

### Troubleshooting

**"Connection refused" when running `make gdb`:**
- Ensure `make debug` is running in Terminal 1
- Check that the port is correct (default: 5005)
- Verify no firewall is blocking localhost connections

**GDB connects but code doesn't match:**
- Rebuild the test with `make clean && make`
- Ensure you're debugging the correct binary

**VP crashes or hangs:**
- Check for memory access violations (invalid addresses)
- Verify peripheral registers are accessed correctly
- Use `info registers` to check PC hasn't jumped to invalid location

## Test Structure

Each test follows a common structure:

```
test-name/
├── Makefile          # Defines SRCS, OBJS, TARGET; includes ../Makefile.common
├── start.S           # Assembly startup code (stack setup, jump to main)
├── main.c            # Main test logic
├── link.ld           # Linker script (memory layout)
├── uart_io.c         # UART I/O functions (printf-like)
└── uart_io.h         # UART I/O header
```

### Key Files

- **start.S**: Minimal boot code that sets up the stack pointer and jumps to `main`
- **link.ld**: Defines memory regions (ROM, RAM, peripherals) and section placement
- **uart_io.c**: Provides `uart_print()`, `uart_println()`, `uart_print_hex()` for debugging output
- **Makefile.common**: Shared build configuration (toolchain, flags, targets)

## Makefile Targets Reference

| Target | Description |
|--------|-------------|
| `make` or `make all` | Build the test binary |
| `make sim` | Run test on SEP VP |
| `make debug` | Start VP in GDB server mode (port 5005) |
| `make gdb` | Connect GDB client to running VP |
| `make dump` | Generate disassembly (`.dump`) and symbol table (`.sym`) |
| `make clean` | Remove build artifacts |

## Environment Variables

- **RISCV_PREFIX**: Toolchain prefix (default: `riscv32-unknown-elf-`)
- **DEBUG_PORT**: GDB server port (default: `5005`)
- **EXTRA_CFLAGS**: Additional compiler flags

Example:
```bash
EXTRA_CFLAGS="-DDEBUG_VERBOSE" make
```

## Writing New Tests

To create a new test:

1. Create a new directory under `sep-vp-tests/`
2. Copy `start.S`, `link.ld`, `uart_io.c`, `uart_io.h` from an existing test
3. Write your `main.c` with test logic
4. Create a `Makefile`:
   ```makefile
   SRCS = start.S main.c uart_io.c
   OBJS = $(SRCS:.c=.o)
   OBJS := $(OBJS:.S=.o)
   TARGET = my_test_name

   include ../Makefile.common
   ```
5. Build and run: `make && make sim`

## Additional Resources

- **Main Project README**: `../../../README.md`
- **RISC-V VP Documentation**: `../../vp/README.md`
- **RISC-V ISA Manual**: https://riscv.org/technical/specifications/
- **GDB Manual**: https://sourceware.org/gdb/current/onlinedocs/gdb/
