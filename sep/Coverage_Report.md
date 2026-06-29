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
- Lines: 93.2% (670 / 719)
- Functions: 100.0% (54 / 54)

## EFUSE
- Lines: 91.8% (539 / 587)
- Functions: 75.4% (49 / 65)

## Entropy SRC
- Lines: 90.6% (746 / 823)
- Functions: 95.8% (91 / 95)

## GPIO
- Lines: 88.8% (300 / 338)
- Functions: 87.5% (28 / 32)

## HMAC
- Lines: 92.1% (898 / 975)
- Functions: 97.2% (69 / 71)

## Key Manager
- Lines: 98.0% (908 / 927)
- Functions: 97.6% (120 / 123)

## KMAC
- Lines: 92.2% (1450 / 1572)
- Functions: 97.1% (67 / 69)

## LC Ctrl
- Lines: 98.1% (103 / 105)
- Functions: 100.0% (12 / 12)

## Mailbox
- Lines: 98.0% (348 / 355)
- Functions: 100.0% (60 / 60)

## OTBN *(otbn co-processor registers are not covered)*
- Lines: 86.7% (1092 / 1260)
- Functions: 90.9% (130 / 143)

## Secure DMA
- Lines: 92.2% (1447 / 1569)
- Functions: 97.7% (128 / 131)

## SPI Controller
- Lines: 96.6% (757 / 784)
- Functions: 100.0% (78 / 78)

## SPI Flash
- Lines: 97.9% (557 / 569)
- Functions: 99.1% (229 / 231)

## UART
- Lines: 97.7% (506 / 518)
- Functions: 91.5% (43 / 47)

---

# Notes
- Coverage includes only peripheral sources under `inc` and `src`.
- Logging, utilities, and external libraries are excluded from coverage calculations.
