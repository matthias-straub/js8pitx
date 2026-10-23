#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "librpitx/src/librpitx.h"
#include "js8_js8call_encoder.h"
#include "js8_varicode.h"

enum class Js8Speed {
    Normal,
    Fast,
    Turbo,
    Slow,
};

struct Js8Mode {
    const char* name;
    const char* designator;
    float symbol_rate;
    float tone_spacing_hz;
    js8call::detail::CostasType costas;
};

static const Js8Mode& js8_mode(Js8Speed speed) {
    // JS8Call submode parameters. Tone spacing equals symbol rate for JS8.
    static const Js8Mode normal = {
        "NORMAL", "JS8A", 6.25f, 6.25f, js8call::detail::CostasType::ORIGINAL};
    static const Js8Mode fast = {
        "FAST", "JS8B", 10.0f, 10.0f, js8call::detail::CostasType::MODIFIED};
    static const Js8Mode turbo = {
        "TURBO", "JS8C", 20.0f, 20.0f, js8call::detail::CostasType::MODIFIED};
    static const Js8Mode slow = {
        "SLOW", "JS8E", 3.125f, 3.125f, js8call::detail::CostasType::MODIFIED};

    switch (speed) {
        case Js8Speed::Normal: return normal;
        case Js8Speed::Fast: return fast;
        case Js8Speed::Turbo: return turbo;
        case Js8Speed::Slow: return slow;
    }

    return normal;
}

static void usage(const char* prog) {
    fprintf(stderr,
            "Usage: %s --freq <rf_hz> <payload mode> [options]\n"
            "\n"
            "Payload modes:\n"
            "  --text <TEXT>                 Free-text JS8Call data message (one or more frames)\n"
            "  --cq <TYPE> --call CALL [--grid GRID]\n"
            "                                CQ frame; TYPE: CQ, \"CQ CQ\", \"CQ DX\", \"CQ QRP\",\n"
            "                                \"CQ CONTEST\", \"CQ FIELD\", or \"CQ FD\"\n"
            "  --heartbeat --call CALL [--grid GRID]\n"
            "                                Heartbeat frame\n"
            "  --directed --from CALL --to CALL --cmd COMMAND [--snr DB]\n"
            "                                Directed QSO/control frame; e.g. SNR, RR, QSL, 73, SK\n"
            "  --raw12 <PAYLOAD12>           Already-packed 12-character payload (diagnostic)\n"
            "\n"
            "JS8 speed (NORMAL is the default):\n"
            "  --normal                      JS8A NORMAL: 6.25 baud, 6.25 Hz tone spacing\n"
            "  --slow                        JS8E SLOW:   3.125 baud, 3.125 Hz tone spacing\n"
            "  --fast                        JS8B FAST:   10 baud, 10 Hz tone spacing\n"
            "  --turbo                       JS8C TURBO:  20 baud, 20 Hz tone spacing\n"
            "\n"
            "Frame options:\n"
            "  --type <0..7>                 Override raw-frame flags (default 3 = FIRST|LAST)\n"
            "  --repeat <n>                  Repeat frame n times (default 1)\n"
            "  --level <0..7>                PAD drive level (default 7)\n"
            "  --ramp <0..0.5>               Symbol ramp fraction (default 0.1)\n"
            "\n"
            "Examples:\n"
            "  %s --freq 7078000 --cq \"CQ DX\" --call N0CALL --grid EM73\n"
            "  %s --freq 7078000 --fast --text \"TEST\"\n"
            "  %s --freq 7078000 --slow --heartbeat --call N0CALL --grid EM73\n"
            "  %s --freq 7078000 --turbo --directed --from N0CALL --to K1ABC --cmd SNR --snr -12\n",
            prog, prog, prog, prog, prog);
}

int main(int argc, char** argv) {
    uint64_t rf_freq = 0;

    std::string text;
    std::string raw12;
    std::string cq_type;
    std::string call;
    std::string grid;
    std::string from;
    std::string to;
    std::string command;
    bool heartbeat = false;
    bool directed = false;
    bool snr_provided = false;
    int snr = 0;

    Js8Speed speed = Js8Speed::Normal;
    int speed_flags = 0;

    uint8_t type3 = 3;  // JS8Call | JS8CallFirst | JS8CallLast
    bool type_overridden = false;
    int repeat = 1;
    int pad_level = 7;
    float ramp_frac = 0.10f;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--freq") && i + 1 < argc) {
            rf_freq = static_cast<uint64_t>(strtoull(argv[++i], nullptr, 10));
        } else if (!strcmp(argv[i], "--text") && i + 1 < argc) {
            text = argv[++i];
        } else if (!strcmp(argv[i], "--raw12") && i + 1 < argc) {
            raw12 = argv[++i];
        } else if (!strcmp(argv[i], "--cq") && i + 1 < argc) {
            cq_type = argv[++i];
        } else if (!strcmp(argv[i], "--heartbeat")) {
            heartbeat = true;
        } else if (!strcmp(argv[i], "--directed")) {
            directed = true;
        } else if (!strcmp(argv[i], "--call") && i + 1 < argc) {
            call = argv[++i];
        } else if (!strcmp(argv[i], "--grid") && i + 1 < argc) {
            grid = argv[++i];
        } else if (!strcmp(argv[i], "--from") && i + 1 < argc) {
            from = argv[++i];
        } else if (!strcmp(argv[i], "--to") && i + 1 < argc) {
            to = argv[++i];
        } else if (!strcmp(argv[i], "--cmd") && i + 1 < argc) {
            command = argv[++i];
        } else if (!strcmp(argv[i], "--snr") && i + 1 < argc) {
            snr = atoi(argv[++i]);
            snr_provided = true;
        } else if (!strcmp(argv[i], "--normal")) {
            speed = Js8Speed::Normal;
            ++speed_flags;
        } else if (!strcmp(argv[i], "--slow")) {
            speed = Js8Speed::Slow;
            ++speed_flags;
        } else if (!strcmp(argv[i], "--fast")) {
            speed = Js8Speed::Fast;
            ++speed_flags;
        } else if (!strcmp(argv[i], "--turbo")) {
            speed = Js8Speed::Turbo;
            ++speed_flags;
        } else if (!strcmp(argv[i], "--type") && i + 1 < argc) {
            type3 = static_cast<uint8_t>(atoi(argv[++i]) & 7);
            type_overridden = true;
        } else if (!strcmp(argv[i], "--repeat") && i + 1 < argc) {
            repeat = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--level") && i + 1 < argc) {
            pad_level = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--ramp") && i + 1 < argc) {
            ramp_frac = static_cast<float>(atof(argv[++i]));
        } else if (!strcmp(argv[i], "--help")) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    const int payload_modes = static_cast<int>(!text.empty()) + static_cast<int>(!raw12.empty()) +
                              static_cast<int>(!cq_type.empty()) + static_cast<int>(heartbeat) +
                              static_cast<int>(directed);
    if (!rf_freq || payload_modes != 1) {
        usage(argv[0]);
        return 1;
    }
    if (speed_flags > 1) {
        fprintf(stderr, "Error: select at most one of --normal, --slow, --fast, or --turbo\n");
        return 1;
    }
    if (repeat < 1) {
        fprintf(stderr, "Error: --repeat must be at least 1\n");
        return 1;
    }
    if (pad_level < 0 || pad_level > 7) {
        fprintf(stderr, "Error: --level must be between 0 and 7\n");
        return 1;
    }
    if (ramp_frac < 0.0f || ramp_frac > 0.5f) {
        fprintf(stderr, "Error: --ramp must be between 0 and 0.5\n");
        return 1;
    }
    if (!raw12.empty() && raw12.size() != 12) {
        fprintf(stderr, "Error: --raw12 payload must be exactly 12 characters (got %zu)\n",
                raw12.size());
        return 1;
    }
    if ((heartbeat || !cq_type.empty()) && call.empty()) {
        fprintf(stderr, "Error: --cq and --heartbeat require --call\n");
        return 1;
    }
    if (directed && (from.empty() || to.empty() || command.empty())) {
        fprintf(stderr, "Error: --directed requires --from, --to, and --cmd\n");
        return 1;
    }
    if (snr_provided && !directed) {
        fprintf(stderr, "Error: --snr applies only to --directed\n");
        return 1;
    }
    if (type_overridden && raw12.empty()) {
        fprintf(stderr, "Error: --type only applies to --raw12; generated frames require FIRST|LAST flags\n");
        return 1;
    }

    const Js8Mode& phy = js8_mode(speed);

    dbg_setlevel(1);

    std::vector<std::string> payloads;
    const char* mode = "raw";
    try {
        if (!raw12.empty()) {
            payloads.push_back(raw12);
        } else if (!text.empty()) {
            payloads = js8call::pack_normal_huffman_frames(text);
            mode = "text";
        } else if (!cq_type.empty()) {
            payloads.push_back(js8call::pack_heartbeat_frame(call, grid, cq_type));
            mode = "cq";
        } else if (heartbeat) {
            payloads.push_back(js8call::pack_heartbeat_frame(call, grid));
            mode = "heartbeat";
        } else {
            payloads.push_back(js8call::pack_directed_frame(from, to, command, snr, snr_provided));
            mode = "directed";
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "Encoding error: %s\n", e.what());
        return 1;
    }

    std::vector<unsigned char> symbols;
    symbols.reserve(static_cast<size_t>(repeat) * payloads.size() * 79);
    for (int r = 0; r < repeat; ++r) {
        for (size_t i = 0; i < payloads.size(); ++i) {
            uint8_t frame_type = type3;
            if (payloads.size() > 1) {
                frame_type = static_cast<uint8_t>((i == 0 ? 1u : 0u) |
                                                  (i + 1 == payloads.size() ? 2u : 0u));
            }
            const auto tones = js8call::encode_tones79(payloads[i], frame_type, phy.costas);
            for (const auto tone : tones) symbols.push_back(static_cast<unsigned char>(tone & 7));
        }
    }

    // NORMAL already uses 32x (200 Hz) because 8x/50 Hz is invalid on a Pi 4.
    // SLOW would be only 100 Hz at 32x, so use 64x and keep the PCM clock at
    // the same known-good 200 Hz used by NORMAL.
    const size_t upsample = speed == Js8Speed::Slow ? 64 : 32;

    // fskburst multiplies FifoSize by upsample internally.
    const uint32_t fifo = static_cast<uint32_t>(symbols.size() + 16);
    padgpio pad;
    pad.setlevel(pad_level);

    fskburst tx(rf_freq, phy.symbol_rate, phy.tone_spacing_hz, 14, fifo, upsample, ramp_frac);
    fprintf(stderr,
            "Transmitting %s/%s %s @ %llu Hz, frames=%zu, symbols=%zu, baud=%.3f, spacing=%.3f Hz\n",
            phy.designator, phy.name, mode, static_cast<unsigned long long>(rf_freq),
            payloads.size(), symbols.size(), phy.symbol_rate, phy.tone_spacing_hz);
    for (size_t i = 0; i < payloads.size(); ++i) {
        const uint8_t frame_type = payloads.size() == 1
            ? type3
            : static_cast<uint8_t>((i == 0 ? 1u : 0u) |
                                   (i + 1 == payloads.size() ? 2u : 0u));
        fprintf(stderr, "  frame %zu/%zu flags=%u payload=%s\n",
                i + 1, payloads.size(), static_cast<unsigned>(frame_type), payloads[i].c_str());
    }
    tx.SetSymbols(symbols.data(), static_cast<uint32_t>(symbols.size()));
    tx.stop();

    return 0;
}
