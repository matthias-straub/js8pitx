#pragma once

#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "js8_huffman.h"

namespace js8call {
namespace detail {

constexpr std::string_view kVaricodeAlphabet =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-+/?.";
inline std::string uppercase_trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = value.find_last_not_of(" \t\r\n");
    value = value.substr(first, last - first + 1);
    for (char& c : value) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return value;
}
inline unsigned alphabet_index(char c) {
    const auto index = kVaricodeAlphabet.find(c);
    if (index == std::string_view::npos) {
        throw std::runtime_error("Internal JS8Call payload alphabet error");
    }
    return static_cast<unsigned>(index);
}

inline void append_bits(std::vector<uint8_t>& bits, uint64_t value, unsigned width) {
    for (unsigned i = width; i-- > 0;) {
        bits.push_back(static_cast<uint8_t>((value >> i) & 1u));
    }
}
inline std::string pack_bits72(const std::vector<uint8_t>& bits) {
    if (bits.size() != 72) {
        throw std::runtime_error("Internal JS8Call structured payload must contain 72 bits");
    }
    std::string payload;
    payload.reserve(12);
    for (size_t offset = 0; offset < bits.size(); offset += 6) {
        uint8_t word = 0;
        for (size_t bit = 0; bit < 6; ++bit) {
            word = static_cast<uint8_t>((word << 1) | bits[offset + bit]);
        }
        payload.push_back(kVaricodeAlphabet[word]);
    }
    return payload;
}
inline unsigned alphanumeric_index(char c) {
    constexpr std::string_view alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ /@";
    const auto index = alphabet.find(c);
    if (index == std::string_view::npos) {
        throw std::runtime_error("Invalid character in JS8Call callsign");
    }
    return static_cast<unsigned>(index);
}
inline uint64_t pack_alphanumeric50(std::string value) {
    value = uppercase_trim(std::move(value));
    for (char c : value) {
        if (std::string_view("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ /@").find(c) ==
            std::string_view::npos) {
            throw std::runtime_error("Compound callsign contains an unsupported character");
        }
    }
    if (value.size() > 3 && value[3] != '/') value.insert(3, " ");
    if (value.size() > 7 && value[7] != '/') value.insert(7, " ");
    if (value.size() > 11) {
        throw std::runtime_error("Compound callsign is too long for a JS8Call frame");
    }
    value.append(11 - value.size(), ' ');
    uint64_t packed = 0;
    packed = packed * 39 + alphanumeric_index(value[0]);
    packed = packed * 38 + alphanumeric_index(value[1]);
    packed = packed * 38 + alphanumeric_index(value[2]);
    packed = packed * 2 + (value[3] == '/');
    packed = packed * 38 + alphanumeric_index(value[4]);
    packed = packed * 38 + alphanumeric_index(value[5]);
    packed = packed * 38 + alphanumeric_index(value[6]);
    packed = packed * 2 + (value[7] == '/');
    packed = packed * 38 + alphanumeric_index(value[8]);
    packed = packed * 38 + alphanumeric_index(value[9]);
    packed = packed * 38 + alphanumeric_index(value[10]);
    return packed;
}
inline uint32_t pack_callsign(std::string callsign, bool* portable) {
    callsign = uppercase_trim(std::move(callsign));
    if (portable) *portable = false;

    if (callsign.size() > 2 && callsign.compare(callsign.size() - 2, 2, "/P") == 0) {
        callsign.resize(callsign.size() - 2);
        if (portable) *portable = true;
    }

    if (callsign.size() < 2 || callsign.size() > 6) {
        throw std::runtime_error("Callsign must be 2 to 6 characters (optionally followed by /P)");
    }
    std::vector<std::string> forms{callsign};
    switch (callsign.size()) {
        case 2:
            forms.push_back(" " + callsign + "   ");
            break;
        case 3:
            forms.push_back(" " + callsign + "  ");
            forms.push_back(callsign + "   ");
            break;
        case 4:
            forms.push_back(" " + callsign + " ");
            forms.push_back(callsign + "  ");
            break;
        case 5:
            forms.push_back(" " + callsign);
            forms.push_back(callsign + " ");
            break;
        default:
            break;
    }
    std::string matched;
    for (const std::string& form : forms) {
        if (form.size() != 6) continue;
        const bool valid =
            std::string_view("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ ").find(form[0]) !=
                std::string_view::npos &&
            std::string_view("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ").find(form[1]) !=
                std::string_view::npos &&
            std::isdigit(static_cast<unsigned char>(form[2])) &&
            std::string_view("ABCDEFGHIJKLMNOPQRSTUVWXYZ ").find(form[3]) !=
                std::string_view::npos &&
            std::string_view("ABCDEFGHIJKLMNOPQRSTUVWXYZ ").find(form[4]) !=
                std::string_view::npos &&
            std::string_view("ABCDEFGHIJKLMNOPQRSTUVWXYZ ").find(form[5]) !=
                std::string_view::npos;
        if (valid) matched = form;
    }
    if (matched.empty()) {
        throw std::runtime_error("Callsign is not encodable in a standard JS8Call directed frame");
    }
    uint32_t packed = alphanumeric_index(matched[0]);
    packed = 36 * packed + alphanumeric_index(matched[1]);
    packed = 10 * packed + alphanumeric_index(matched[2]);
    packed = 27 * packed + alphanumeric_index(matched[3]) - 10;
    packed = 27 * packed + alphanumeric_index(matched[4]) - 10;
    packed = 27 * packed + alphanumeric_index(matched[5]) - 10;
    return packed;
}
inline uint16_t pack_grid(std::string grid) {
    grid = uppercase_trim(std::move(grid));
    if (grid.size() != 4 || grid[0] < 'A' || grid[0] > 'R' ||
        grid[1] < 'A' || grid[1] > 'R' ||
        !std::isdigit(static_cast<unsigned char>(grid[2])) ||
        !std::isdigit(static_cast<unsigned char>(grid[3]))) {
        throw std::runtime_error(
            "Grid must be a valid 4-character Maidenhead locator (for example EM73)");
    }
    return static_cast<uint16_t>(
        180 * (180 - 10 * (grid[0] - 'A') - (grid[2] - '0')) +
        10 * (grid[1] - 'A') + (grid[3] - '0'));
}

inline bool whitespace_only(const std::string& value) {
    return !value.empty() && value.find_first_not_of(" \t\r\n") == std::string::npos;
}
inline uint8_t command_code(const std::string& command) {
    // JS8Call command 31 is the special directed-freetext marker. Upstream
    // represents it literally as a single space, so handle whitespace before
    // trimming the normal command strings.
    if (whitespace_only(command)) return 31;
    const std::string cmd = uppercase_trim(command);
    static const std::array<std::pair<std::string_view, uint8_t>, 27> commands{{
        {"SNR?", 0},       {"DIT DIT", 1},   {"NACK", 2},        {"HEARING?", 3},
        {"GRID?", 4},      {">", 5},         {"STATUS?", 6},     {"STATUS", 7},
        {"HEARING", 8},    {"MSG", 9},       {"MSG TO:", 10},    {"QUERY", 11},
        {"QUERY MSGS", 12},{"QUERY MSGS?", 12}, {"QUERY CALL", 13}, {"ACK", 14},
        {"GRID", 15},      {"INFO?", 16},    {"INFO", 17},       {"FB", 18},
        {"HW CPY?", 19},   {"SK", 20},       {"RR", 21},         {"QSL?", 22},
        {"QSL", 23},       {"CMD", 24},      {"SNR", 25},
    }};
    for (const auto& [name, code] : commands) {
        if (cmd == name) return code;
    }
    if (cmd == "NO") return 26;
    if (cmd == "YES") return 27;
    if (cmd == "73") return 28;
    if (cmd == "HEARTBEAT SNR") return 29;
    if (cmd == "AGN?") return 30;
    throw std::runtime_error("Unsupported JS8Call directed command");
}
inline uint8_t pack_snr(int snr) {
    if (snr < -30 || snr > 31) {
        throw std::runtime_error("SNR must be between -30 and +31 dB");
    }
    return static_cast<uint8_t>(snr + 31);
}

}  // namespace detail
// Build legacy NORMAL-mode JS8Call Huffman data frames. A frame contains the
// largest whole-character prefix that fits in 72 bits. Longer text is split
// across successive frames.
inline std::vector<std::string> pack_normal_huffman_frames(const std::string& input) {
    if (input.empty()) {
        throw std::runtime_error("Text must not be empty");
    }

    const auto& table = js8::huffman_table();
    std::vector<std::string> frames;
    std::size_t offset = 0;
    while (offset < input.size()) {
        std::vector<uint8_t> bits;
        bits.reserve(72);

        // Legacy NORMAL data prefix: data=1, compressed=0.
        bits.push_back(1);
        bits.push_back(0);

        const std::size_t frame_start = offset;
        while (offset < input.size()) {
            unsigned char ch = static_cast<unsigned char>(input[offset]);
            if (ch >= 'a' && ch <= 'z') {
                ch = static_cast<unsigned char>(ch - 'a' + 'A');
            }
            const auto it = table.find(static_cast<char32_t>(ch));
            if (it == table.end()) {
                throw std::runtime_error(
                    "Text includes a character not supported by JS8Call's normal Huffman table");
            }

            const auto code = it->second;
            // One bit is always kept available for the padding terminator.
            if (bits.size() + code.nbits >= 72) break;
            for (int bit = code.nbits - 1; bit >= 0; --bit) {
                bits.push_back(static_cast<uint8_t>((code.bits >> bit) & 1u));
            }
            ++offset;
        }

        if (offset == frame_start) {
            throw std::runtime_error("No input characters fit in a JS8Call data frame");
        }

        const std::size_t payload_end = bits.size();
        while (bits.size() < 72) {
            bits.push_back(bits.size() == payload_end ? 0 : 1);
        }
        std::string frame;
        frame.reserve(12);
        for (std::size_t i = 0; i < 72; i += 6) {
            uint8_t word = 0;
            for (std::size_t j = 0; j < 6; ++j) {
                word = static_cast<uint8_t>((word << 1) | bits[i + j]);
            }
            frame.push_back(detail::kVaricodeAlphabet[word]);
        }
        frames.push_back(std::move(frame));
    }

    return frames;
}
inline std::string pack_normal_huffman_frame(const std::string& input) {
    const auto frames = pack_normal_huffman_frames(input);
    if (frames.size() != 1) {
        throw std::runtime_error(
            "Text is too long for one JS8Call normal-mode frame; use pack_normal_huffman_frames");
    }
    return frames.front();
}
// JS8Call heartbeat/CQ frame: [type:3][call:50][grid:16][subtype:3].
inline std::string pack_heartbeat_frame(const std::string& callsign,
                                        const std::string& grid,
                                        const std::string& cq_type = "") {
    uint8_t subtype = 0;
    if (!cq_type.empty()) {
        const std::string cq = detail::uppercase_trim(cq_type);
        if (cq == "CQ CQ CQ") subtype = 0;
        else if (cq == "CQ DX") subtype = 1;
        else if (cq == "CQ QRP") subtype = 2;
        else if (cq == "CQ CONTEST") subtype = 3;
        else if (cq == "CQ FIELD") subtype = 4;
        else if (cq == "CQ FD") subtype = 5;
        else if (cq == "CQ CQ") subtype = 6;
        else if (cq == "CQ") subtype = 7;
        else throw std::runtime_error("Unsupported CQ type");
    }
    uint16_t extra = grid.empty() ? 0x7fffu : detail::pack_grid(grid);
    if (!cq_type.empty()) extra |= 0x8000u;

    std::vector<uint8_t> bits;
    bits.reserve(72);
    detail::append_bits(bits, 0, 3);  // FrameHeartbeat
    detail::append_bits(bits, detail::pack_alphanumeric50(callsign), 50);
    detail::append_bits(bits, extra, 16);
    detail::append_bits(bits, subtype, 3);
    return detail::pack_bits72(bits);
}
// JS8Call directed frame: [type:3][from:28][to:28][command:5]
//                          [portable flags:2][number:6].
inline std::string pack_directed_frame(const std::string& from,
                                       const std::string& to,
                                       const std::string& command,
                                       int snr = 0,
                                       bool include_snr = false) {
    bool from_portable = false;
    bool to_portable = false;
    const uint32_t packed_from = detail::pack_callsign(from, &from_portable);
    const uint32_t packed_to = detail::pack_callsign(to, &to_portable);
    const uint8_t code = detail::command_code(command);
    uint8_t number = 0;
    const std::string normalized_command = detail::uppercase_trim(command);
    if (include_snr) {
        if (normalized_command != "ACK" && normalized_command != "SNR" &&
            normalized_command != "HEARTBEAT SNR") {
            throw std::runtime_error(
                "--snr is only valid with directed ACK, SNR, or HEARTBEAT SNR");
        }
        number = detail::pack_snr(snr);
    }
    std::vector<uint8_t> bits;
    bits.reserve(72);
    detail::append_bits(bits, 3, 3);  // FrameDirected
    detail::append_bits(bits, packed_from, 28);
    detail::append_bits(bits, packed_to, 28);
    detail::append_bits(bits, code, 5);
    detail::append_bits(bits, from_portable ? 1 : 0, 1);
    detail::append_bits(bits, to_portable ? 1 : 0, 1);
    detail::append_bits(bits, number, 6);
    return detail::pack_bits72(bits);
}
// A normal keyboard-to-keyboard directed message is represented by one
// FrameDirected with command 31 (space / freetext marker), followed by one or
// more ordinary Huffman data frames. Transmission FIRST/LAST flags are applied
// by the caller across the complete returned sequence.
inline std::vector<std::string> pack_directed_text_frames(const std::string& from,
                                                          const std::string& to,
                                                          const std::string& text) {
    if (text.empty()) {
        throw std::runtime_error("Directed text must not be empty");
    }
    std::vector<std::string> frames;
    frames.push_back(pack_directed_frame(from, to, " "));
    auto data_frames = pack_normal_huffman_frames(text);
    frames.insert(frames.end(), data_frames.begin(), data_frames.end());
    return frames;
}

}  // namespace js8call
