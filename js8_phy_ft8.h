#pragma once
#include <cstdint>
#include <string>
#include <vector>

// FT8 physical layer bits -> tones helper reused for JS8Call when assuming FT8 PHY.
// This wraps the encoder pieces from app/ft8tx.cpp into reusable functions.
//
// Contract:
// - Input: 77-bit payload (msg77) as bits MSB-first in a vector<uint8_t> of size 77 with values 0/1.
// - Output: 79 tones (0..7) as vector<uint8_t> size 79.
//
// Implementation matches app/ft8tx.cpp (CRC14 + LDPC(174,91) + interleaver + Gray map + 3x7 sync blocks).
namespace js8 {

// CRC14 polynomial per ft8tx.cpp
inline uint16_t ft8_crc14(const std::vector<uint8_t>& msg77) {
    uint16_t reg = 0;
    const uint16_t poly = 0x2757;
    for (int i = 0; i < 77; i++) {
        uint16_t top = (reg >> 13) & 1;
        uint16_t in = msg77[i] & 1;
        reg = ((reg << 1) & 0x3fff) | in;
        if (top) reg ^= poly;
    }
    return reg & 0x3fff;
}

// Interleaver and parity equations are provided by existing header.
#include "ft8_ldpc_174_91.h"

inline void ldpc_encode_174_91(const std::vector<uint8_t>& msg91, std::vector<uint8_t>& code174) {
    code174.assign(174, 0);
    for (int i = 0; i < 91; i++) code174[i] = msg91[i];
    for (int i = 0; i < 83; i++) {
        uint8_t p = 0;
        const ft8_parity_eq& eq = FT8_PARITY_EQS[i];
        for (int k = 0; k < eq.count; k++) {
            int pos = eq.pos[k];
            if (pos < 0 || pos >= 91 + i) continue;
            p ^= code174[pos];
        }
        code174[91 + i] = p;
    }
}

static const uint8_t FT8_SYNC[7] = {3, 1, 4, 0, 6, 5, 2};

inline void ft8_make_tones(const std::vector<uint8_t>& code174, std::vector<uint8_t>& tones79) {
    std::vector<uint8_t> inter(174, 0);
    for (int i = 0; i < 174; i++) inter[i] = code174[FT8_INTERLEAVER[i]];

    std::vector<uint8_t> sym58(58, 0);
    for (int s = 0; s < 58; s++) {
        uint8_t v = 0;
        for (int b = 0; b < 3; b++) {
            v = (v << 1) | inter[s * 3 + b];
        }
        uint8_t gray = v ^ (v >> 1);
        sym58[s] = gray & 7;
    }

    tones79.assign(79, 0);
    int di = 0;
    for (int i = 0; i < 79; i++) {
        bool sync = (i < 7) || (i >= 36 && i < 43) || (i >= 72);
        if (sync) {
            tones79[i] = FT8_SYNC[i % 7];
        } else {
            tones79[i] = sym58[di++];
        }
    }
}

inline std::vector<uint8_t> encode_msg77_to_tones79(const std::vector<uint8_t>& msg77) {
    std::vector<uint8_t> msg91(91, 0);
    for (int i = 0; i < 77; i++) msg91[i] = msg77[i] & 1;
    uint16_t crc = ft8_crc14(msg77);
    for (int i = 0; i < 14; i++) msg91[77 + i] = (crc >> (13 - i)) & 1;

    std::vector<uint8_t> code174;
    ldpc_encode_174_91(msg91, code174);

    std::vector<uint8_t> tones79;
    ft8_make_tones(code174, tones79);
    return tones79;
}

} // namespace js8
