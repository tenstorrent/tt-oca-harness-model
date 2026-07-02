// Self-checking tests for the SEP production-status decoder + TSV name parser.
//
// Pure C++ (no SystemC) so the decode/parse logic — the correctness oracle for the
// whole feature — is verifiable in isolation. Tests assert the decoded line's *tokens*
// (stage, severity, value, name) rather than exact column padding, which is cosmetic.

#include <cstdint>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include "sep_status_decoder.h"

using sep_status_report::StatusDecoder;

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

// Encode a packed status word the way fw report_status() does.
static uint32_t status_word(uint8_t type, uint8_t fw_id, uint16_t value) {
    return (static_cast<uint32_t>(type) << 24) |
           (static_cast<uint32_t>(fw_id) << 16) |
           static_cast<uint32_t>(value);
}

// Split a decoded line into whitespace-separated tokens (ignores padding).
static std::vector<std::string> tokens(const std::string& line) {
    std::istringstream ss(line);
    std::vector<std::string> t;
    std::string w;
    while (ss >> w) t.push_back(w);
    return t;
}

int main() {
    // ----line content per type (no names loaded -> SEP_MSG_UNKNOWN) ----
    {
        std::vector<std::string> out;
        StatusDecoder d([&out](const std::string& l) { out.push_back(l); });

        d.on_word(status_word(0x01, 1, 0x0044));  // INFO  BL0
        d.on_word(status_word(0x0f, 1, 0x000e));  // ERROR BL0
        d.on_word(status_word(0x08, 2, 0x0069));  // WARN  BL1
        d.on_word(status_word(0x81, 2, 0x0220));  // INFO_EXT BL1

        CHECK(out.size() == 4, "four words -> four lines");
        auto t0 = tokens(out[0]);
        CHECK(t0.size() == 4 && t0[0] == "BL0" && t0[1] == "INFO" &&
              t0[2] == "0x0044" && t0[3] == "SEP_MSG_UNKNOWN", "INFO BL0 0x0044 unknown");
        auto t1 = tokens(out[1]);
        CHECK(t1[0] == "BL0" && t1[1] == "ERROR" && t1[2] == "0x000e", "ERROR BL0 0x000e");
        auto t2 = tokens(out[2]);
        CHECK(t2[0] == "BL1" && t2[1] == "WARN" && t2[2] == "0x0069", "WARN BL1 0x0069");
        auto t3 = tokens(out[3]);
        CHECK(t3[0] == "BL1" && t3[1] == "INFO_EXT" && t3[2] == "0x0220", "INFO_EXT BL1 0x0220");
    }

    // ----unknown type -> hex; unknown fw-id -> ID<n> ----
    {
        std::vector<std::string> out;
        StatusDecoder d([&out](const std::string& l) { out.push_back(l); });
        d.on_word(status_word(0x55, 5, 0x1234));  // type 0x55, fw-id 5
        auto t = tokens(out.at(0));
        CHECK(t[0] == "ID5" && t[1] == "T0x55" && t[2] == "0x1234", "unknown type/fw-id rendered");
    }

    // ----little-endian byte assembly; short/null writes ignored ----
    {
        std::vector<std::string> out;
        StatusDecoder d([&out](const std::string& l) { out.push_back(l); });
        const uint8_t two[2] = {0x44, 0x00};
        d.on_bytes(two, 2);       // len < 4 -> ignored
        d.on_bytes(nullptr, 4);   // null -> ignored
        CHECK(out.empty(), "sub-word / null write ignored");

        const uint32_t w = status_word(0x01, 1, 0x0044);
        const uint8_t le[4] = {static_cast<uint8_t>(w), static_cast<uint8_t>(w >> 8),
                               static_cast<uint8_t>(w >> 16), static_cast<uint8_t>(w >> 24)};
        d.on_bytes(le, 4);
        auto t = tokens(out.at(0));
        CHECK(out.size() == 1 && t[0] == "BL0" && t[1] == "INFO" && t[2] == "0x0044",
              "full LE write decodes");
    }

    // ----disabled decoder emits nothing ----
    {
        std::vector<std::string> out;
        StatusDecoder d([&out](const std::string& l) { out.push_back(l); });
        d.set_enabled(false);
        d.on_word(status_word(0x01, 1, 0x0044));
        d.on_bytes(reinterpret_cast<const uint8_t*>("\x44\x00\x01\x01"), 4);
        CHECK(out.empty(), "disabled decoder emits nothing");
    }

    // ----TSV parsing (comments / blank lines / data rows / malformed) ----
    {
        const std::string tsv =
            "/*\n"
            " * Flat enumeration of status values.\n"
            " * Generated into status_values.h\n"
            " */\n"
            "\n"
            "// -- Error codes --\n"
            "SEP_MSG_INVALID_SIGNATURE   0x0e\n"
            "SEP_MSG_BOOTROM_START       0x44\n"
            "   \n"                          // blank/whitespace
            "garbage_without_value\n"        // malformed -> skip
            "SEP_MSG_NOT_HEX             0xZZ\n"  // bad hex -> skip
            "SEP_MSG_INFO_OVERFLOW       0xfffe\n";
        auto map = StatusDecoder::parse_tsv_string(tsv);
        CHECK(map.size() == 3, "tsv parses 3 valid rows, skips comments/blank/malformed");
        CHECK(map[0x000e] == "SEP_MSG_INVALID_SIGNATURE", "0x0e name");
        CHECK(map[0x0044] == "SEP_MSG_BOOTROM_START", "0x44 name");
        CHECK(map[0xfffe] == "SEP_MSG_INFO_OVERFLOW", "0xfffe name");
        CHECK(map.find(0x1234) == map.end(), "malformed/non-hex rows not inserted");
    }

    // ----empty / whitespace-only input -> empty map ----
    {
        CHECK(StatusDecoder::parse_tsv_string("").empty(), "empty input -> empty map");
        CHECK(StatusDecoder::parse_tsv_string("   \n\t\n").empty(), "whitespace -> empty map");
        CHECK(StatusDecoder::parse_tsv_string("// only a comment\n").empty(),
              "comment-only -> empty map");
    }

    // ----name resolution end-to-end (hit -> name, miss -> UNKNOWN) ----
    {
        std::vector<std::string> out;
        StatusDecoder d([&out](const std::string& l) { out.push_back(l); });
        d.set_names(StatusDecoder::parse_tsv_string("SEP_MSG_BOOTROM_START 0x44\n"));
        d.on_word(status_word(0x01, 1, 0x0044));  // known
        d.on_word(status_word(0x01, 1, 0x4242));  // unknown value
        auto k = tokens(out.at(0));
        auto u = tokens(out.at(1));
        CHECK(k[3] == "SEP_MSG_BOOTROM_START", "known value resolves to name");
        CHECK(u[3] == "SEP_MSG_UNKNOWN", "unknown value -> SEP_MSG_UNKNOWN");
    }

    std::printf("\nsep_status_decoder_test: %d checks, %d failures\n", g_checks, g_failures);
    if (g_failures == 0) {
        std::printf("ALL TESTS PASSED\n");
        return 0;
    }
    std::printf("TESTS FAILED\n");
    return 1;
}
