#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace js8 {

// Canonical Huffman code table as documented in "JS8Call Guide v2.2" Appendix A.
// Codewords are bitstrings MSB-first.
struct HuffCode {
    uint32_t bits;  // right-aligned
    uint8_t nbits;
};

inline const std::unordered_map<char32_t, HuffCode>& huffman_table() {
    static const std::unordered_map<char32_t, HuffCode> table = {
        {U' ', {0b01, 2}},
        {U'E', {0b100, 3}},
        {U'T', {0b1101, 4}},
        {U'A', {0b0011, 4}},
        {U'O', {0b11111, 5}},
        {U'I', {0b11100, 5}},
        {U'N', {0b10111, 5}},
        {U'S', {0b10100, 5}},
        {U'H', {0b00011, 5}},
        {U'R', {0b00000, 5}},
        {U'D', {0b111011, 6}},
        {U'L', {0b110011, 6}},
        {U'C', {0b110001, 6}},
        {U'U', {0b101101, 6}},
        {U'M', {0b101011, 6}},
        {U'W', {0b001011, 6}},
        {U'F', {0b001001, 6}},
        {U'G', {0b000101, 6}},
        {U'Y', {0b000011, 6}},
        {U'P', {0b1111011, 7}},
        {U'B', {0b1111001, 7}},
        {U'.', {0b1110100, 7}},
        {U'V', {0b1100101, 7}},
        {U'K', {0b1100100, 7}},
        {U'-', {0b1100001, 7}},
        {U'+', {0b1100000, 7}},
        {U'?', {0b1011001, 7}},
        {U'!', {0b1011000, 7}},
        {U'"', {0b1010101, 7}},
        {U'X', {0b1010100, 7}},
        {U'0', {0b0010101, 7}},
        {U'J', {0b0010100, 7}},
        {U'1', {0b0010001, 7}},
        {U'Q', {0b0010000, 7}},
        {U'2', {0b0001001, 7}},
        {U'Z', {0b0001000, 7}},
        {U'3', {0b0000101, 7}},
        {U'5', {0b0000100, 7}},
        {U'4', {0b11110101, 8}},
        {U'9', {0b11110100, 8}},
        {U'8', {0b11110001, 8}},
        {U'6', {0b11110000, 8}},
        {U'7', {0b11101011, 8}},
        {U'/', {0b11101010, 8}},
    };
    return table;
}

// Pack text to a stream of bits (MSB-first per codeword), returned as bytes (bit 7 first).
// Only supports the subset of characters present in the Appendix A table; caller should pre-map to that set.
inline std::vector<uint8_t> huffman_encode_bits(const std::string& ascii_upper) {
    const auto& tbl = huffman_table();
    std::vector<uint8_t> out;
    uint8_t cur = 0;
    int fill = 0; // bits filled into cur (0..7)
    auto push_bit = [&](int b) {
        cur = (uint8_t)((cur << 1) | (b & 1));
        fill++;
        if (fill == 8) {
            out.push_back(cur);
            cur = 0;
            fill = 0;
        }
    };

    for (unsigned char ch8 : ascii_upper) {
        char32_t ch = (char32_t)ch8;
        auto it = tbl.find(ch);
        if (it == tbl.end()) continue; // drop unsupported chars
        HuffCode hc = it->second;
        for (int i = hc.nbits - 1; i >= 0; i--) {
            push_bit((hc.bits >> i) & 1);
        }
    }
    if (fill) {
        cur <<= (8 - fill);
        out.push_back(cur);
    }
    return out;
}

} // namespace js8
