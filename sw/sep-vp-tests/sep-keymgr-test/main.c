/*
 * Key Manager firmware test for the SEP VP
 *
 * Drives the KM from the SEP EL2 CPU through the only interface SEP has: the
 * KM<->SEP mailbox at KM_MAILBOX_SEP (0x10920000).  Everything the KM exposes
 * is behind that window, so this test carries its own copy of the message
 * framing the KM ROM expects -- a 32-bit header (seq[7:0], id[15:8],
 * payload_len[23:16], CRC-8/ROHC[31:24]) followed by the payload words and a
 * CRC-32C trailer, with the final word of every frame tagged as a separator.
 *
 * Phases:
 *   1  Boot handshake        unsolicited RESP_KM_READY, byte-exact
 *   2  Command round-trip    CMD_STAT, sequence echo and framing
 *   3  Protocol negatives    one case per return code
 *   4  CMD_KEY_LOAD          8-word key, handle/geometry in the return arg
 *   5  Frame reassembly      32- and 128-word keys, frames far wider than the
 *                            16-word FIFO, so the push has to stall and resume
 *   6  CMD_KEY_TRANSFER      sideload into HMAC, then hash and compare the
 *                            digest against a host-computed HMAC-SHA256
 *   7  Policy and lifecycle  dest_valid, lock_use, revoke, engine shred
 *   8  Stub pinning          commands documented as unimplemented
 *
 * Phases 5 and 6 are the ones with no coverage elsewhere: nothing else in the
 * VP suite sends a frame wider than the FIFO, and nothing else proves key
 * material survives the XOR-share sideload path into a crypto engine.
 */

#include <stdint.h>
#include "och_sep_top_reg.h"
#include "och_sep_common.h"

int printf(const char *format, ...);

/* =========================================================================
 * Mailbox registers
 * ========================================================================= */
#define MB_WDATA    KM_MAILBOX_SEP_SEP_WRITE_DATA_REG_ADDR
#define MB_WSEP     KM_MAILBOX_SEP_SEP_WRITE_SEPARATOR_REG_ADDR
#define MB_RDATA    KM_MAILBOX_SEP_SEP_READ_DATA_REG_ADDR
#define MB_STATUS   KM_MAILBOX_SEP_SEP_STATUS_REG_ADDR
#define MB_IRQS     KM_MAILBOX_SEP_SEP_IRQ_STATUS_REG_ADDR
#define MB_CTRL     KM_MAILBOX_SEP_SEP_CTRL_REG_ADDR

typedef KM_MAILBOX_SEP_STATUS_REG_reg_u mb_status_t;

/* Each FIFO is 16 words deep (ROM_KM_MAILBOX_FIFO_DEPTH). */
#define KM_FIFO_DEPTH   16u

/* Spin budget for a single word.  Generous: the KM firmware thread only runs
 * when the SEP CPU yields simulated time by spinning here. */
#define MB_GUARD        2000000u

/* =========================================================================
 * KM protocol constants
 * ========================================================================= */
#define CMD_HW_VER              0x00u
#define CMD_STAT                0x03u
#define CMD_EXEC_ROM            0x10u
#define CMD_SRAM_LOAD_EXEC      0x11u
#define CMD_SRAM_EXEC           0x12u
#define CMD_KEY_GENERATE        0x22u
#define CMD_KEY_REVOKE          0x23u
#define CMD_KEY_TRANSFER        0x24u
#define CMD_ENGINE_SHRED        0x25u
#define CMD_KEY_LOAD            0x26u
#define CMD_ABR_SK_TRANSFER     0x27u

/* Retired with the KPVLP window; must now be rejected outright. */
#define CMD_RETIRED_SLOT_REQ    0x20u
#define CMD_RETIRED_KEY_REG     0x21u

#define RESP_CMD                0x00u
#define RESP_KM_READY           0x55u

#define RET_SUCCESS              0
#define RET_FAILURE             (-1)
#define RET_HEADER_CRC          (-2)
#define RET_CMD_NOSEQ           (-3)
#define RET_INVALID_CMD         (-4)
#define RET_INVALID_LEN         (-5)
#define RET_PAYLOAD_CRC         (-6)
#define RET_INVALID_ARG         (-7)

#define DEST_HMAC               (1u << 0)
#define DEST_KMAC               (1u << 1)
#define DEST_AES                (1u << 2)
#define DEST_OTBN               (1u << 3)

/* Boot announcement: seq=0, id=0x55, len=0, CRC-8/ROHC over {00,55,00}. */
#define KM_READY_HEADER         0x8d005500u

#define KM_MAX_KEY_WORDS        128u

/* SEP_RESET_CTRL.SW_RESET_N bit 0 gates the KM out of software reset. */
#define SW_RESET_N_KM           (1u << 0)

/* =========================================================================
 * Check bookkeeping
 * ========================================================================= */
static unsigned checks_passed;
static unsigned checks_failed;

static void chk(int ok, const char *what)
{
    if (ok) {
        checks_passed++;
        printf("  [PASS] %s\n", what);
    } else {
        checks_failed++;
        printf("  [FAIL] %s\n", what);
    }
}

static void chk_eq(uint32_t got, uint32_t want, const char *what)
{
    if (got == want) {
        checks_passed++;
        printf("  [PASS] %s\n", what);
    } else {
        checks_failed++;
        printf("  [FAIL] %s (got 0x%08x want 0x%08x)\n", what, got, want);
    }
}

static void chk_rc(int32_t got, int32_t want, const char *what)
{
    if (got == want) {
        checks_passed++;
        printf("  [PASS] %s\n", what);
    } else {
        checks_failed++;
        printf("  [FAIL] %s (rc=%d want %d)\n", what, (int)got, (int)want);
    }
}

/* =========================================================================
 * CRCs -- must agree bit-for-bit with km_firmware_handler
 * ========================================================================= */

/* CRC-8/ROHC: poly 0x07 reflected (0xE0), init 0xFF, no final xor. */
static uint8_t crc8_rohc(const uint8_t *d, unsigned n)
{
    uint8_t crc = 0xFFu;
    for (unsigned i = 0; i < n; i++) {
        crc ^= d[i];
        for (int b = 0; b < 8; b++)
            crc = (crc & 1u) ? (uint8_t)((crc >> 1) ^ 0xE0u) : (uint8_t)(crc >> 1);
    }
    return crc;
}

/* CRC-32C (Castagnoli): poly 0x82F63B78 reflected, init and final xor all-ones. */
static uint32_t crc32c_words(const uint32_t *w, unsigned n)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (unsigned i = 0; i < n; i++) {
        for (int b = 0; b < 4; b++) {
            crc ^= (uint8_t)((w[i] >> (b * 8)) & 0xFFu);
            for (int bit = 0; bit < 8; bit++)
                crc = (crc & 1u) ? ((crc >> 1) ^ 0x82F63B78u) : (crc >> 1);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

/* =========================================================================
 * Mailbox primitives
 * ========================================================================= */

/* Push one word, stalling while the inbound FIFO is full.  A frame wider than
 * KM_FIFO_DEPTH gets here repeatedly and only makes progress because the KM
 * drains concurrently while we spin. */
static int mb_put(uint32_t word, int last)
{
    mb_status_t s;
    unsigned guard = MB_GUARD;

    for (;;) {
        s.val = READ_REG(MB_STATUS);
        if (!s.f.inbound_full) break;
        if (--guard == 0) {
            printf("  mb_put: inbound stayed full\n");
            return -1;
        }
    }
    if (last) WRITE_REG(MB_WSEP, 1u);
    WRITE_REG(MB_WDATA, word);
    return 0;
}

static int mb_get(uint32_t *word)
{
    mb_status_t s;
    unsigned guard = MB_GUARD;

    for (;;) {
        s.val = READ_REG(MB_STATUS);
        if (!s.f.outbound_empty) break;
        if (--guard == 0) {
            printf("  mb_get: outbound stayed empty\n");
            return -1;
        }
    }
    *word = READ_REG(MB_RDATA);
    return 0;
}

/* =========================================================================
 * Framing
 * ========================================================================= */
static uint8_t km_seq;   /* next sequence number the KM will accept */

static uint32_t km_header(uint8_t seq, uint8_t id, uint8_t len)
{
    uint32_t h24 = (uint32_t)seq | ((uint32_t)id << 8) | ((uint32_t)len << 16);
    uint8_t  b[3];

    b[0] = (uint8_t)( h24        & 0xFFu);
    b[1] = (uint8_t)((h24 >> 8)  & 0xFFu);
    b[2] = (uint8_t)((h24 >> 16) & 0xFFu);
    return h24 | ((uint32_t)crc8_rohc(b, 3) << 24);
}

/*
 * Send one frame.  hdr_len is what the header advertises and n is how many
 * payload words are actually pushed; they differ only in the frame-length
 * negative test.  The two *_ok flags corrupt a CRC on purpose.
 */
static int km_send_ex(uint8_t seq, uint8_t id, uint8_t hdr_len,
                      const uint32_t *p, unsigned n,
                      int hdr_crc_ok, int pay_crc_ok)
{
    uint32_t hdr = km_header(seq, id, hdr_len);

    if (!hdr_crc_ok) hdr ^= 0x01000000u;

    /* A zero-length frame is just the header, and it carries the separator. */
    if (hdr_len == 0 && n == 0)
        return mb_put(hdr, 1);

    if (mb_put(hdr, 0)) return -1;
    for (unsigned i = 0; i < n; i++)
        if (mb_put(p[i], 0)) return -1;

    uint32_t crc = crc32c_words(p, n);
    if (!pay_crc_ok) crc ^= 0xFFFFFFFFu;
    return mb_put(crc, 1);
}

static int km_send(uint8_t id, const uint32_t *p, unsigned n)
{
    return km_send_ex(km_seq, id, (uint8_t)n, p, n, 1, 1);
}

typedef struct {
    uint32_t hdr;
    uint8_t  seq;
    uint8_t  id;
    uint8_t  len;
    uint8_t  crc8;
    uint32_t pay[8];
    unsigned npay;
    int      framing_ok;   /* separator seen where expected, payload CRC good */
} km_resp_t;

static int km_recv(km_resp_t *r)
{
    mb_status_t s;
    uint32_t    crc;

    if (mb_get(&r->hdr)) return -1;
    r->seq  = (uint8_t)( r->hdr        & 0xFFu);
    r->id   = (uint8_t)((r->hdr >> 8)  & 0xFFu);
    r->len  = (uint8_t)((r->hdr >> 16) & 0xFFu);
    r->crc8 = (uint8_t)((r->hdr >> 24) & 0xFFu);
    r->npay = 0;

    {   /* the header CRC the KM computed must match what we compute */
        uint8_t b[3];
        b[0] = (uint8_t)( r->hdr        & 0xFFu);
        b[1] = (uint8_t)((r->hdr >> 8)  & 0xFFu);
        b[2] = (uint8_t)((r->hdr >> 16) & 0xFFu);
        r->framing_ok = (crc8_rohc(b, 3) == r->crc8);
    }

    if (r->len == 0) {
        s.val = READ_REG(MB_STATUS);
        r->framing_ok = r->framing_ok && s.f.outbound_separator;
        return 0;
    }
    if (r->len > 8) {
        printf("  km_recv: response payload %u words is too wide\n", r->len);
        return -1;
    }
    for (unsigned i = 0; i < r->len; i++) {
        if (mb_get(&r->pay[i])) return -1;
        r->npay++;
    }
    if (mb_get(&crc)) return -1;

    s.val = READ_REG(MB_STATUS);
    r->framing_ok = r->framing_ok
                 && s.f.outbound_separator
                 && (crc == crc32c_words(r->pay, r->len));
    return 0;
}

/*
 * Send a command and collect its RESP_CMD.  km_seq advances because the KM
 * consumes the sequence number for any frame that clears the header CRC and
 * sequence checks, whatever the command then returns.
 */
static int km_cmd(uint8_t id, const uint32_t *p, unsigned n,
                  int32_t *rc, uint32_t *arg)
{
    km_resp_t r;

    if (km_send(id, p, n)) return -1;
    if (km_recv(&r)) return -1;
    km_seq++;

    if (r.id != RESP_CMD || r.npay < 3) {
        printf("  km_cmd: expected RESP_CMD, got id=0x%02x len=%u\n", r.id, r.len);
        return -1;
    }
    if (!r.framing_ok) {
        printf("  km_cmd: response framing bad (hdr=0x%08x)\n", r.hdr);
        return -1;
    }
    *rc  = (int32_t)r.pay[2];
    *arg = (r.npay >= 4) ? r.pay[3] : 0u;
    return 0;
}

/* Load a key and return its handle, or -1. */
static int km_key_load(const uint32_t *key, unsigned words, uint8_t dest_valid)
{
    static uint32_t p[2 + KM_MAX_KEY_WORDS];
    int32_t  rc;
    uint32_t arg;

    p[0] = words - 1u;          /* KEY_SIZE is word-count-minus-one */
    p[1] = dest_valid;
    for (unsigned i = 0; i < words; i++)
        p[2 + i] = key[i];

    if (km_cmd(CMD_KEY_LOAD, p, words + 2u, &rc, &arg)) return -1;
    if (rc != RET_SUCCESS) {
        printf("  km_key_load(%u words): rc=%d\n", words, (int)rc);
        return -1;
    }
    return (int)(arg & 0xFFu);
}

/* =========================================================================
 * Phase 1 -- boot handshake
 * ========================================================================= */
static void phase1_boot(void)
{
    mb_status_t s;
    km_resp_t   r;
    uint32_t    rst;

    printf("\n=== Phase 1: boot handshake ===\n");

    /* SW_RESET_N resets to 0x1E: the crypto engines come up released but the KM
     * is held in software reset, so SEP has to let it go before it will boot. */
    rst = READ_REG(SEP_RESET_CTRL_SW_RESET_N_REG_ADDR);
    chk_eq(rst & SW_RESET_N_KM, 0u, "KM starts held in software reset");

    s.val = READ_REG(MB_STATUS);
    chk(s.f.outbound_empty, "no boot message while the KM is in reset");
    chk(s.f.inbound_empty,  "inbound FIFO starts empty");

    WRITE_REG(SEP_RESET_CTRL_SW_RESET_N_REG_ADDR, rst | SW_RESET_N_KM);
    chk_eq(READ_REG(SEP_RESET_CTRL_SW_RESET_N_REG_ADDR) & SW_RESET_N_KM,
           SW_RESET_N_KM, "km_sw_rst_n reads back released");

    if (km_recv(&r)) {
        chk(0, "boot message readable");
        return;
    }
    chk_eq(r.hdr, KM_READY_HEADER, "boot header is byte-exact RESP_KM_READY");
    chk_eq(r.id,  RESP_KM_READY,   "response id is 0x55");
    chk_eq(r.seq, 0u,              "boot message carries seq 0");
    chk_eq(r.len, 0u,              "boot message has no payload");
    chk(r.framing_ok,              "boot header CRC-8 and separator are correct");

    s.val = READ_REG(MB_STATUS);
    chk(s.f.outbound_empty, "outbound FIFO drains to empty after the boot message");
}

/* =========================================================================
 * Phase 2 -- command round-trip
 * ========================================================================= */
static void phase2_roundtrip(void)
{
    km_resp_t r;

    printf("\n=== Phase 2: command round-trip ===\n");

    if (km_send(CMD_STAT, 0, 0)) { chk(0, "CMD_STAT sent"); return; }
    if (km_recv(&r))             { chk(0, "CMD_STAT answered"); return; }

    chk_eq(r.id, RESP_CMD, "CMD_STAT gets a RESP_CMD");
    chk(r.framing_ok,      "response CRC-8, CRC-32C and separator all check out");
    chk(r.npay >= 3,       "RESP_CMD carries at least seq, cmd and rc");

    if (r.npay >= 3) {
        chk_eq(r.pay[0], km_seq,   "response echoes the command sequence number");
        chk_eq(r.pay[1], CMD_STAT, "response echoes the command id");
        chk_rc((int32_t)r.pay[2], RET_SUCCESS, "CMD_STAT returns RET_SUCCESS");
    }
    km_seq++;
}

/* =========================================================================
 * Phase 3 -- protocol negatives
 * ========================================================================= */
static void phase3_negatives(void)
{
    km_resp_t r;
    int32_t   rc;
    uint32_t  arg;
    uint32_t  p[2];

    printf("\n=== Phase 3: protocol negatives ===\n");

    /* Bad header CRC. The KM must not consume the sequence number. */
    if (!km_send_ex(km_seq, CMD_STAT, 0, 0, 0, 0, 1) && !km_recv(&r) && r.npay >= 3)
        chk_rc((int32_t)r.pay[2], RET_HEADER_CRC, "corrupt header CRC -> RET_HEADER_CRC");
    else
        chk(0, "corrupt header CRC answered");

    /* Wrong sequence number. Also must not consume it. */
    if (!km_send_ex((uint8_t)(km_seq + 7u), CMD_STAT, 0, 0, 0, 1, 1)
        && !km_recv(&r) && r.npay >= 3)
        chk_rc((int32_t)r.pay[2], RET_CMD_NOSEQ, "wrong sequence -> RET_CMD_NOSEQ");
    else
        chk(0, "wrong sequence answered");

    /* The two checks above must have left the sequence untouched. */
    if (!km_cmd(CMD_STAT, 0, 0, &rc, &arg))
        chk_rc(rc, RET_SUCCESS, "sequence survived the two rejected frames");
    else
        chk(0, "sequence survived the two rejected frames");

    /* Unknown command id. */
    if (!km_cmd(0x7Fu, 0, 0, &rc, &arg))
        chk_rc(rc, RET_INVALID_CMD, "unknown command id -> RET_INVALID_CMD");
    else
        chk(0, "unknown command id answered");

    /* Commands retired along with the KPVLP window. */
    if (!km_cmd(CMD_RETIRED_SLOT_REQ, 0, 0, &rc, &arg))
        chk_rc(rc, RET_INVALID_CMD, "retired 0x20 -> RET_INVALID_CMD");
    else
        chk(0, "retired 0x20 answered");

    if (!km_cmd(CMD_RETIRED_KEY_REG, 0, 0, &rc, &arg))
        chk_rc(rc, RET_INVALID_CMD, "retired 0x21 -> RET_INVALID_CMD");
    else
        chk(0, "retired 0x21 answered");

    /* Fixed-length command given the wrong payload length. */
    p[0] = 0u;
    p[1] = 0u;
    if (!km_cmd(CMD_STAT, p, 2, &rc, &arg))
        chk_rc(rc, RET_INVALID_LEN, "CMD_STAT with a payload -> RET_INVALID_LEN");
    else
        chk(0, "CMD_STAT with a payload answered");

    /* Header advertises three payload words but only one is pushed. */
    p[0] = 0xA5A5A5A5u;
    if (!km_send_ex(km_seq, CMD_STAT, 3, p, 1, 1, 1) && !km_recv(&r) && r.npay >= 3) {
        km_seq++;
        chk_rc((int32_t)r.pay[2], RET_INVALID_LEN, "short frame -> RET_INVALID_LEN");
    } else {
        chk(0, "short frame answered");
    }

    /* Corrupt payload CRC-32C. */
    p[0] = 0u;
    p[1] = DEST_HMAC;
    if (!km_send_ex(km_seq, CMD_KEY_TRANSFER, 2, p, 2, 1, 0)
        && !km_recv(&r) && r.npay >= 3) {
        km_seq++;
        chk_rc((int32_t)r.pay[2], RET_PAYLOAD_CRC, "corrupt payload CRC -> RET_PAYLOAD_CRC");
    } else {
        chk(0, "corrupt payload CRC answered");
    }
}

/* =========================================================================
 * Phase 4 -- CMD_KEY_LOAD
 * ========================================================================= */

/* The key phase 6 hashes with; 8 words is exactly the HMAC sideload width. */
static const uint32_t hmac_key[8] = {
    0xA5A5A5A5u, 0x5A5A5A5Au, 0xDEADBEEFu, 0xCAFEBABEu,
    0x01234567u, 0x89ABCDEFu, 0xFEDCBA98u, 0x76543210u
};

static int hmac_handle = -1;

static void phase4_key_load(void)
{
    int32_t  rc;
    uint32_t arg;
    uint32_t p[2 + 8];

    printf("\n=== Phase 4: CMD_KEY_LOAD ===\n");

    p[0] = 8u - 1u;
    p[1] = DEST_HMAC;
    for (unsigned i = 0; i < 8; i++) p[2 + i] = hmac_key[i];

    if (km_cmd(CMD_KEY_LOAD, p, 10, &rc, &arg)) {
        chk(0, "CMD_KEY_LOAD answered");
        return;
    }
    chk_rc(rc, RET_SUCCESS, "8-word CMD_KEY_LOAD succeeds");
    chk((arg & 0xFFu) != 0u,           "a non-zero handle comes back");
    chk_eq((arg >> 8) & 0x7Fu, 8u - 1u, "return arg echoes KEY_SIZE");
    chk_eq((arg >> 16) & 0xFFu, DEST_HMAC, "return arg echoes DEST_VALID");
    hmac_handle = (int)(arg & 0xFFu);

    /* DEST_VALID of zero would make the key unusable, so it is refused, and the
     * offending word index comes back in the return argument. */
    p[1] = 0u;
    if (!km_cmd(CMD_KEY_LOAD, p, 10, &rc, &arg)) {
        chk_rc(rc, RET_INVALID_ARG, "DEST_VALID=0 -> RET_INVALID_ARG");
        chk_eq(arg, 1u, "return arg points at payload word 1");
    } else {
        chk(0, "DEST_VALID=0 answered");
    }

    /* KEY_SIZE reserved bits must be clear. */
    p[0] = 0xFFFFFF00u;
    p[1] = DEST_HMAC;
    if (!km_cmd(CMD_KEY_LOAD, p, 10, &rc, &arg))
        chk_rc(rc, RET_INVALID_ARG, "KEY_SIZE reserved bits set -> RET_INVALID_ARG");
    else
        chk(0, "KEY_SIZE reserved bits answered");

    /* Frame that does not carry the key length it claims. */
    p[0] = 8u - 1u;
    if (!km_cmd(CMD_KEY_LOAD, p, 6, &rc, &arg))
        chk_rc(rc, RET_INVALID_ARG, "KEY_SIZE disagrees with the frame -> RET_INVALID_ARG");
    else
        chk(0, "KEY_SIZE/frame mismatch answered");
}

/* =========================================================================
 * Phase 5 -- frames wider than the FIFO
 * ========================================================================= */
static void phase5_reassembly(void)
{
    static uint32_t key[KM_MAX_KEY_WORDS];
    unsigned sizes[3] = { 17u, 32u, KM_MAX_KEY_WORDS };

    printf("\n=== Phase 5: frames wider than the %u-word FIFO ===\n", KM_FIFO_DEPTH);

    for (unsigned s = 0; s < 3; s++) {
        unsigned words = sizes[s];
        unsigned frame = 1u + 2u + words + 1u;   /* header + args + key + CRC */

        for (unsigned i = 0; i < words; i++)
            key[i] = 0x10000000u + (i * 0x01010101u);

        printf("  %u-word key -> %u-word frame (%ux the FIFO)\n",
               words, frame, frame / KM_FIFO_DEPTH);

        int h = km_key_load(key, words, DEST_KMAC);
        if (h < 0) {
            chk(0, "wide key loaded");
            continue;
        }
        chk(h > 0, "wide key loaded and returned a handle");
    }

    /* A key one word past the maximum has no valid KEY_SIZE encoding. */
    {
        int32_t  rc;
        uint32_t arg;
        static uint32_t p[2 + KM_MAX_KEY_WORDS + 1];

        p[0] = KM_MAX_KEY_WORDS;      /* encodes 129 words */
        p[1] = DEST_KMAC;
        for (unsigned i = 0; i <= KM_MAX_KEY_WORDS; i++) p[2 + i] = i;

        if (!km_cmd(CMD_KEY_LOAD, p, KM_MAX_KEY_WORDS + 3u, &rc, &arg))
            chk(rc != RET_SUCCESS, "a key past the 128-word maximum is refused");
        else
            chk(0, "oversized key answered");
    }
}

/* =========================================================================
 * Phase 6 -- sideload into HMAC and hash
 * ========================================================================= */

/* HMAC-SHA256 over "abc" with hmac_key, computed on the host. */
static const uint32_t expect_digest[8] = {
    0x55f1d6e0u, 0x8f794ccbu, 0xadf40ce9u, 0xdbaeb255u,
    0x8e4367acu, 0x91a6f600u, 0xc62b49ffu, 0xabe6d7f0u
};

static int hmac_sha256_abc(uint32_t digest[8])
{
    HMAC_CFG_reg_u          cfg;
    HMAC_CMD_reg_u          cmd;
    HMAC_STATUS_reg_u       sts;
    HMAC_INTR_STATE_reg_u   intr;
    volatile uint8_t       *fifo8 = (volatile uint8_t *)(uintptr_t)HMAC_MSG_FIFO_MEM_BASE_ADDR;
    const char             *msg   = "abc";
    int                     spins;

    cfg.val = 0;
    cfg.f.hmac_en     = 1;      /* keyed: takes the sideloaded key */
    cfg.f.sha_en      = 1;
    cfg.f.endian_swap = 0;
    cfg.f.digest_swap = 0;
    cfg.f.digest_size = 0x1;    /* SHA2_256 */
    cfg.f.key_length  = 0x2;    /* 256-bit */
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg.val);

    cmd.val = 0;
    cmd.f.hash_start = 1;
    WRITE_REG(HMAC_CMD_REG_ADDR, cmd.val);

    for (unsigned i = 0; i < 3u; i++) {
        spins = 0;
        for (;;) {
            sts.val = READ_REG(HMAC_STATUS_REG_ADDR);
            if (!sts.f.fifo_full) break;
            if (++spins > 100000) { printf("  HMAC FIFO stayed full\n"); return -1; }
        }
        *fifo8 = (uint8_t)msg[i];
    }

    cmd.val = 0;
    cmd.f.hash_process = 1;
    WRITE_REG(HMAC_CMD_REG_ADDR, cmd.val);

    spins = 0;
    for (;;) {
        intr.val = READ_REG(HMAC_INTR_STATE_REG_ADDR);
        sts.val  = READ_REG(HMAC_STATUS_REG_ADDR);
        if (intr.f.hmac_done || sts.f.hmac_idle) break;
        if (++spins > 1000000) { printf("  HMAC never completed\n"); return -1; }
    }
    intr.val = READ_REG(HMAC_INTR_STATE_REG_ADDR);
    if (intr.f.hmac_done) {
        HMAC_INTR_STATE_reg_u clr = {.val = 0};
        clr.f.hmac_done = 1;
        WRITE_REG(HMAC_INTR_STATE_REG_ADDR, clr.val);
    }

    /* DIGEST_n already holds the big-endian digest word, so it compares
     * directly against the host-computed vector. */
    for (int i = 0; i < 8; i++)
        digest[i] = READ_REG(HMAC_DIGEST_0__REG_ADDR + (i * 4));

    cfg.val = READ_REG(HMAC_CFG_REG_ADDR);
    cfg.f.sha_en = 0;
    WRITE_REG(HMAC_CFG_REG_ADDR, cfg.val);
    return 0;
}

static void phase6_sideload(void)
{
    uint32_t digest[8];
    uint32_t p[2];
    int32_t  rc;
    uint32_t arg;
    int      match;

    printf("\n=== Phase 6: sideload into HMAC ===\n");

    if (hmac_handle < 0) {
        chk(0, "phase 4 produced a handle to transfer");
        return;
    }

    p[0] = (uint32_t)hmac_handle;
    p[1] = DEST_HMAC;
    if (km_cmd(CMD_KEY_TRANSFER, p, 2, &rc, &arg)) {
        chk(0, "CMD_KEY_TRANSFER answered");
        return;
    }
    chk_rc(rc, RET_SUCCESS, "CMD_KEY_TRANSFER to HMAC succeeds");

    /* Once the sideload key is valid the software key registers are ignored,
     * so a stray write here must not disturb the digest below. */
    WRITE_REG(HMAC_KEY_0__REG_ADDR, 0xFFFFFFFFu);

    if (hmac_sha256_abc(digest)) {
        chk(0, "HMAC-SHA256 completed with the sideloaded key");
        return;
    }

    match = 1;
    for (int i = 0; i < 8; i++)
        if (digest[i] != expect_digest[i]) match = 0;

    printf("  digest   %08x%08x%08x%08x%08x%08x%08x%08x\n",
           digest[0], digest[1], digest[2], digest[3],
           digest[4], digest[5], digest[6], digest[7]);
    printf("  expected %08x%08x%08x%08x%08x%08x%08x%08x\n",
           expect_digest[0], expect_digest[1], expect_digest[2], expect_digest[3],
           expect_digest[4], expect_digest[5], expect_digest[6], expect_digest[7]);

    chk(match, "sideloaded key reconstructs from XOR shares and hashes correctly");
}

/* =========================================================================
 * Phase 7 -- policy and lifecycle
 * ========================================================================= */
static void phase7_policy(void)
{
    uint32_t p[2];
    int32_t  rc;
    uint32_t arg;
    int      h;

    printf("\n=== Phase 7: policy and lifecycle ===\n");

    /* A key restricted to HMAC must not reach a different engine. */
    h = km_key_load(hmac_key, 8, DEST_HMAC);
    if (h < 0) {
        chk(0, "key loaded for the destination-policy check");
    } else {
        p[0] = (uint32_t)h;
        p[1] = DEST_AES;
        if (!km_cmd(CMD_KEY_TRANSFER, p, 2, &rc, &arg))
            chk(rc != RET_SUCCESS, "transfer to a destination outside DEST_VALID is refused");
        else
            chk(0, "out-of-policy transfer answered");
    }

    /* A handle is single-use: the vault locks the slot after the first transfer. */
    h = km_key_load(hmac_key, 8, DEST_KMAC);
    if (h < 0) {
        chk(0, "key loaded for the single-use check");
    } else {
        p[0] = (uint32_t)h;
        p[1] = DEST_KMAC;
        if (!km_cmd(CMD_KEY_TRANSFER, p, 2, &rc, &arg))
            chk_rc(rc, RET_SUCCESS, "first transfer of a handle succeeds");
        else
            chk(0, "first transfer answered");

        if (!km_cmd(CMD_KEY_TRANSFER, p, 2, &rc, &arg))
            chk(rc != RET_SUCCESS, "second transfer of the same handle is refused");
        else
            chk(0, "second transfer answered");
    }

    /* Revoked keys cannot be transferred, and the handle is not recycled. */
    h = km_key_load(hmac_key, 8, DEST_HMAC);
    if (h < 0) {
        chk(0, "key loaded for the revoke check");
    } else {
        p[0] = (uint32_t)h;
        if (!km_cmd(CMD_KEY_REVOKE, p, 1, &rc, &arg))
            chk_rc(rc, RET_SUCCESS, "CMD_KEY_REVOKE succeeds");
        else
            chk(0, "CMD_KEY_REVOKE answered");

        p[1] = DEST_HMAC;
        if (!km_cmd(CMD_KEY_TRANSFER, p, 2, &rc, &arg))
            chk(rc != RET_SUCCESS, "transferring a revoked handle is refused");
        else
            chk(0, "revoked transfer answered");

        int h2 = km_key_load(hmac_key, 8, DEST_HMAC);
        chk(h2 > 0 && h2 != h, "handles advance monotonically and are never reused");
    }

    /* Shredding zeroes the engine sideload key. */
    p[0] = DEST_HMAC;
    if (!km_cmd(CMD_ENGINE_SHRED, p, 1, &rc, &arg))
        chk_rc(rc, RET_SUCCESS, "CMD_ENGINE_SHRED succeeds");
    else
        chk(0, "CMD_ENGINE_SHRED answered");
}

/* =========================================================================
 * Phase 8 -- documented stubs
 * ========================================================================= */
static void phase8_stubs(void)
{
    uint32_t p[1];
    int32_t  rc;
    uint32_t arg;

    printf("\n=== Phase 8: documented stubs ===\n");

    /* Latching ROM-only operation is something the model genuinely does, and it
     * is what turns the SRAM handover commands below into a definite refusal
     * rather than an unknown command. */
    if (!km_cmd(CMD_EXEC_ROM, 0, 0, &rc, &arg))
        chk_rc(rc, RET_SUCCESS, "CMD_EXEC_ROM latches ROM-only operation");
    else
        chk(0, "CMD_EXEC_ROM answered");

    /* Mutable KM firmware is not modelled: the VP has no instruction-set
     * simulator for the KM core, so these must refuse rather than pretend. */

    if (!km_cmd(CMD_SRAM_EXEC, 0, 0, &rc, &arg))
        chk(rc != RET_SUCCESS, "CMD_SRAM_EXEC refuses (no mutable firmware model)");
    else
        chk(0, "CMD_SRAM_EXEC answered");

    p[0] = 0u;
    if (!km_cmd(CMD_SRAM_LOAD_EXEC, p, 1, &rc, &arg))
        chk(rc != RET_SUCCESS, "CMD_SRAM_LOAD_EXEC refuses (no mutable firmware model)");
    else
        chk(0, "CMD_SRAM_LOAD_EXEC answered");

    /* Adams Bridge has no VP model, so there is no shared key to ingest. */
    if (!km_cmd(CMD_ABR_SK_TRANSFER, p, 1, &rc, &arg))
        chk(rc != RET_SUCCESS, "CMD_ABR_SK_TRANSFER refuses (Adams Bridge stubbed)");
    else
        chk(0, "CMD_ABR_SK_TRANSFER answered");
}

/* =========================================================================
 * main
 * ========================================================================= */
int main(void)
{
    printf("========================================\n");
    printf("SEP Key Manager mailbox test\n");
    printf("========================================\n");
    printf("KM mailbox : 0x%08x\n", KM_MAILBOX_SEP_REG_MAP_BASE_ADDR);
    printf("HMAC       : 0x%08x\n", HMAC_REG_MAP_BASE_ADDR);
    printf("FIFO depth : %u words\n", KM_FIFO_DEPTH);

    phase1_boot();
    phase2_roundtrip();
    phase3_negatives();
    phase4_key_load();
    phase5_reassembly();
    phase6_sideload();
    phase7_policy();
    phase8_stubs();

    printf("\n========================================\n");
    printf("Passed: %u\n", checks_passed);
    printf("Failed: %u\n", checks_failed);
    printf("========================================\n");

    if (checks_failed == 0)
        printf("\nAll checks PASSED!\n");
    else
        printf("\nSome checks FAILED!\n");

    while (1) {
        __asm__("wfi");
    }
    return 0;
}
