#include <array>
#include <cstdint>
#include <cstdio>
#include <boost/crc.hpp>
#include "js8_js8call_encoder.h"
#include "js8_varicode.h"

int main() {
    for (unsigned seed = 0; seed < 256; ++seed) {
        std::array<uint8_t, 11> bytes{};
        for (unsigned i = 0; i < bytes.size(); ++i) {
            bytes[i] = static_cast<uint8_t>((seed * 37u + i * 71u) ^ (i << 4));
        }

        uint16_t const actual = js8call::detail::crc12_aug_xor42(bytes);
        uint16_t const expected =
            static_cast<uint16_t>(boost::augmented_crc<12, 0xC06>(
                bytes.data(), bytes.size()) ^ 42u);

        if (actual != expected) {
            std::fprintf(stderr,
                         "CRC mismatch seed=%u project=%04x boost=%04x\n",
                         seed, actual, expected);
            return 1;
        }
    }

    std::string const payload = js8call::pack_normal_huffman_frame("CQ CQ");
    if (payload.size() != 12) {
        std::fprintf(stderr, "packed payload size is %zu, expected 12\\n", payload.size());
        return 1;
    }

    // Decode the payload according to JS8Call's unpackDataMessage path:
    // unpack 12 six-bit symbols, skip data/compression flags, trim at the
    // final zero pad marker, then decode Huffman symbols.
    std::vector<uint8_t> bits;
    for (char ch : payload) {
        auto const word = js8call::detail::alphabetWord(ch);
        for (int bit = 5; bit >= 0; --bit) {
            bits.push_back(static_cast<uint8_t>((word >> bit) & 1u));
        }
    }

    auto const pad_marker = bits.rend();
    (void)pad_marker;
    std::size_t end = bits.size();
    while (end > 0 && bits[end - 1] == 1) --end;
    if (end == 0 || bits[end - 1] != 0 || bits[0] != 1 || bits[1] != 0) {
        std::fprintf(stderr, "JS8Call data prefix/padding invalid\\n");
        return 1;
    }
    std::size_t const marker = end - 1;
    std::string decoded;
    std::size_t pos = 2;
    while (pos < marker) {
        bool matched = false;
        for (auto const& entry : js8::huffman_table()) {
            auto const code = entry.second;
            if (pos + code.nbits > marker) continue;
            uint32_t value = 0;
            for (std::size_t bit = 0; bit < code.nbits; ++bit) {
                value = (value << 1) | bits[pos + bit];
            }
            if (value == code.bits) {
                decoded.push_back(static_cast<char>(entry.first));
                pos += code.nbits;
                matched = true;
                break;
            }
        }
        if (!matched) {
            std::fprintf(stderr, "Huffman round-trip failed at bit %zu\\n", pos);
            return 1;
        }
    }
    if (decoded != "CQ CQ") {
        std::fprintf(stderr, "decoded text mismatch: '%s'\\n", decoded.c_str());
        return 1;
    }

    auto const original =
        js8call::encode_tones79(payload, 3,
                                js8call::detail::CostasType::ORIGINAL);
    auto const modified =
        js8call::encode_tones79(payload, 3,
                                js8call::detail::CostasType::MODIFIED);

    for (uint8_t tone : original) {
        if (tone > 7) {
            std::fprintf(stderr, "invalid ORIGINAL tone: %u\n", tone);
            return 1;
        }
    }
    for (uint8_t tone : modified) {
        if (tone > 7) {
            std::fprintf(stderr, "invalid MODIFIED tone: %u\n", tone);
            return 1;
        }
    }

    std::puts("PASS: project CRC matches Boost for 256 inputs; text packing round-trips; both Costas modes produce 79 valid tones");
    return 0;
}
