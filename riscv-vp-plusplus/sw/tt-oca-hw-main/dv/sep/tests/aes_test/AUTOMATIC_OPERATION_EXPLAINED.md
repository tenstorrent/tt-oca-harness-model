# AES Automatic Operation - How It Works

## Overview

The OpenTitan AES module has a sophisticated automatic operation mode that tracks DATA_IN writes and DATA_OUT reads **in hardware**. This eliminates the need for manual triggering and prevents data loss.

## How AES Knows Output Has Been Read

The AES module uses **hardware registers with read-enable (hwre) signals** to track when software reads the DATA_OUT registers.

### Hardware Tracking Mechanism

From the AES register specification (`aes.hjson`):

```hjson
{ multireg: {
  name: "DATA_OUT",
  swaccess: "ro",      // Software Read-Only
  hwaccess: "hrw",     // Hardware Read-Write
  hwext:    "true",    // Hardware external signals
  hwre:     "true",    // Hardware Read Enable - KEY FEATURE!
  fields: [...]
}
```

**The `hwre: "true"` parameter** means:
- Each DATA_OUT register has a hardware read-enable output signal
- This signal pulses HIGH whenever software reads that register
- The AES control logic monitors these signals
- When all 4 DATA_OUT registers have been read, INPUT_READY goes HIGH

## Automatic Operation State Machine

```
Initial State: IDLE=1, INPUT_READY=0, OUTPUT_VALID=0
    ↓
(1) Software writes all 4 DATA_IN registers
    ↓
[AUTOMATIC] AES detects last DATA_IN write → starts encryption
    ↓
State: IDLE=0, INPUT_READY=0, OUTPUT_VALID=0
    ↓
(2) Encryption completes
    ↓
[AUTOMATIC] AES sets OUTPUT_VALID=1
    ↓
State: IDLE=1, INPUT_READY=0, OUTPUT_VALID=1
    ↓
(3) Software reads all 4 DATA_OUT registers
    ↓
[AUTOMATIC] AES detects all 4 reads complete → sets INPUT_READY=1
    ↓
State: IDLE=1, INPUT_READY=1, OUTPUT_VALID=1 (ready for next block)
    ↓
(4) Software writes next block's DATA_IN
    ↓
[AUTOMATIC] Process repeats from step 1
```

## Key Hardware Signals

### STATUS Register Fields

| Field | Description | Auto-Mode Behavior |
|-------|-------------|-------------------|
| **IDLE** | 0=busy, 1=idle | Goes to 0 when encryption starts, back to 1 when done |
| **INPUT_READY** | Ready for new input | Goes to 1 after all DATA_OUT reads complete |
| **OUTPUT_VALID** | Valid output available | Goes to 1 when encryption completes |
| **STALL** | Stalled waiting for read | Goes to 1 if trying to start but output not read |

### Internal Tracking Logic (simplified)

```systemverilog
// Inside aes_reg_top.sv (register module)
logic [3:0] data_in_qe;   // Write enables for DATA_IN
logic [3:0] data_out_re;  // Read enables for DATA_OUT (hwre signals!)

// Track when all DATA_IN written
assign all_data_in_written = &data_in_qe_sticky;

// Track when all DATA_OUT read
assign all_data_out_read = &data_out_re_sticky;

// Control logic responds to these signals
if (all_data_in_written && input_ready) begin
    start_encryption <= 1'b1;  // Auto-start!
end

if (all_data_out_read && output_valid) begin
    input_ready <= 1'b1;  // Ready for next block!
    clear_data_out_read_tracking <= 1'b1;
end
```

## Multi-Block CBC Operation Flow

### Block 0:
```
Write IV → Wait INPUT_READY → Write DATA_IN[0:3]
  → [AUTO START] → Wait OUTPUT_VALID
  → Read DATA_OUT[0:3] → [AUTO SET INPUT_READY]
```

### Block 1:
```
(INPUT_READY already high from Block 0 reads)
Write DATA_IN[0:3] (plaintext XOR previous ciphertext happens inside AES)
  → [AUTO START] → Wait OUTPUT_VALID
  → Read DATA_OUT[0:3] → [AUTO SET INPUT_READY]
```

### Block 2:
```
(INPUT_READY already high from Block 1 reads)
Write DATA_IN[0:3]
  → [AUTO START] → Wait OUTPUT_VALID
  → Read DATA_OUT[0:3] → [DONE]
```

## Stall Protection

If software tries to write a new block before reading the previous output:

```
State: OUTPUT_VALID=1, but DATA_OUT not fully read
Software writes DATA_IN[0:3]
  ↓
[AUTOMATIC] AES detects: "output not read yet"
  ↓
AES sets STALL=1 and does NOT start encryption
  ↓
Software must read all DATA_OUT registers first
  ↓
Then AES will process the waiting input
```

This prevents data loss - the previous output is never overwritten.

## Why This Is Efficient

1. **No polling between operations**: Just write input, wait for valid, read output
2. **Hardware handshaking**: No race conditions or lost data
3. **Pipeline-friendly**: Can prepare next block while reading current output
4. **Minimal SW overhead**: Only check OUTPUT_VALID, no complex state tracking

## Test Code Example

```c
// Setup phase (once)
Configure_AES_CBC_mode();
Write_Key();
Write_IV();

// Per-block operation (repeated for each block)
for (each block) {
    Wait_for_INPUT_READY();        // HW says "ready for input"

    Write_DATA_IN[0:3](plaintext); // Write all 4 registers
    // [AUTOMATIC]: AES starts immediately after 4th write!

    Wait_for_OUTPUT_VALID();       // HW says "output ready"

    Read_DATA_OUT[0:3](ciphertext); // Read all 4 registers
    // [AUTOMATIC]: INPUT_READY goes high after 4th read!

    // Now ready for next block immediately!
}
```

## CBC Mode Chaining

In CBC mode, the AES hardware automatically:
1. Saves the ciphertext output internally
2. XORs it with the next plaintext input before encryption
3. This happens entirely in hardware - software just provides sequential blocks

```
Block 0: PT[0] XOR IV        → Encrypt → CT[0]
Block 1: PT[1] XOR CT[0]     → Encrypt → CT[1]  (CT[0] saved in HW)
Block 2: PT[2] XOR CT[1]     → Encrypt → CT[2]  (CT[1] saved in HW)
```

## Manual Mode (for comparison)

When `MANUAL_OPERATION=1`:
- Software must explicitly trigger each operation via TRIGGER.START
- AES does NOT auto-start on DATA_IN writes
- AES does NOT track DATA_OUT reads
- Can overwrite output before it's read (data loss possible)
- Useful for debugging or special cases

## Summary

The AES "knows" output has been read through:
1. **Hardware read-enable signals** (`hwre` in register spec)
2. **Per-register tracking** of which DATA_OUT registers have been read
3. **Automatic INPUT_READY assertion** when all reads complete
4. **Stall protection** to prevent starting if output not read

This is all implemented in the `aes_reg_top.sv` register block and `aes_control.sv` control FSM - completely transparent to software!
