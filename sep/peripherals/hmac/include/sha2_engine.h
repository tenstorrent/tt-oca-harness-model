#pragma once

#include <cstddef>
#include <cstdint>

/**
 * @file sha2_engine.h
 * @brief SHA-2 core with externally visible chaining state.
 *
 * The HMAC block exposes its intermediate hash state to software through the
 * DIGEST registers so that a hash can be paused and resumed, possibly with
 * another hash running in between. Reproducing that requires loading an
 * arbitrary chaining state into the hash, which OpenSSL's supported API
 * deliberately does not permit: EVP exposes no digest mid-state, and the
 * low-level SHA256_CTX/SHA512_CTX structs that would allow it are deprecated
 * and vanish entirely from builds configured with OPENSSL_NO_DEPRECATED_3_0.
 *
 * This engine therefore implements the compression function directly, keeping
 * the chaining variables and the absorbed bit count as first-class state that
 * hash_stop can read out and hash_continue can write back.
 */

enum class sha2_mode {
  sha256,
  sha384,
  sha512
};

class sha2_engine {
public:
  sha2_engine();

  /// Reset to the initial chaining values for the given mode.
  void init(sha2_mode mode);

  /// Absorb exactly one full block (block_bytes() bytes).
  void compress(const uint8_t *block);

  /// Absorb the trailing partial data, apply padding and emit the digest.
  /// @param tail     Remaining message bytes, fewer than block_bytes()
  /// @param tail_len Length of @p tail in bytes
  /// @param out      Receives digest_bytes() bytes
  void finalize(const uint8_t *tail, size_t tail_len, uint8_t *out);

  sha2_mode mode() const { return m_mode; }

  /// Block size in bytes: 64 for SHA-256, 128 for SHA-384/512.
  size_t block_bytes() const { return (m_mode == sha2_mode::sha256) ? 64u : 128u; }

  /// Digest size in bytes.
  size_t digest_bytes() const;

  /// Width of one chaining variable in bytes: 4 for SHA-256, 8 otherwise.
  size_t word_bytes() const { return (m_mode == sha2_mode::sha256) ? 4u : 8u; }

  // Chaining state access, used to implement hash_stop and hash_continue.
  // For SHA-256 only the low 32 bits of each word are significant.
  uint64_t chain(unsigned index) const { return m_h[index & 0x7]; }
  void set_chain(unsigned index, uint64_t value) { m_h[index & 0x7] = value; }

  /// Bits absorbed so far through compress(). Excludes anything passed to
  /// finalize(), which is accounted for at the point of padding.
  uint64_t absorbed_bits() const { return m_absorbed_bits; }
  void set_absorbed_bits(uint64_t bits) { m_absorbed_bits = bits; }

private:
  void compress256(const uint8_t *block);
  void compress512(const uint8_t *block);

  sha2_mode m_mode;
  uint64_t  m_h[8];
  uint64_t  m_absorbed_bits;
};
