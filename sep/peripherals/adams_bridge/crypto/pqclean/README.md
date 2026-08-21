# PQClean snapshot (ML-DSA-87 + ML-KEM-1024)

Vendored from [PQClean](https://github.com/PQClean/PQClean) (CC0 / public
domain) for the Adams Bridge FIPS 204 / FIPS 203 backend.

- `ml-dsa-87/` — `crypto_sign/ml-dsa-87/clean`
- `ml-kem-1024/` — `crypto_kem/ml-kem-1024/clean`
- `common/` — shared FIPS 202 (`fips202.c`) plus `randombytes.h`

Do not edit these files except for the small internal-API additions in
`ml-dsa-87/sign.c` / `sign.h` (seeded keygen, external-mu sign/verify).
Upstream randomized `randombytes()` wrappers stay unused; the model supplies
a stub so the objects still link.

Snapshot date: 2026-08-20 (PQClean `master`).
