#include "sha2_engine.h"

#include <cstring>
#include <vector>

namespace {

inline uint32_t ror32(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }
inline uint64_t ror64(uint64_t x, unsigned n) { return (x >> n) | (x << (64 - n)); }

const uint32_t kInit256[8] = {
    0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
    0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};

const uint64_t kInit384[8] = {
    0xcbbb9d5dc1059ed8ull, 0x629a292a367cd507ull, 0x9159015a3070dd17ull,
    0x152fecd8f70e5939ull, 0x67332667ffc00b31ull, 0x8eb44a8768581511ull,
    0xdb0c2e0d64f98fa7ull, 0x47b5481dbefa4fa4ull};

const uint64_t kInit512[8] = {
    0x6a09e667f3bcc908ull, 0xbb67ae8584caa73bull, 0x3c6ef372fe94f82bull,
    0xa54ff53a5f1d36f1ull, 0x510e527fade682d1ull, 0x9b05688c2b3e6c1full,
    0x1f83d9abfb41bd6bull, 0x5be0cd19137e2179ull};

const uint32_t kK256[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

const uint64_t kK512[80] = {
    0x428a2f98d728ae22ull, 0x7137449123ef65cdull, 0xb5c0fbcfec4d3b2full,
    0xe9b5dba58189dbbcull, 0x3956c25bf348b538ull, 0x59f111f1b605d019ull,
    0x923f82a4af194f9bull, 0xab1c5ed5da6d8118ull, 0xd807aa98a3030242ull,
    0x12835b0145706fbeull, 0x243185be4ee4b28cull, 0x550c7dc3d5ffb4e2ull,
    0x72be5d74f27b896full, 0x80deb1fe3b1696b1ull, 0x9bdc06a725c71235ull,
    0xc19bf174cf692694ull, 0xe49b69c19ef14ad2ull, 0xefbe4786384f25e3ull,
    0x0fc19dc68b8cd5b5ull, 0x240ca1cc77ac9c65ull, 0x2de92c6f592b0275ull,
    0x4a7484aa6ea6e483ull, 0x5cb0a9dcbd41fbd4ull, 0x76f988da831153b5ull,
    0x983e5152ee66dfabull, 0xa831c66d2db43210ull, 0xb00327c898fb213full,
    0xbf597fc7beef0ee4ull, 0xc6e00bf33da88fc2ull, 0xd5a79147930aa725ull,
    0x06ca6351e003826full, 0x142929670a0e6e70ull, 0x27b70a8546d22ffcull,
    0x2e1b21385c26c926ull, 0x4d2c6dfc5ac42aedull, 0x53380d139d95b3dfull,
    0x650a73548baf63deull, 0x766a0abb3c77b2a8ull, 0x81c2c92e47edaee6ull,
    0x92722c851482353bull, 0xa2bfe8a14cf10364ull, 0xa81a664bbc423001ull,
    0xc24b8b70d0f89791ull, 0xc76c51a30654be30ull, 0xd192e819d6ef5218ull,
    0xd69906245565a910ull, 0xf40e35855771202aull, 0x106aa07032bbd1b8ull,
    0x19a4c116b8d2d0c8ull, 0x1e376c085141ab53ull, 0x2748774cdf8eeb99ull,
    0x34b0bcb5e19b48a8ull, 0x391c0cb3c5c95a63ull, 0x4ed8aa4ae3418acbull,
    0x5b9cca4f7763e373ull, 0x682e6ff3d6b2b8a3ull, 0x748f82ee5defb2fcull,
    0x78a5636f43172f60ull, 0x84c87814a1f0ab72ull, 0x8cc702081a6439ecull,
    0x90befffa23631e28ull, 0xa4506cebde82bde9ull, 0xbef9a3f7b2c67915ull,
    0xc67178f2e372532bull, 0xca273eceea26619cull, 0xd186b8c721c0c207ull,
    0xeada7dd6cde0eb1eull, 0xf57d4f7fee6ed178ull, 0x06f067aa72176fbaull,
    0x0a637dc5a2c898a6ull, 0x113f9804bef90daeull, 0x1b710b35131c471bull,
    0x28db77f523047d84ull, 0x32caab7b40c72493ull, 0x3c9ebe0a15c9bebcull,
    0x431d67c49c100d4cull, 0x4cc5d4becb3e42b6ull, 0x597f299cfc657e2aull,
    0x5fcb6fab3ad6faecull, 0x6c44198c4a475817ull};

} // namespace

sha2_engine::sha2_engine() : m_mode(sha2_mode::sha256), m_absorbed_bits(0) {
  init(sha2_mode::sha256);
}

void sha2_engine::init(sha2_mode mode) {
  m_mode = mode;
  m_absorbed_bits = 0;

  switch (mode) {
    case sha2_mode::sha256:
      for (unsigned i = 0; i < 8; i++) m_h[i] = kInit256[i];
      break;
    case sha2_mode::sha384:
      for (unsigned i = 0; i < 8; i++) m_h[i] = kInit384[i];
      break;
    case sha2_mode::sha512:
      for (unsigned i = 0; i < 8; i++) m_h[i] = kInit512[i];
      break;
  }
}

size_t sha2_engine::digest_bytes() const {
  switch (m_mode) {
    case sha2_mode::sha256: return 32;
    case sha2_mode::sha384: return 48;
    case sha2_mode::sha512: return 64;
  }
  return 32;
}

void sha2_engine::compress(const uint8_t *block) {
  if (m_mode == sha2_mode::sha256) {
    compress256(block);
  } else {
    compress512(block);
  }
  m_absorbed_bits += static_cast<uint64_t>(block_bytes()) * 8u;
}

void sha2_engine::compress256(const uint8_t *block) {
  uint32_t w[64];

  for (unsigned i = 0; i < 16; i++) {
    w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
           (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
           (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
            static_cast<uint32_t>(block[i * 4 + 3]);
  }
  for (unsigned i = 16; i < 64; i++) {
    const uint32_t s0 = ror32(w[i - 15], 7) ^ ror32(w[i - 15], 18) ^ (w[i - 15] >> 3);
    const uint32_t s1 = ror32(w[i - 2], 17) ^ ror32(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  uint32_t a = static_cast<uint32_t>(m_h[0]);
  uint32_t b = static_cast<uint32_t>(m_h[1]);
  uint32_t c = static_cast<uint32_t>(m_h[2]);
  uint32_t d = static_cast<uint32_t>(m_h[3]);
  uint32_t e = static_cast<uint32_t>(m_h[4]);
  uint32_t f = static_cast<uint32_t>(m_h[5]);
  uint32_t g = static_cast<uint32_t>(m_h[6]);
  uint32_t h = static_cast<uint32_t>(m_h[7]);

  for (unsigned i = 0; i < 64; i++) {
    const uint32_t S1 = ror32(e, 6) ^ ror32(e, 11) ^ ror32(e, 25);
    const uint32_t ch = (e & f) ^ (~e & g);
    const uint32_t t1 = h + S1 + ch + kK256[i] + w[i];
    const uint32_t S0 = ror32(a, 2) ^ ror32(a, 13) ^ ror32(a, 22);
    const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    const uint32_t t2 = S0 + maj;

    h = g; g = f; f = e; e = d + t1;
    d = c; c = b; b = a; a = t1 + t2;
  }

  m_h[0] = static_cast<uint32_t>(m_h[0]) + a;
  m_h[1] = static_cast<uint32_t>(m_h[1]) + b;
  m_h[2] = static_cast<uint32_t>(m_h[2]) + c;
  m_h[3] = static_cast<uint32_t>(m_h[3]) + d;
  m_h[4] = static_cast<uint32_t>(m_h[4]) + e;
  m_h[5] = static_cast<uint32_t>(m_h[5]) + f;
  m_h[6] = static_cast<uint32_t>(m_h[6]) + g;
  m_h[7] = static_cast<uint32_t>(m_h[7]) + h;

  for (unsigned i = 0; i < 8; i++) {
    m_h[i] &= 0xFFFFFFFFull;
  }
}

void sha2_engine::compress512(const uint8_t *block) {
  uint64_t w[80];

  for (unsigned i = 0; i < 16; i++) {
    uint64_t word = 0;
    for (unsigned b = 0; b < 8; b++) {
      word = (word << 8) | block[i * 8 + b];
    }
    w[i] = word;
  }
  for (unsigned i = 16; i < 80; i++) {
    const uint64_t s0 = ror64(w[i - 15], 1) ^ ror64(w[i - 15], 8) ^ (w[i - 15] >> 7);
    const uint64_t s1 = ror64(w[i - 2], 19) ^ ror64(w[i - 2], 61) ^ (w[i - 2] >> 6);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  uint64_t a = m_h[0], b = m_h[1], c = m_h[2], d = m_h[3];
  uint64_t e = m_h[4], f = m_h[5], g = m_h[6], h = m_h[7];

  for (unsigned i = 0; i < 80; i++) {
    const uint64_t S1 = ror64(e, 14) ^ ror64(e, 18) ^ ror64(e, 41);
    const uint64_t ch = (e & f) ^ (~e & g);
    const uint64_t t1 = h + S1 + ch + kK512[i] + w[i];
    const uint64_t S0 = ror64(a, 28) ^ ror64(a, 34) ^ ror64(a, 39);
    const uint64_t maj = (a & b) ^ (a & c) ^ (b & c);
    const uint64_t t2 = S0 + maj;

    h = g; g = f; f = e; e = d + t1;
    d = c; c = b; b = a; a = t1 + t2;
  }

  m_h[0] += a; m_h[1] += b; m_h[2] += c; m_h[3] += d;
  m_h[4] += e; m_h[5] += f; m_h[6] += g; m_h[7] += h;
}

void sha2_engine::finalize(const uint8_t *tail, size_t tail_len, uint8_t *out) {
  const size_t block = block_bytes();
  // SHA-256 encodes the length in 64 bits, SHA-384/512 in 128 bits.
  const size_t len_bytes = (m_mode == sha2_mode::sha256) ? 8u : 16u;

  const uint64_t total_bits = m_absorbed_bits + static_cast<uint64_t>(tail_len) * 8u;

  // 0x80 terminator, zero padding, then the length, rounded up to a block.
  size_t padded = tail_len + 1 + len_bytes;
  padded = ((padded + block - 1) / block) * block;

  std::vector<uint8_t> buffer(padded, 0);
  if (tail_len > 0) {
    std::memcpy(buffer.data(), tail, tail_len);
  }
  buffer[tail_len] = 0x80;

  // Length field is big-endian and occupies the final len_bytes. Only the low
  // 64 bits are ever populated, which bounds messages at 2^64 bits; the upper
  // half of the 128-bit SHA-512 field stays zero.
  for (unsigned i = 0; i < 8; i++) {
    buffer[padded - 1 - i] = static_cast<uint8_t>((total_bits >> (8 * i)) & 0xFF);
  }

  for (size_t offset = 0; offset < padded; offset += block) {
    compress(buffer.data() + offset);
  }

  const size_t words = digest_bytes() / word_bytes();
  for (size_t i = 0; i < words; i++) {
    if (m_mode == sha2_mode::sha256) {
      const uint32_t value = static_cast<uint32_t>(m_h[i]);
      out[i * 4]     = static_cast<uint8_t>((value >> 24) & 0xFF);
      out[i * 4 + 1] = static_cast<uint8_t>((value >> 16) & 0xFF);
      out[i * 4 + 2] = static_cast<uint8_t>((value >> 8) & 0xFF);
      out[i * 4 + 3] = static_cast<uint8_t>(value & 0xFF);
    } else {
      const uint64_t value = m_h[i];
      for (unsigned b = 0; b < 8; b++) {
        out[i * 8 + b] = static_cast<uint8_t>((value >> (56 - 8 * b)) & 0xFF);
      }
    }
  }
}
