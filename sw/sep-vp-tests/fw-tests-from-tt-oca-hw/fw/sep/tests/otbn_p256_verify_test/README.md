# OTBN P256 ECDSA Verification Test

This test validates the P256 ECDSA signature verification functionality of the OTBN (OpenTitan BigNumber) processor in the OCH SEP environment.

## Purpose

This test demonstrates and validates:

1. **OTBN Integration**: Proper integration of OTBN into the OCH SEP system
2. **Memory Loading**: Loading OTBN applications into IMEM and DMEM
3. **P256 Verification**: P256 ECDSA signature verification operation execution
4. **Result Validation**: Verification against OpenTitan assembly test vectors

## Test Structure

### Key Components

- `otbn_p256_verify_test.c`: Main test implementation
- `Makefile`: Build configuration following OCH test patterns
- OpenTitan assembly test vector values

### Test Flow

1. **Initialization**: Check OTBN accessibility and idle state
2. **App Loading**: Load P256 verification application into OTBN IMEM
3. **Data Setup**: Configure DMEM with message hash, signature components (r,s), and public key
4. **Execution**: Execute OTBN P256 verification operation
5. **Validation**: Verify result indicates valid signature

## Memory Layout

### Two Address Spaces

**Important**: There are two distinct address spaces to understand:

#### 1. EL2 CPU Address Space (SEP perspective)

When the EL2 CPU accesses OTBN, it uses these addresses:

- **OTBN Base**: `0x4000_0000` (from `sep_crypto_pkg.sv`)
- **OTBN DMEM Window**: Base + `0x00000` (3KB data memory window)
- **OTBN IMEM Window**: Base + `0x10000` (8KB instruction memory window)
- **OTBN Registers**: Base + `0x20000` (control/status registers)

#### 2. OTBN Internal Address Space

When OTBN code executes, it sees its own Harvard architecture:

- **OTBN IMEM**: Starts at `0x0` (OTBN's instruction space)
- **OTBN DMEM**: Starts at `0x0` (OTBN's data space)

### Address Translation

- EL2 CPU writes to `0x4001_0040` → OTBN sees data at DMEM address `0x40`
- OTBN code `sw x1, 64(x0)` → writes to OTBN DMEM offset `0x40` → EL2 reads from `0x4000_0040`

### DMEM Data Layout

```
Offset   Size    Description
0x0000   4B      Operation mode (MODE_SIGN = 0x31b)
0x0020   32B     Message digest (256-bit)
0x0040   32B     Signature R component (256-bit)
0x0060   32B     Signature S component (256-bit)
0x0080   40B     Private key share D0 (320-bit)
0x00A0   40B     Private key share D1 (320-bit)
```

## Test Vectors

The test uses validated test vectors from OpenTitan's assembly test suite:

**Source**: `opentitan/sw/otbn/crypto/tests/p256_ecdsa_verify_test.s`

- **Message**: SHA-256 digest from OpenTitan assembly test ("Hello OTBN.")
- **Signature**: Known (r,s) signature components from OpenTitan reference
- **Public Key**: P256 public key coordinates (x,y) from OpenTitan reference
- **Expected Result**: Verification should produce x_r that matches signature R component

## Implementation Notes

### Current Status

This implementation provides:

- Complete OTBN register access framework
- Memory loading and data management functions
- Test vector validation with known good values
- Functional stub OTBN application for testing infrastructure

### Future Enhancements

For a complete implementation:

1. **Full P256 App**: Replace stub with complete P256 OTBN application from OpenTitan
2. **Key Generation**: Add support for generating fresh private keys
3. **Multiple Test Vectors**: Expand test coverage with additional vectors
4. **Performance Testing**: Add instruction count and timing validation

## Building and Running

### Prerequisites

```bash
export OCH_ROOT=/proj_soc/user_dev/cgewehr/tt-och
export RV_ROOT=${OCH_ROOT}/vendor/chipsalliance/Cores-VeeR-EL2/upstream
```

### Build

```bash
cd ${OCH_ROOT}/dv/sep/tests/otbn_p256_sign
make all
```

### Run in OCH SEP Simulation

```bash
cd ${OCH_ROOT}/dv/sep
make verilator debug=1 PRELOAD=BACKDOOR_TCM TEST=otbn_p256_sign
```

## Integration with OpenTitan P256

To integrate the full OpenTitan P256 implementation:

1. **Build P256 App**: Use OpenTitan build system to compile `run_p256.s`
2. **Extract Binary**: Extract IMEM/DMEM sections using `otbn_build.py`
3. **Embed Data**: Replace `otbn_p256_app_imem` with actual P256 binary
4. **Update Constants**: Ensure DMEM layout matches OpenTitan symbols

### Example Integration

```c
// Replace the stub app with real P256 binary
extern const uint32_t run_p256_imem[];
extern const uint32_t run_p256_imem_size;
extern const uint32_t run_p256_dmem[];
extern const uint32_t run_p256_dmem_size;
```

## Debugging

### Common Issues

1. **Access Faults**: Verify OTBN base address matches hardware configuration
2. **Timeout Errors**: Check OTBN clock and reset configuration
3. **Signature Mismatches**: Ensure test vectors are used directly without format conversion

### Debug Output

The test provides detailed debug output:

- OTBN status register values
- Memory operation confirmations
- Step-by-step signature verification
- Clear pass/fail indication
