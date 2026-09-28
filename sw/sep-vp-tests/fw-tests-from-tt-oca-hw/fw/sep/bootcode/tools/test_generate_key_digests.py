#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.

"""Tests for ephemeral secure-boot key and digest generation."""

import hashlib
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec, rsa

import generate_key_digests as generator


class GenerateKeyDigestsTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def test_generated_key_is_rsa3072_and_digest_uses_raw_modulus(self):
        pem = generator.generate_ephemeral_key(self.root / "ephemeral.pem")
        digest = generator.extract_modulus_digest(pem)

        key = serialization.load_pem_private_key(pem.read_bytes(), password=None)
        self.assertIsInstance(key, rsa.RSAPrivateKey)
        self.assertEqual(key.key_size, 3072)
        self.assertEqual(key.public_key().public_numbers().e, 65537)
        modulus = key.public_key().public_numbers().n.to_bytes(384, "big")
        self.assertEqual(digest, hashlib.sha256(modulus).digest())
        if os.name == "posix":
            self.assertEqual(pem.stat().st_mode & 0o777, 0o600)

    def test_each_generation_uses_a_fresh_key(self):
        first = generator.generate_ephemeral_key(self.root / "first.pem")
        second = generator.generate_ephemeral_key(self.root / "second.pem")
        self.assertNotEqual(
            generator.extract_modulus_digest(first),
            generator.extract_modulus_digest(second),
        )

    def test_cli_populates_only_rom_slot_zero(self):
        pem = self.root / "ephemeral.pem"
        output = self.root / "key_digests.c"
        subprocess.run(
            [
                sys.executable,
                str(Path(__file__).with_name("generate_key_digests.py")),
                "--generate-key",
                str(pem),
                "--output",
                str(output),
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        generated = output.read_text()
        self.assertEqual(generated.count("static const uint8_t digest_"), 1)
        self.assertIn("{.digest = digest_rom_key0}", generated)
        self.assertEqual(generated.count("{.digest = (void *)0}"), 5)

    def test_rejects_wrong_key_type_and_size(self):
        ec_path = self.root / "ec.pem"
        ec_path.write_bytes(
            ec.generate_private_key(ec.SECP256R1()).private_bytes(
                serialization.Encoding.PEM,
                serialization.PrivateFormat.PKCS8,
                serialization.NoEncryption(),
            )
        )
        with self.assertRaisesRegex(ValueError, "expected an RSA private key"):
            generator.extract_modulus_digest(ec_path)

        rsa2048_path = self.root / "rsa2048.pem"
        rsa2048_path.write_bytes(
            rsa.generate_private_key(public_exponent=65537, key_size=2048).private_bytes(
                serialization.Encoding.PEM,
                serialization.PrivateFormat.PKCS8,
                serialization.NoEncryption(),
            )
        )
        with self.assertRaisesRegex(ValueError, "expected RSA-3072"):
            generator.extract_modulus_digest(rsa2048_path)

    def test_rejects_forbidden_key_by_modulus_digest(self):
        pem = generator.generate_ephemeral_key(self.root / "forbidden.pem")
        digest = generator.extract_modulus_digest(pem)
        with mock.patch.object(generator, "FORBIDDEN_MODULUS_DIGESTS", {digest}):
            with self.assertRaisesRegex(ValueError, "ITSEC-214"):
                generator.extract_modulus_digest(pem)


if __name__ == "__main__":
    unittest.main()
