// Self-checking tests for the SEP virtual-console decoder.
//
// Pure C++ (no SystemC) so the decode logic — the correctness oracle for the whole
// feature — is verifiable in isolation. Parity is on decoded message *content & order*
// with the cocotb reference decoder (sep_virt_console.py); the prefix differs by design
// (SIM_OUT here vs SEP_CONSOLE in cocotb) and is not compared.
//

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "virt_console_decoder.h"

using sep_virt_console::VirtConsoleDecoder;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        ++g_checks;                                                            \
        if (!(cond)) {                                                         \
            ++g_failures;                                                      \
            std::printf("  FAIL: %s  (%s:%d)\n", (msg), __FILE__, __LINE__);   \
        }                                                                      \
    } while (0)

// --- packed-word builders (mirror fw/sep/bootcode/include/rom_virt_console.h) ---

// ASCII word: opcode 0 in bits[3:1]; up to 3 bytes at [15:8],[23:16],[31:24].
static uint32_t ascii_word(const char* s, int n, uint32_t toggle = 0) {
    uint32_t w = 0u;  // OP_ASCII == 0
    int off = 1;
    for (int i = 0; i < n && i < 3; ++i) {
        w |= static_cast<uint32_t>(static_cast<uint8_t>(s[i])) << (8 * off++);
    }
    return w | (toggle & 1u);
}
static uint32_t hex16_word(uint16_t v, uint32_t toggle = 0) {
    return (static_cast<uint32_t>(v) << 8) | (1u << 1) | (toggle & 1u);  // op HEX16 == 1
}
static uint32_t dec24_word(uint32_t v, uint32_t toggle = 0) {
    return ((v & 0xFFFFFFu) << 8) | (2u << 1) | (toggle & 1u);           // op DEC24 == 2
}

// Feed an arbitrary string exactly as simputs() would: 3 chars per ASCII word.
static void feed_string(VirtConsoleDecoder& d, const std::string& s) {
    size_t i = 0;
    while (i < s.size()) {
        const int n = static_cast<int>(std::min<size_t>(3, s.size() - i));
        d.on_word(ascii_word(s.data() + i, n));
        i += static_cast<size_t>(n);
    }
}

static VirtConsoleDecoder make(std::vector<std::string>& sink) {
    VirtConsoleDecoder d([&sink](const std::string& line) { sink.push_back(line); });
    return d;
}

int main() {
    // --- ASCII single word ---
    {
        std::vector<std::string> out;
        auto d = make(out);
        feed_string(d, "OK\n");
        CHECK(out.size() == 1 && out[0] == "OK", "ascii single-word 'OK'");
    }

    // --- ASCII reassembly across multiple words ---
    {
        std::vector<std::string> out;
        auto d = make(out);
        feed_string(d, "DECRYPT_OK\n");
        CHECK(out.size() == 1 && out[0] == "DECRYPT_OK", "ascii reassembly 'DECRYPT_OK'");
    }

    // --- duplicate messages, 2nd with toggled bit[0], both emit ---
    {
        std::vector<std::string> out;
        auto d = make(out);
        const char* ab = "AB\n";
        d.on_word(ascii_word(ab, 3, /*toggle=*/0));
        d.on_word(ascii_word(ab, 3, /*toggle=*/1));  // bit[0] flipped — must be ignored
        CHECK(out.size() == 2 && out[0] == "AB" && out[1] == "AB", "duplicate AB twice");
    }

    // --- trailing buffer flushed at end ---
    {
        std::vector<std::string> out;
        auto d = make(out);
        feed_string(d, "PARTIAL");  // no newline
        CHECK(out.empty(), "no emit before flush");
        d.flush();
        CHECK(out.size() == 1 && out[0] == "PARTIAL", "flush emits trailing line");
    }

    // --- HEX16 ---
    {
        std::vector<std::string> out;
        auto d = make(out);
        d.on_word(hex16_word(0xF001));
        d.on_word(ascii_word("\n", 1));
        CHECK(out.size() == 1 && out[0] == "f001", "hex16 0xF001 -> f001");
    }

    // --- HEX32 == '0x' + two HEX16 (high half first) -> 0xdeadbeef ---
    {
        std::vector<std::string> out;
        auto d = make(out);
        feed_string(d, "0x");
        d.on_word(hex16_word(0xDEAD));
        d.on_word(hex16_word(0xBEEF));
        feed_string(d, "\n");
        CHECK(out.size() == 1 && out[0] == "0xdeadbeef", "hex32 -> 0xdeadbeef");
    }

    // --- DEC24 ---
    {
        std::vector<std::string> out;
        auto d = make(out);
        d.on_word(dec24_word(123));
        feed_string(d, "\n");
        CHECK(out.size() == 1 && out[0] == "123", "dec24 123 -> 123");
    }

    // --- labeled value 'X=0xdeadbeef' on one line ---
    {
        std::vector<std::string> out;
        auto d = make(out);
        feed_string(d, "X=0x");
        d.on_word(hex16_word(0xDEAD));
        d.on_word(hex16_word(0xBEEF));
        feed_string(d, "\n");
        CHECK(out.size() == 1 && out[0] == "X=0xdeadbeef", "labeled value one line");
    }

    // --- unknown opcode -> no output, no crash ---
    {
        std::vector<std::string> out;
        auto d = make(out);
        d.on_word((3u << 1) | (0xABu << 8));  // opcode 3 (reserved)
        d.on_word((7u << 1) | (0x55u << 8));  // opcode 7 (reserved)
        d.flush();
        CHECK(out.empty(), "unknown opcode produces no line");
    }

    // --- sub-word / short write -> ignored, no crash ---
    {
        std::vector<std::string> out;
        auto d = make(out);
        const uint8_t two[2] = {0x41, 0x00};
        d.on_bytes(two, 2);          // len < 4
        d.on_bytes(nullptr, 4);      // null pointer
        d.flush();
        CHECK(out.empty(), "sub-word/null write ignored");

        // ...but a full 4-byte LE write decodes normally.
        const uint32_t w = ascii_word("Hi\n", 3);
        const uint8_t le[4] = {static_cast<uint8_t>(w), static_cast<uint8_t>(w >> 8),
                               static_cast<uint8_t>(w >> 16), static_cast<uint8_t>(w >> 24)};
        d.on_bytes(le, 4);
        CHECK(out.size() == 1 && out[0] == "Hi", "full LE write decodes");
    }

    // --- enable=false produces no output ---
    {
        std::vector<std::string> out;
        auto d = make(out);
        d.set_enabled(false);
        feed_string(d, "SHOULD_NOT_APPEAR\n");
        d.on_word(hex16_word(0x1234));
        d.flush();
        CHECK(out.empty(), "disabled decoder emits nothing");
    }

    std::printf("\nvirt_console_decoder_test: %d checks, %d failures\n", g_checks, g_failures);
    if (g_failures == 0) {
        std::printf("ALL TESTS PASSED\n");
        return 0;
    }
    std::printf("TESTS FAILED\n");
    return 1;
}
