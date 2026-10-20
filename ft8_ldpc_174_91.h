#ifndef FT8_LDPC_174_91_H
#define FT8_LDPC_174_91_H
/*
  Minimal FT8 LDPC(174,91) support tables.

  This header provides:
    - FT8_INTERLEAVER[174] : permutation for FT8 interleaver
    - FT8_PARITY_EQS[83]   : parity equations in lower-triangular form

  IMPORTANT:
  The correct FT8 LDPC tables are required for over-the-air decodability.
  The tables below are placeholders and MUST be replaced with the official
  ldpc_174_91 + interleaver tables from WSJT-X (or equivalent reference).

  Reason: FT8 uses a specific standardized LDPC code and interleaver; any
  deviation will produce an undecodable transmission.

  Next step in this workspace:
    - Populate these tables from WSJT-X sources and keep license notice.
*/

#include <cstdint>

struct ft8_parity_eq {
    uint8_t count;
    int16_t pos[64];
};

// Placeholder interleaver: identity (NOT CORRECT)
static const uint8_t FT8_INTERLEAVER[174] = {
    // 0..173
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
    50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
    60, 61, 62, 63, 64, 65, 66, 67, 68, 69,
    70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
    80, 81, 82, 83, 84, 85, 86, 87, 88, 89,
    90, 91, 92, 93, 94, 95, 96, 97, 98, 99,
    100, 101, 102, 103, 104, 105, 106, 107, 108, 109,
    110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
    120, 121, 122, 123, 124, 125, 126, 127, 128, 129,
    130, 131, 132, 133, 134, 135, 136, 137, 138, 139,
    140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
    150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
    160, 161, 162, 163, 164, 165, 166, 167, 168, 169,
    170, 171, 172, 173
};

// Placeholder parity equations: none (NOT CORRECT)
static const ft8_parity_eq FT8_PARITY_EQS[83] = {
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}},
    {0, {0}}
};

#endif
