    # Standalone Model Coverage Summary

This document summarizes line and function coverage for all standalone models.  
Coverage results were obtained using the `Coverage` CMake build type and the `make coverage` target for each model.

---

## AES
- Lines: 92.7% (820 / 885)
- Functions: 94.4% (68 / 72)

## AON Timer
- Lines: 98.5% (649 / 659)
- Functions: 98.8% (81 / 82)

## CSRNG
- Lines: 91.7% (1066 / 1163)
- Functions: 96.5% (109 / 113)

## EDN
- Lines: 93.2% (670 / 719)
- Functions: 100% (54 / 54)

## EFUSE
- Lines: 91.8% (536 / 584)
- Functions: 75.4% (49 / 65)

## Entropy SRC
- Lines: 90.6% (746 / 823)
- Functions: 95.8% (91 / 95)

## GPIO
- Lines: 88.8% (300 / 338)
- Functions: 87.5% (28 / 32)

## HMAC
- Lines: 88.7% (867/977)
- Functions: 91.5% (65/71)

## Key Manager
- Lines: 88.5% (820 / 927)
- Functions: 89.4% (110 / 123)

## KMAC
- Lines: 91.8% (1685 / 1836)
- Functions: 97.1% (67 / 69)

## LC Ctrl
- Lines: 81.9% (86 / 105)
- Functions: 100.0% (12 / 12)

## Mailbox
- Lines: 98.0% (348 / 355)
- Functions: 100.0% (60 / 60)

## OTBN *(some algorithms are currently not covered by tests)*
- Lines: 59.6% (715 / 1200)
- Functions: 61.7% (82 / 133)

## Secure DMA
- Lines: 81.8% (1518 / 1855)
- Functions: 97.7% (128 / 131)

## SPI Controller
- Lines: 88.7% (771 / 869)
- Functions: 100.0% (64 / 64)

## SPI Flash
- Lines: 88.9% (505 / 568)
- Functions: 97.8% (226 / 231)

## UART
- Lines: 97.7% (506 / 518)
- Functions: 91.5% (43 / 47)

---

# Notes
- Coverage includes only peripheral sources under `inc` and `src`.
- Logging, utilities, and external libraries are excluded from coverage calculations.