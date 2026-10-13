#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
"""Generate test/inc/abr_acvp_sigver_all.h from the NIST ACVP-Server ML-DSA vectors.

Takes every ML-DSA-87 signature-verification case the ACVP-Server publishes for the
two interfaces Adams Bridge implements -- pure ML-DSA with a context (tgId 5) and
external mu (tgId 11) -- plus the deterministic ML-DSA-87 signatures from sigGen
(tgId 5), which must all verify.

Each case is classified by decoding its signature here, independently of the model,
so the testbench can check what the RTL does with it:

  ACCEPT      the signature verifies: VERIFY_RES equals c~
  Z_REJECT    ||z|| >= gamma1 - beta: the core stops before it asks for the message
              and VERIFY_RES keeps the inverted c~ it was preloaded with
  HINT_REJECT the hint encoding is malformed: the core consumes the message, then
              stops with the inverted c~
  MISMATCH    well formed but wrong: VERIFY_RES is a recomputed c~' that is neither
              c~ nor its inverse

The files are fetched from a pinned ACVP-Server commit and their SHA-256 digests are
written into the header, so a regenerated header shows whether its inputs changed.

Usage: gen_abr_acvp_sigver.py [--cache DIR]
"""

import argparse
import hashlib
import json
import os
import urllib.request

ACVP_COMMIT = "a7f283cdc87d"
ACVP_URL = (
    "https://raw.githubusercontent.com/usnistgov/ACVP-Server/{commit}/gen-val/json-files/"
    "{name}/internalProjection.json"
)
SIGVER = "ML-DSA-sigVer-FIPS204"
SIGGEN = "ML-DSA-sigGen-FIPS204"
PARAM = "ML-DSA-87"

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "inc", "abr_acvp_sigver_all.h")

# ML-DSA-87 (FIPS 204 Table 1)
N = 256
K = 8
L = 7
GAMMA1 = 1 << 19
BETA = 120
OMEGA = 75
CTILDE_BYTES = 64
POLYZ_PACKED = 640
SIG_BYTES = CTILDE_BYTES + L * POLYZ_PACKED + OMEGA + K


def fetch(name, cache):
    path = os.path.join(cache, f"{name}_{ACVP_COMMIT}.json")
    if not os.path.exists(path):
        with urllib.request.urlopen(ACVP_URL.format(commit=ACVP_COMMIT, name=name)) as r:
            data = r.read()
        with open(path, "wb") as f:
            f.write(data)
    with open(path, "rb") as f:
        data = f.read()
    return json.loads(data), hashlib.sha256(data).hexdigest()


def z_out_of_range(sig):
    """True when any z coefficient has |z| >= gamma1 - beta (FIPS 204 Algorithm 8)."""
    body = sig[CTILDE_BYTES : CTILDE_BYTES + L * POLYZ_PACKED]
    for i in range(L * N // 2):
        b = body[5 * i : 5 * i + 5]
        t0 = b[0] | (b[1] << 8) | ((b[2] & 0x0F) << 16)
        t1 = (b[2] >> 4) | (b[3] << 4) | (b[4] << 12)
        for t in (t0, t1):
            if abs(GAMMA1 - t) >= GAMMA1 - BETA:
                return True
    return False


def hint_malformed(sig):
    """True when the hint encoding breaks FIPS 204 HintBitUnpack (Algorithm 21)."""
    h = sig[CTILDE_BYTES + L * POLYZ_PACKED :]
    k = 0
    for i in range(K):
        end = h[OMEGA + i]
        if end < k or end > OMEGA:
            return True
        for j in range(k, end):
            if j > k and h[j] <= h[j - 1]:
                return True
        k = end
    return any(h[j] for j in range(k, OMEGA))


def classify(sig, passed):
    if passed:
        return "ACCEPT"
    if z_out_of_range(sig):
        return "Z_REJECT"
    if hint_malformed(sig):
        return "HINT_REJECT"
    return "MISMATCH"


def group(doc, tg_id):
    for g in doc["testGroups"]:
        if g["tgId"] == tg_id:
            if g["parameterSet"] != PARAM:
                raise SystemExit(f"tgId {tg_id} is {g['parameterSet']}, expected {PARAM}")
            return g
    raise SystemExit(f"tgId {tg_id} missing")


def c_str(hexstr):
    return '"' + hexstr.lower() + '"'


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument(
        "--cache", default=os.environ.get("TMPDIR", "."), help="download cache directory"
    )
    args = ap.parse_args()

    sigver, sigver_sha = fetch(SIGVER, args.cache)
    siggen, siggen_sha = fetch(SIGGEN, args.cache)

    cases = []
    for tg_id in (5, 11):
        g = group(sigver, tg_id)
        ext_mu = bool(g["externalMu"])
        for t in g["tests"]:
            sig = bytes.fromhex(t["signature"])
            if len(sig) != SIG_BYTES:
                raise SystemExit(f"sigVer tc {t['tcId']}: signature is {len(sig)} bytes")
            cases.append(
                {
                    "origin": f"sigVer tgId {tg_id} tcId {t['tcId']}",
                    "reason": t["reason"],
                    "kind": classify(sig, t["testPassed"]),
                    "ext_mu": ext_mu,
                    "pk": t["pk"],
                    "msg": t.get("message", ""),
                    "ctx": t.get("context", ""),
                    "mu": t.get("mu", ""),
                    "sig": t["signature"],
                }
            )

    g = group(siggen, 5)
    if not g["deterministic"] or g["signatureInterface"] != "external" or g["preHash"] != "pure":
        raise SystemExit("sigGen tgId 5 is not deterministic external pure ML-DSA")
    for t in g["tests"]:
        cases.append(
            {
                "origin": f"sigGen tgId 5 tcId {t['tcId']} (deterministic)",
                "reason": "deterministic signature - signature should verify successfully",
                "kind": "ACCEPT",
                "ext_mu": False,
                "pk": t["pk"],
                "msg": t["message"],
                "ctx": t["context"],
                "mu": "",
                "sig": t["signature"],
            }
        )

    lines = [
        "// SPDX-License-Identifier: Apache-2.0",
        "// SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.",
        "// Generated by test/scripts/gen_abr_acvp_sigver.py. Do not edit.",
        f"// NIST ACVP-Server commit {ACVP_COMMIT}:",
        f"//   {SIGVER}/internalProjection.json sha256 {sigver_sha}",
        f"//   {SIGGEN}/internalProjection.json sha256 {siggen_sha}",
        "",
        "#pragma once",
        "",
        "enum abr_acvp_kind { ABR_ACVP_ACCEPT, ABR_ACVP_Z_REJECT, ABR_ACVP_HINT_REJECT, ABR_ACVP_MISMATCH };",
        "",
        "struct abr_acvp_case {",
        "    const char *origin;",
        "    const char *reason;",
        "    abr_acvp_kind kind;",
        "    bool external_mu;",
        "    const char *pk;   // hex",
        "    const char *msg;  // hex, empty for external mu",
        "    const char *ctx;  // hex",
        "    const char *mu;   // hex, external mu only",
        "    const char *sig;  // hex",
        "};",
        "",
        "static const abr_acvp_case abr_acvp_cases[] = {",
    ]
    for c in cases:
        lines.append("    {")
        lines.append(f'        "{c["origin"]}", "{c["reason"]}", ABR_ACVP_{c["kind"]},')
        lines.append(f"        {'true' if c['ext_mu'] else 'false'},")
        for key in ("pk", "msg", "ctx", "mu", "sig"):
            lines.append(f"        {c_str(c[key])},")
        lines.append("    },")
    lines += ["};", ""]

    with open(OUT, "w", encoding="ascii") as f:
        f.write("\n".join(lines))

    counts = {}
    for c in cases:
        counts[c["kind"]] = counts.get(c["kind"], 0) + 1
    print(f"wrote {len(cases)} cases to {OUT}: {counts}")


if __name__ == "__main__":
    main()
