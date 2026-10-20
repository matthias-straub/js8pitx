#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace js8call {

// JS8Call-compatible encoder extracted/ported from https://github.com/js8call/js8call
// (see JS8.cpp / JS8.hpp). This is the encoder that produces JS8 tones (79 symbols)
// which stock JS8Call should decode, assuming correct timing/frequency.
namespace detail {

constexpr std::string_view kAlphabet =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-+";
static_assert(kAlphabet.size() == 64, "JS8 alphabet must be 64 symbols");

inline uint8_t alphabetWord(char c) {
    for (size_t i = 0; i < kAlphabet.size(); ++i) {
        if (kAlphabet[i] == c) return static_cast<uint8_t>(i);
    }
    throw std::runtime_error("Invalid character for JS8Call alphabet");
}

// CRC12 equivalent to JS8Call's
// boost::augmented_crc<12, 0xC06>(bytes) ^ 42.
inline uint16_t crc12_aug_xor42(std::array<uint8_t, 11> const& bytes) {
    constexpr uint32_t poly = 0xC06;  // Truncated polynomial for width 12.
    uint32_t remainder = 0;

    // Match Boost's augmented_crc_modulo_word_update exactly: consume each
    // byte MSB-first, shift in the next input bit, then divide by the
    // truncated polynomial when the previous remainder's top bit was set.
    for (uint8_t byte : bytes) {
        for (int bit = 7; bit >= 0; --bit) {
            uint32_t const quotient = (remainder >> 11) & 1u;
            remainder = (remainder << 1) | ((byte >> bit) & 1u);
            remainder ^= quotient ? poly : 0u;
            remainder &= 0xFFFu;
        }
    }

    return static_cast<uint16_t>(remainder ^ 42u);
}

// Costas arrays (from JS8.hpp)
enum class CostasType { ORIGINAL = 0, MODIFIED = 1 };

constexpr std::array<std::array<std::array<int, 7>, 3>, 2> kCostas = {{
    // ORIGINAL
    {{
        {{4, 2, 5, 6, 1, 3, 0}},
        {{4, 2, 5, 6, 1, 3, 0}},
        {{4, 2, 5, 6, 1, 3, 0}},
    }},
    // MODIFIED
    {{
        {{0, 6, 2, 3, 5, 4, 1}},
        {{1, 5, 0, 2, 3, 6, 4}},
        {{2, 5, 0, 6, 4, 1, 3}},
    }},
}};

// Parity matrix, harvested from JS8Call JS8.cpp (952 bytes of hex, 87 rows x 87 cols bits).
// We keep it as the same 87 hex strings as upstream, and expose a parity(row,col) bit accessor.
constexpr std::array<std::string_view, 87> kParityHex = {
    "23bba830e23b6b6f50982e", "1f8e55da218c5df3309052", "ca7b3217cd92bd59a5ae20",
    "56f78313537d0f4382964e", "6be396b5e2e819e373340c", "293548a138858328af4210",
    "cb6c6afcdc28bb3f7c6e86", "3f2a86f5c5bd225c961150", "849dd2d63673481860f62c",
    "56cdaec6e7ae14b43feeee", "04ef5cfa3766ba778f45a4", "c525ae4bd4f627320a3974",
    "41fd9520b2e4abeb2f989c", "7fb36c24085a34d8c1dbc4", "40fc3e44bb7d2bb2756e44",
    "d38ab0a1d2e52a8ec3bc76", "3d0f929ef3949bd84d4734", "45d3814f504064f80549ae",
    "f14dbf263825d0bd04b05e", "db714f8f64e8ac7af1a76e", "8d0274de71e7c1a8055eb0",
    "51f81573dd4049b082de14", "d8f937f31822e57c562370", "b6537f417e61d1a7085336",
    "ecbd7c73b9cd34c3720c8a", "3d188ea477f6fa41317a4e", "1ac4672b549cd6dba79bcc",
    "a377253773ea678367c3f6", "0dbd816fba1543f721dc72", "ca4186dd44c3121565cf5c",
    "29c29dba9c545e267762fe", "1616d78018d0b4745ca0f2", "fe37802941d66dde02b99c",
    "a9fa8e50bcb032c85e3304", "83f640f1a48a8ebc0443ea", "3776af54ccfbae916afde6",
    "a8fc906976c35669e79ce0", "f08a91fb2e1f78290619a8", "cc9da55fe046d0cb3a770c",
    "d36d662a69ae24b74dcbd8", "40907b01280f03c0323946", "d037db825175d851f3af00",
    "1bf1490607c54032660ede", "0af7723161ec223080be86", "eca9afa0f6b01d92305edc",
    "7a8dec79a51e8ac5388022", "9059dfa2bb20ef7ef73ad4", "6abb212d9739dfc02580f2",
    "f6ad4824b87c80ebfce466", "d747bfc5fd65ef70fbd9bc", "612f63acc025b6ab476f7c",
    "05209a0abb530b9e7e34b0", "45b7ab6242b77474d9f11a", "6c280d2a0523d9c4bc5946",
    "f1627701a2d692fd9449e6", "8d9071b7e7a6a2eed6965e", "bf4f56e073271f6ab4bf80",
    "c0fc3ec4fb7d2bb2756644", "57da6d13cb96a7689b2790", "a9fa2eefa6f8796a355772",
    "164cc861bdd803c547f2ac", "cc6de59755420925f90ed2", "a0c0033a52ab6299802fd2",
    "b274db8abd3c6f396ea356", "97d4169cb33e7435718d90", "81cfc6f18c35b1e1f17114",
    "481a2a0df8a23583f82d6c", "081c29a10d468ccdbcecb6", "2c4142bf42b01e71076acc",
    "a6573f3dc8b16c9d19f746", "c87af9a5d5206abca532a8", "012dee2198eba82b19a1da",
    "b1ca4ea2e3d173bad4379c", "b33ec97be83ce413f9acc8", "5b0f7742bca86b8012609a",
    "37d8e0af9258b9e8c5f9b2", "35ad3fb0faeb5f1b0c30dc", "6114e08483043fd3f38a8a",
    "cd921fdf59e882683763f6", "95e45ecd0135aca9d6e6ae", "2e547dd7a05f6597aac516",
    "14cd0f642fc0c5fe3a65ca", "3a0a1dfd7eee29c2e827e0", "c8b5dffc335095dcdcaf2a",
    "3dd01a59d86310743ec752", "8abdb889efbe39a510a118", "3f231f212055371cf3e2a2",
};

constexpr uint8_t hexNibble(char c) {
    return (c >= '0' && c <= '9') ? (uint8_t)(c - '0')
         : (c >= 'a' && c <= 'f') ? (uint8_t)(c - 'a' + 10)
         : (c >= 'A' && c <= 'F') ? (uint8_t)(c - 'A' + 10)
         : 0;
}

inline bool parityBit(size_t row, size_t col) {
    // Row-major bits. Each row is 87 bits; encoded as hex nibbles MSB->LSB within each nibble.
    // This matches the JS8Call C++ port in JS8.cpp.
    const auto& s = kParityHex[row];
    size_t bitIndex = col;
    size_t nib = bitIndex / 4;
    size_t off = bitIndex % 4;
    if (nib >= s.size()) return false;
    uint8_t v = hexNibble(s[nib]);
    uint8_t mask = (uint8_t)(0x8u >> off);
    return (v & mask) != 0;
}

}  // namespace detail

// Encode 12-char JS8 message (JS8Call alphabet) + type (3-bit) into 79 tones (0..7).
// For JS8A/NORMAL, costas type is ORIGINAL. Other submodes use MODIFIED.
inline std::array<uint8_t, 79> encode_tones79(std::string const& msg12,
                                              uint8_t type3bit,
                                              detail::CostasType costasType = detail::CostasType::ORIGINAL) {
    if (msg12.size() != 12) throw std::runtime_error("JS8Call message must be exactly 12 chars");
    std::array<uint8_t, 11> bytes = {};

    // Pack 12x 6-bit words into first 9 bytes (72 bits), 4 chars -> 24 bits -> 3 bytes.
    for (int i = 0, j = 0; i < 12; i += 4, j += 3) {
        uint32_t words = (detail::alphabetWord(msg12[i + 0]) << 18) |
                         (detail::alphabetWord(msg12[i + 1]) << 12) |
                         (detail::alphabetWord(msg12[i + 2]) << 6) |
                         (detail::alphabetWord(msg12[i + 3]) << 0);
        bytes[j + 0] = (uint8_t)(words >> 16);
        bytes[j + 1] = (uint8_t)(words >> 8);
        bytes[j + 2] = (uint8_t)(words >> 0);
    }

    // Frame type (3 bits) into top 3 bits of byte 9.
    bytes[9] = (uint8_t)((type3bit & 0x7u) << 5);

    // CRC12 over all 11 bytes with trailing zeros, then place CRC bits.
    uint16_t crc = detail::crc12_aug_xor42(bytes);
    bytes[9] |= (uint8_t)((crc >> 7) & 0x1Fu);
    bytes[10] = (uint8_t)((crc & 0x7Fu) << 1);

    // Build tones output: Costas + parity29 + Costas + data29 + Costas
    std::array<uint8_t, 79> tones = {};
    const auto& costas = detail::kCostas[(int)costasType];

    // insert costas at offsets 0,36,72
    for (int block = 0; block < 3; ++block) {
        for (int i = 0; i < 7; ++i) tones[block * 36 + i] = (uint8_t)costas[block][i];
    }

    uint8_t parityWord = 0;
    uint8_t outputWord = 0;
    int outBits = 0;
    size_t outByte = 0;
    uint8_t outMask = 0x80;

    int parityPos = 7;
    int dataPos = 43;

    for (size_t i = 0; i < 87; ++i) {
        // parity bit for row i
        size_t parityBits = 0;
        size_t pByte = 0;
        uint8_t pMask = 0x80;
        for (size_t j = 0; j < 87; ++j) {
            bool p = detail::parityBit(i, j);
            bool b = (bytes[pByte] & pMask) != 0;
            parityBits += (p && b) ? 1 : 0;
            if (pMask == 1) {
                ++pByte;
                pMask = 0x80;
            } else {
                pMask >>= 1;
            }
        }

        parityWord = (uint8_t)((parityWord << 1) | (parityBits & 1u));
        outputWord = (uint8_t)((outputWord << 1) | (((bytes[outByte] & outMask) != 0) ? 1u : 0u));
        if (outMask == 1) {
            ++outByte;
            outMask = 0x80;
        } else {
            outMask >>= 1;
        }

        if (++outBits == 3) {
            tones[parityPos++] = (uint8_t)(parityWord & 7u);
            tones[dataPos++] = (uint8_t)(outputWord & 7u);
            parityWord = 0;
            outputWord = 0;
            outBits = 0;
        }
    }

    return tones;
}

}  // namespace js8call
