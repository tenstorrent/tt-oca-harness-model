# ROM Sanity Test

## Test Description

This test will test the basics of the ROM.  It will preload the ROM with a known pattern and then read back the pattern with the sep_cpu to verify it. It will also test what happens when a write is attempted to the ROM.

## Test Procedure

1. Preload the ROM with a known pattern
2. Read back the pattern from the ROM
3. Attempt to write to the ROM
4. Verify that the write is ignored
5. Read back the pattern from the ROM again
6. Verify that the pattern is still the same

## Notes about write behavior
writes should be silently accepted, but ignored.  The ROM should not perform the write. Please ensure this behavior is implemented in the ROM interface shim.

## Test Results

The test will report the results of the test.