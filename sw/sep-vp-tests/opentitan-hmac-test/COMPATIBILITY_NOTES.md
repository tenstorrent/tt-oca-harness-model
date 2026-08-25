# OpenTitan HMAC Test - SEP Platform Compatibility Issues

## Problem Summary

You successfully ported the OpenTitan HMAC smoketest to SEP platform, but it hangs after printing the first two log messages. The same issue affects your existing `sep-hmac-test`.

## Root Cause

**Your SEP platform HMAC peripheral has register differences from the OpenTitan specification:**

### 1. STATUS Register Bit Position Mismatch

**OpenTitan Specification:**
```
Bit 0: fifo_empty
Bit 1: fifo_full  
Bits 4-8: fifo_depth
```

**SEP Platform (your implementation):**
```
Bit 0: hmac_idle  ← Extra bit not in OpenTitan
Bit 1: fifo_empty ← Different position!
Bit 2: fifo_full  ← Different position!
Bits 4-8: fifo_depth
```

### 2. FIFO Depth Counter Not Updating

The `fifo_depth` field (bits 4-8) in your HMAC STATUS register **does not decrement** as the hardware processes data from the FIFO. This causes any code that polls `fifo_depth` waiting for it to reach 0 to hang forever.

## Changes Made

### Fixed STATUS Register Bit Definitions

**File:** `opentitan-compat/dif_hmac_sep.h`

```c
// STATUS register bit fields (SEP Platform - different from OpenTitan!)
#define HMAC_STATUS_HMAC_IDLE_BIT      0  // SEP specific
#define HMAC_STATUS_FIFO_EMPTY_BIT     1  // SEP: bit 1, OpenTitan: bit 0
#define HMAC_STATUS_FIFO_FULL_BIT      2  // SEP: bit 2, OpenTitan: bit 1
#define HMAC_STATUS_FIFO_DEPTH_OFFSET  4  // Same in both
```

### Changed FIFO Polling Strategy

**File:** `opentitan-compat/hmac_testutils.h`

**Before (doesn't work on SEP):**
```c
// Poll fifo_depth counter until it reaches 0
do {
    dif_hmac_fifo_count_entries(hmac, &num_entries);
} while (num_entries > 0);  // Hangs forever!
```

**After (works on SEP):**
```c
// Poll fifo_empty status bit instead
do {
    uint32_t status = read_status_register();
    uint32_t fifo_empty = (status >> HMAC_STATUS_FIFO_EMPTY_BIT) & 0x1;
    if (fifo_empty) break;
} while (--timeout > 0);
```

## Next Steps

### Test the Fix

```bash
cd sw/sep-vp-tests/opentitan-hmac-test
make sim
```

You should now see debug output showing FIFO status. If it still hangs, your HMAC peripheral's `fifo_empty` bit is not being set correctly.

## Hardware Requirements

For the test to work, your HMAC peripheral must:
1. ✅ Respond to register reads/writes at 0x40026000
2. ✅ Accept data into FIFO
3. ⚠️ **Update STATUS.fifo_empty bit** when FIFO is processed
4. ⚠️ **Set INTR_STATE.hmac_done bit** when complete
5. ⚠️ **Provide digest** in DIGEST registers

Items 3-5 need verification in your VP.
