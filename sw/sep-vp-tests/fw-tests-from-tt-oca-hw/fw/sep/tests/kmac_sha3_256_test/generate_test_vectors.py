#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Tenstorrent USA, Inc.
"""
Generate SHA3/KMAC test vectors from known-good Python hashlib implementation.
This prevents typos in hardcoded expected values.
"""

import hashlib
import sys

def generate_sha3_test_vectors():
    """Generate test vectors for common SHA3 test cases"""

    test_cases = [
        {
            'name': 'sha3_256_empty',
            'msg': b'',
            'desc': 'SHA3-256 of empty string',
        },
        {
            'name': 'sha3_256_abc',
            'msg': b'abc',
            'desc': 'SHA3-256 of "abc" (NIST short message test)',
        },
        {
            'name': 'sha3_256_alphabet',
            'msg': b'abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq',
            'desc': 'SHA3-256 of alphabet string (NIST test)',
        },
    ]

    vectors = []
    for tc in test_cases:
        h = hashlib.sha3_256()
        h.update(tc['msg'])
        digest = h.digest()

        # Convert to 32-bit big-endian words
        words = []
        for i in range(0, 32, 4):
            word = int.from_bytes(digest[i:i+4], byteorder='big')
            words.append(f"0x{word:08x}")

        vectors.append({
            'name': tc['name'],
            'desc': tc['desc'],
            'msg_hex': tc['msg'].hex() if tc['msg'] else '(empty)',
            'digest_hex': digest.hex(),
            'words': words
        })

    return vectors

def generate_header(vectors, output_file):
    """Generate C header file with test vectors"""

    header = """/* Auto-generated test vectors - DO NOT EDIT BY HAND
 *
 * Use 'make generate-test-vectors' to regenerate this file.
 */

#ifndef KMAC_TEST_VECTORS_H_
#define KMAC_TEST_VECTORS_H_

#include <stdint.h>

"""

    for vec in vectors:
        header += f"""/* {vec['desc']}
 * Message: {vec['msg_hex']}
 * Digest:  {vec['digest_hex']}
 */
static const uint32_t {vec['name']}_ref[8] = {{
    {', '.join(vec['words'][:4])},
    {', '.join(vec['words'][4:])}
}};

"""

    header += "#endif  // KMAC_TEST_VECTORS_H_\n"

    with open(output_file, 'w') as f:
        f.write(header)

    print(f"Generated {output_file}")
    for vec in vectors:
        print(f"  {vec['name']}: {vec['digest_hex']}")

if __name__ == '__main__':
    output = sys.argv[1] if len(sys.argv) > 1 else 'kmac_test_vectors.h'
    vectors = generate_sha3_test_vectors()
    generate_header(vectors, output)
