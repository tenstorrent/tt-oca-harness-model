# Standalone Model Coverage Summary

This document summarizes line and function coverage for all standalone models.
Coverage results were obtained using the `Coverage` CMake build type and the `make coverage` target for each model.

---

## UART 16550
- **Line coverage:** 97.7% (505 of 517)
- **Function coverage:** 91.5% (43 of 47)

---

## I2C
- **Line coverage:** 93.1% (1262 of 1356)
- **Function coverage:** 90.8% (108 of 119)

---

## GPIO
- **Line coverage:** 89.5% (290 of 324)
- **Function coverage:** 93.3% (28 of 30)

---

## HMAC
- **Line coverage:** 90.6% (841 of 928)
- **Function coverage:** 94.2% (65 of 69)

---

## OTBN
- **Line coverage:** 67.1% (706 of 1052)
- **Function coverage:** 70.6% (84 of 119)

---

## SPI Controller
- **Line coverage:** 89.7% (783 of 873)
- **Function coverage:** 100.0% (64 of 64)

---

# Notes
- Coverage includes only model sources (`model/inc`, `model/src`).
- Logging, utilities, and external libraries are excluded from coverage calculations.

