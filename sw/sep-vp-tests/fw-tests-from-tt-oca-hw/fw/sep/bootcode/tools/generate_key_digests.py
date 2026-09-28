#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

"""Generate key_digests.c from RSA-3072 PEM key files.

Usage:
    python3 generate_key_digests.py --keys rom_key0.pem,rom_key1.pem,...
    python3 generate_key_digests.py --keys-dir path/to/signing_keys/
    python3 generate_key_digests.py --generate-key build/ephemeral.pem

Reads RSA-3072 private key PEM files, extracts the public key modulus,
computes SHA-256(modulus_big_endian_384bytes), and outputs key_digests.c.
"""

import argparse
import hashlib
import os
import sys
from pathlib import Path

RSA_KEY_BITS = 3072
RSA_MODULUS_BYTES = RSA_KEY_BITS // 8
RSA_PUBLIC_EXPONENT = 65537

# ITSEC-214: this was the modulus digest of the repository-carried dev0 key.
# Refuse it by key material, not filename, so renaming the PEM cannot bypass
# the guard.
FORBIDDEN_MODULUS_DIGESTS = {
    bytes.fromhex("4676d023736b5ebd5131f75b062a355e9ae1790e80c872b5ee9b0c1fff04c3e3")
}


def extract_modulus_digest(pem_path):
    """Extract RSA-3072 modulus from PEM and return SHA-256 digest."""
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric import rsa

    with open(pem_path, "rb") as f:
        private_key = serialization.load_pem_private_key(f.read(), password=None)
    if not isinstance(private_key, rsa.RSAPrivateKey):
        raise ValueError(f"{pem_path}: expected an RSA private key")
    if private_key.key_size != RSA_KEY_BITS:
        raise ValueError(
            f"{pem_path}: expected RSA-{RSA_KEY_BITS}, got RSA-{private_key.key_size}"
        )
    pub = private_key.public_key()
    numbers = pub.public_numbers()
    if numbers.e != RSA_PUBLIC_EXPONENT:
        raise ValueError(
            f"{pem_path}: expected public exponent {RSA_PUBLIC_EXPONENT}, got {numbers.e}"
        )
    modulus_bytes = numbers.n.to_bytes(RSA_MODULUS_BYTES, byteorder="big")
    digest = hashlib.sha256(modulus_bytes).digest()
    if digest in FORBIDDEN_MODULUS_DIGESTS:
        raise ValueError(
            f"{pem_path}: refusing ITSEC-214 committed dev0 key "
            f"(modulus SHA-256 {digest.hex()})"
        )
    return digest


def generate_ephemeral_key(pem_path):
    """Generate a fresh RSA-3072 test key at *pem_path* with mode 0600."""
    from cryptography.hazmat.primitives import serialization
    from cryptography.hazmat.primitives.asymmetric import rsa

    pem_path = Path(pem_path)
    pem_path.parent.mkdir(parents=True, exist_ok=True)
    private_key = rsa.generate_private_key(
        public_exponent=RSA_PUBLIC_EXPONENT, key_size=RSA_KEY_BITS
    )
    encoded = private_key.private_bytes(
        serialization.Encoding.PEM,
        serialization.PrivateFormat.PKCS8,
        serialization.NoEncryption(),
    )
    tmp_path = pem_path.with_name(f".{pem_path.name}.tmp-{os.getpid()}")
    fd = os.open(tmp_path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    try:
        with os.fdopen(fd, "wb") as f:
            f.write(encoded)
        os.replace(tmp_path, pem_path)
        os.chmod(pem_path, 0o600)
    finally:
        if tmp_path.exists():
            tmp_path.unlink()
    return pem_path


def format_digest(name, digest):
    """Format a digest as a C static const array."""
    lines = [f"static const uint8_t {name}[SHA256_DIGEST_SIZE_BYTES] = {{"]
    # 16 bytes per line: 4 indent + 16 * "0xXX, " lands on 99 columns, just inside the
    # tree's clang-format ColumnLimit of 100. At 8 per line clang-format repacks the array
    # and `make ocah-format-c-check` reports the generated file as unformatted.
    for i in range(0, 32, 16):
        chunk = ", ".join(f"0x{digest[i + j]:02x}" for j in range(16))
        lines.append(f"    {chunk},")
    lines.append("};")
    return "\n".join(lines)


# Slot names are positional only, because the ROM draws no trust distinction
# between them: all six are ROM-embedded root keys, resolved the same way by the
# same bitmap (public_key_select_classic bits [7:0], which also index
# CHIPLET_PUBK_REVOKE).
SLOT_NAMES = [f"rom_key{n}" for n in range(6)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--keys", help="Comma-separated PEM files (slot0,slot1,...)")
    group.add_argument("--keys-dir", help="Directory with rsa_private_key.*.pem files")
    group.add_argument(
        "--generate-key",
        metavar="PEM",
        help="Generate a fresh RSA-3072 key and place it in ROM slot 0",
    )
    parser.add_argument("-o", "--output", default="key_digests.c")
    args = parser.parse_args()

    pem_files = {}
    if args.generate_key:
        pem_files[SLOT_NAMES[0]] = generate_ephemeral_key(args.generate_key)
    elif args.keys_dir:
        d = Path(args.keys_dir)
        for name in SLOT_NAMES:
            p = d / f"rsa_private_key.{name}.pem"
            if p.exists():
                pem_files[name] = p
    else:
        paths = args.keys.split(",")
        if len(paths) > len(SLOT_NAMES):
            parser.error(f"--keys accepts at most {len(SLOT_NAMES)} entries")
        for i, path in enumerate(paths):
            path = path.strip()
            if path and path != "-":
                pem_files[SLOT_NAMES[i]] = Path(path)

    # Generate C file
    digest_defs = []
    entries = []
    for name in SLOT_NAMES:
        if name in pem_files:
            digest = extract_modulus_digest(pem_files[name])
            var = f"digest_{name}"
            digest_defs.append(format_digest(var, digest))
            entries.append(f"    {{.digest = {var}}}, // slot {SLOT_NAMES.index(name)}: {name}")
            print(f"  slot {SLOT_NAMES.index(name)} ({name}): {pem_files[name]}", file=sys.stderr)
        else:
            entries.append(f"    {{.digest = (void *)0}}, // slot {SLOT_NAMES.index(name)}: {name}")

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    with open(output, "w") as f:
        # Emit the licence header the tree-wide OSPO sweep expects. Without it every
        # regeneration silently strips the header off key_digests.c again.
        f.write("/* SPDX-License-Identifier: Apache-2.0 */\n")
        f.write("/* SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc. */\n\n")
        f.write("// Auto-generated by generate_key_digests.py — DO NOT EDIT.\n")
        f.write("//\n")
        f.write("// SHA-256 digests of RSA-3072 public key moduli.\n\n")
        f.write('#include "key_digests.h"\n\n')
        for d in digest_defs:
            f.write(d + "\n\n")
        f.write("public_key_info_t public_key_digests[NUM_PUBLIC_KEY_DIGESTS] = {\n")
        f.write("\n".join(entries) + "\n")
        f.write("};\n")

    print(f"Generated {output}", file=sys.stderr)


if __name__ == "__main__":
    main()
