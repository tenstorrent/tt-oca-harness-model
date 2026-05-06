# AES Implementation Guide - Key Functional Changes from OpenTitan

1. The core cryptographic operations (supporting encryption and decryption for AES-128/192/256 in ECB, CBC, CFB, OFB, and CTR modes) leverage the OpenSSL library
2. Security hardening features (such as 1st-order masking, side-channel countermeasures, and fault injection protections) are implemented at a functional level to ensure full compatibility with the OpenTitan software drivers, while micro-architectural simulation details are omitted.
3. TL-UL transactions in OpenTitan are modeled as TLM transactions, and the model is not cycle-accurate; timing behavior and delays are not considered.
4. Functional delays, such as the initial latency for key expansion in ECB/CBC decryption mode, are modeled using temporal decoupling (quantum keeper).