    # Standalone Model Coverage Summary

This document summarizes line and function coverage for all standalone models.  
Coverage results were obtained using the `Coverage` CMake build type and the `make coverage` target for each model.

---

## AES
- Lines: 92.6% (818 / 883)
- Functions: 94.4% (68 / 72)

## AON Timer
- Lines: 98.5% (655 / 665)
- Functions: 98.8% (81 / 82)

## CSRNG
- Lines: 92.2% (1066 / 1156)
- Functions: 96.5% (109 / 113)

## EDN
- Lines: 93.1% (675 / 725)
- Functions: 100.0% (56 / 56)

## EFUSE
- Lines: 100.0% (589 / 589)
- Functions: 100.0% (67 / 67)

## EL2 PIC
- Lines: 93.3% (196 / 210)
- Functions: 100.0% (25 / 25)

## Entropy SRC
- Lines: 91.4% (752 / 823)
- Functions: 95.8% (91 / 95)

## HMAC
- Lines: 92.1% (898 / 975)
- Functions: 97.2% (69 / 71)

## Key Manager
- Lines: 98.0% (908 / 927)
- Functions: 97.6% (120 / 123)

## KMAC
- Lines: 80.9% (1497 / 1850)
- Functions: 97.1% (67 / 69)

## LC Ctrl
- Lines: 98.1% (103 / 105)
- Functions: 100.0% (12 / 12)

## Local Master Alias Remap Ctrl
- Lines: 98.9% (88 / 89)
- Functions: 100.0% (12 / 12)

## Mailbox
- Lines: 98.0% (348 / 355)
- Functions: 100.0% (60 / 60)

## OTBN *(otbn co-processor registers are not covered)*
- Lines: 91.9% (1193 / 1298)
- Functions: 96.6% (144 / 149)

## Secure DMA
- Lines: 92.0% (1455 / 1581)
- Functions: 97.7% (128 / 131)

## SEP CPU Ctrl
- Lines: 99.3% (428 / 431)
- Functions: 98.2% (54 / 55)

## SEP Filter Ctrl *(covers both inbound and outbound filter control instances)*
- Lines: 91.6% (174 / 190)
- Functions: 95.0% (19 / 20)

## SEP Memory
- Excluded by design (not run by `run_all_peripherals.sh`; no coverage build available).

## SEP Output Remap Ctrl *(covers both AP and STEE output remap instances)*
- Lines: 97.4% (76 / 78)
- Functions: 100.0% (10 / 10)

## SEP Reset Ctrl
- Lines: 100.0% (60 / 60)
- Functions: 100.0% (10 / 10)

## SEP Scratch Cold
- Lines: 90.1% (182 / 202)
- Functions: 100.0% (33 / 33)

## SEP Scratch Warm
- No `run_tests.sh` — stub, store-only peripheral with no behavioral logic to test.

## SPI Controller
- Lines: 95.6% (756 / 791)
- Functions: 100.0% (78 / 78)

## SPI Flash
- Lines: 97.3% (586 / 602)
- Functions: 99.2% (234 / 236)

---

# Notes
- Coverage includes only peripheral sources under `include` and `src`.
- Logging, utilities, and external libraries are excluded from coverage calculations.
- Peripherals are listed in the same order as they appear in `sep/peripherals/`.
