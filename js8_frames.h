#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "js8_huffman.h"

namespace js8 {

// JS8Call uses 75-bit frames at the protocol layer (per JS8Call Guide v2.2).
// This header implements a *minimal* clean-room framing that fits inside a 77-bit FT8-style payload:
// - We store: [frame_type:3][tx_flag:2][payload75:75] -> total 80 bits would not fit.
// Because the public guide does not provide exact bit layout, we implement a pragmatic mapping:
//
// FT8-style msg77 is used as:
//   msg77[0..1]   : reserved (0)
//   msg77[2..76]  : js8_frame75 bits (75 bits)
//
// js8_frame75 bits layout we define as:
//   [type:3][flag:2][data:70]
//
// This is sufficient to build a deterministic on-air format, but may not decode in JS8Call
// without matching the real bit layout. It is the best possible under the chosen constraints.
//
// Frame types (from doc), mapped to 3-bit ids:
enum class FrameType : uint8_t {
    Heartbeat = 0,
    CompoundPartial = 1,
    CompoundDirectedCmd = 2,
    DirectedCmd = 3,
    DataHuffman = 4,
    DataDictionary = 5,
};

enum class TxFlag : uint8_t { Default = 0, First = 1, Last = 2, Reserved = 3 };

struct Frame75 {
    FrameType type;
    TxFlag flag;
    std::vector<uint8_t> data_bits; // up to 70 bits, MSB-first
};

// Utility: append N bits MSB-first
inline void append_bits(std::vector<uint8_t>& out, uint64_t v, int n) {
    for (int i = n - 1; i >= 0; i--) out.push_back((v >> i) & 1);
}

inline Frame75 make_data_huffman(const std::string& msg_ascii_upper, TxFlag flag) {
    // Encode using Appendix A Huffman table and then truncate/pad to fit 70 bits.
    auto bytes = huffman_encode_bits(msg_ascii_upper);
    std::vector<uint8_t> bits;
    bits.reserve(bytes.size() * 8);
    for (uint8_t b : bytes) {
        for (int i = 7; i >= 0; i--) bits.push_back((b >> i) & 1);
    }
    if ((int)bits.size() > 70) bits.resize(70);
    while ((int)bits.size() < 70) bits.push_back(0);

    return Frame75{FrameType::DataHuffman, flag, bits};
}

inline Frame75 make_heartbeat(const std::string& grid4, TxFlag flag) {
    // Minimal heartbeat payload: 4-char grid packed to 15 bits (FT8 pack) + 55 bits reserved.
    // Grid packing per WSJT-X: ng = 180*(A*10+B)+10*C+D, A,B:0..17 C,D:0..9
    auto up = [](std::string s) {
        for (auto& c : s) c = (char)toupper((unsigned char)c);
        return s;
    };
    std::string g = up(grid4);
    uint16_t ng = 0;
    if (g.size() >= 4) {
        int A = g[0] - 'A', B = g[1] - 'A', C = g[2] - '0', D = g[3] - '0';
        if (A >= 0 && A <= 17 && B >= 0 && B <= 17 && C >= 0 && C <= 9 && D >= 0 && D <= 9) {
            ng = (uint16_t)(180 * (A * 10 + B) + 10 * C + D);
        }
    }

    std::vector<uint8_t> bits;
    bits.reserve(70);
    append_bits(bits, ng, 15);
    while ((int)bits.size() < 70) bits.push_back(0);
    return Frame75{FrameType::Heartbeat, flag, bits};
}

inline std::vector<uint8_t> frame75_to_msg77(const Frame75& f) {
    std::vector<uint8_t> msg77(77, 0);
    std::vector<uint8_t> frame;
    frame.reserve(75);
    append_bits(frame, (uint8_t)f.type, 3);
    append_bits(frame, (uint8_t)f.flag, 2);
    for (auto b : f.data_bits) frame.push_back(b & 1);
    if ((int)frame.size() > 75) frame.resize(75);
    while ((int)frame.size() < 75) frame.push_back(0);

    // map into msg77[2..76]
    for (int i = 0; i < 75; i++) msg77[2 + i] = frame[i];
    return msg77;
}

} // namespace js8
