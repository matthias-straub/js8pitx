#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <time.h>

#include "librpitx/src/librpitx.h"
#include "js8_js8call_encoder.h"
#include "js8_varicode.h"

namespace {

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
    int64_t period_ms;
    int64_t tone_start_delay_ms;
    size_t upsample;
    js8call::detail::CostasType costas;
};

const Js8Mode& js8_mode(Js8Speed speed) {
    // JS8Call submode parameters. Tone spacing equals symbol rate.
    // Period/start-delay values match current JS8Call JS8Submode/commons data.
    static const Js8Mode normal = {
        "NORMAL", "JS8A", 6.25f, 6.25f, 15000, 500, 32,
        js8call::detail::CostasType::ORIGINAL};
    static const Js8Mode fast = {
        "FAST", "JS8B", 10.0f, 10.0f, 10000, 200, 32,
        js8call::detail::CostasType::MODIFIED};
    static const Js8Mode turbo = {
        "TURBO", "JS8C", 20.0f, 20.0f, 6000, 100, 32,
        js8call::detail::CostasType::MODIFIED};
    static const Js8Mode slow = {
        "SLOW", "JS8E", 3.125f, 3.125f, 30000, 500, 64,
        js8call::detail::CostasType::MODIFIED};

    switch (speed) {
        case Js8Speed::Normal: return normal;
        case Js8Speed::Fast: return fast;
        case Js8Speed::Turbo: return turbo;
        case Js8Speed::Slow: return slow;
    }

    return normal;
}

int64_t realtime_ns() {
    timespec ts{};
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        throw std::runtime_error(std::string("clock_gettime failed: ") + std::strerror(errno));
    }
    return static_cast<int64_t>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

int64_t next_slot_ms(int64_t period_ms) {
    const int64_t now_ms = realtime_ns() / 1000000LL;
    return ((now_ms / period_ms) + 1) * period_ms;
}

void wait_until_realtime_ns(int64_t target_ns) {
    timespec target{};
    target.tv_sec = static_cast<time_t>(target_ns / 1000000000LL);
    target.tv_nsec = static_cast<long>(target_ns % 1000000000LL);

    while (true) {
        const int rc = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &target, nullptr);
        if (rc == 0) return;
        if (rc == EINTR) continue;
        throw std::runtime_error(std::string("clock_nanosleep failed: ") + std::strerror(rc));
    }
}

int64_t librpitx_startup_lead_ns(const Js8Mode& phy) {
    // fskburst primes the 64-word PCM FIFO with 65 DMA writes before GPIO4 is
    // enabled. The 65th write normally waits for roughly one PCM sample.
    // PCM pacing is symbol_rate * upsample, so compensate by one sample here.
    const double pcm_rate_hz = static_cast<double>(phy.symbol_rate) *
                               static_cast<double>(phy.upsample);
    return static_cast<int64_t>(1000000000.0 / pcm_rate_hz + 0.5);
}

uint8_t frame_flags(size_t index, size_t count, uint8_t single_frame_flags) {
    if (count == 1) return single_frame_flags;
    return static_cast<uint8_t>((index == 0 ? 1u : 0u) |
                                (index + 1 == count ? 2u : 0u));
}

void usage(const char* prog) {
    fprintf(stderr,
            "Usage: %s --freq <rf_hz> <payload mode> [options]\n"
            "\n"
            "Payload modes:\n"
            "  --text <TEXT>                 Undirected free-text data message\n"
            "  --cq <TYPE> --call CALL [--grid GRID]\n"
            "                                CQ frame; TYPE: CQ, \"CQ CQ\", \"CQ DX\", \"CQ QRP\",\n"
            "                                \"CQ CONTEST\", \"CQ FIELD\", or \"CQ FD\"\n"
            "  --heartbeat --call CALL [--grid GRID]\n"
            "                                Heartbeat frame\n"
            "  --directed --from CALL --to CALL --cmd COMMAND [--snr DB]\n"
            "                                Directed QSO/control command\n"
            "  --directed --from CALL --to CALL --message TEXT\n"
            "                                Directed keyboard-to-keyboard free text\n"
            "  --raw12 <PAYLOAD12>           Already-packed 12-character payload (diagnostic)\n"
            "\n"
            "JS8 speed (NORMAL is the default):\n"
            "  --normal                      JS8A NORMAL: 6.25 baud, 15 s period, +500 ms start\n"
            "  --fast                        JS8B FAST:   10 baud,   10 s period, +200 ms start\n"
            "  --turbo                       JS8C TURBO:  20 baud,    6 s period, +100 ms start\n"
            "  --slow                        JS8E SLOW:    3.125 baud,30 s period, +500 ms start\n"
            "\n"
            "Timing / frame options:\n"
            "  --no-sync                     Disable JS8 slot synchronization (diagnostic)\n"
            "  --timing-adjust-ms <ms>       Fine adjustment of nominal RF tone start (default 0)\n"
            "  --type <0..7>                 Override raw-frame flags (default 3 = FIRST|LAST)\n"
            "  --repeat <n>                  Repeat the complete message n times (default 1)\n"
            "  --level <0..7>                PAD drive level (default 7)\n"
            "  --ramp <0..0.5>               Symbol ramp fraction (default 0.1)\n"
            "  --dry-run                     Encode and print frames without touching RF hardware\n"
            "\n"
            "Examples:\n"
            "  %s --freq 7078000 --cq \"CQ DX\" --call DL1ABC --grid JO62\n"
            "  %s --freq 7078000 --fast --text \"FAST TEST\"\n"
            "  %s --freq 7078000 --slow --heartbeat --call DL1ABC --grid JO62\n"
            "  %s --freq 7078000 --turbo --directed --from DL1ABC --to K1ABC --cmd SNR --snr -12\n",
            prog, prog, prog, prog, prog);
}

}  // namespace

int main(int argc, char** argv) {
    uint64_t rf_freq = 0;

    std::string text;
    std::string directed_text;
    std::string raw12;
    std::string cq_type;
    std::string call;
    std::string grid;
    std::string from;
    std::string to;
    std::string command;
    bool text_provided = false;
    bool directed_text_provided = false;
    bool command_provided = false;
    bool heartbeat = false;
    bool directed = false;
    bool snr_provided = false;
    bool sync_slots = true;
    bool dry_run = false;
    int snr = 0;
    int timing_adjust_ms = 0;

    Js8Speed speed = Js8Speed::Normal;
    int speed_flags = 0;

    uint8_t type3 = 3;  // JS8CallFirst | JS8CallLast
    bool type_overridden = false;
    int repeat = 1;
    int pad_level = 7;
    float ramp_frac = 0.10f;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--freq") && i + 1 < argc) {
            rf_freq = static_cast<uint64_t>(strtoull(argv[++i], nullptr, 10));
        } else if (!strcmp(argv[i], "--text") && i + 1 < argc) {
            text = argv[++i];
            text_provided = true;
        } else if (!strcmp(argv[i], "--message") && i + 1 < argc) {
            directed_text = argv[++i];
            directed_text_provided = true;
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
            command_provided = true;
        } else if (!strcmp(argv[i], "--snr") && i + 1 < argc) {
            snr = atoi(argv[++i]);
            snr_provided = true;
        } else if (!strcmp(argv[i], "--normal")) {
            speed = Js8Speed::Normal;
            ++speed_flags;
        } else if (!strcmp(argv[i], "--fast")) {
            speed = Js8Speed::Fast;
            ++speed_flags;
        } else if (!strcmp(argv[i], "--turbo")) {
            speed = Js8Speed::Turbo;
            ++speed_flags;
        } else if (!strcmp(argv[i], "--slow")) {
            speed = Js8Speed::Slow;
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
        } else if (!strcmp(argv[i], "--no-sync")) {
            sync_slots = false;
        } else if (!strcmp(argv[i], "--timing-adjust-ms") && i + 1 < argc) {
            timing_adjust_ms = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--dry-run")) {
            dry_run = true;
        } else if (!strcmp(argv[i], "--help")) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    const int payload_modes = static_cast<int>(text_provided) +
                              static_cast<int>(!raw12.empty()) +
                              static_cast<int>(!cq_type.empty()) +
                              static_cast<int>(heartbeat) +
                              static_cast<int>(directed);
    if (!rf_freq || payload_modes != 1) {
        usage(argv[0]);
        return 1;
    }
    if (speed_flags > 1) {
        fprintf(stderr, "Error: select at most one of --normal, --fast, --turbo, or --slow\n");
        return 1;
    }
    if (text_provided && text.empty()) {
        fprintf(stderr, "Error: --text must not be empty\n");
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
    if (timing_adjust_ms < -5000 || timing_adjust_ms > 5000) {
        fprintf(stderr, "Error: --timing-adjust-ms must be between -5000 and +5000\n");
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
    if (directed) {
        if (from.empty() || to.empty()) {
            fprintf(stderr, "Error: --directed requires --from and --to\n");
            return 1;
        }
        if (static_cast<int>(command_provided) + static_cast<int>(directed_text_provided) != 1) {
            fprintf(stderr, "Error: --directed requires exactly one of --cmd or --message\n");
            return 1;
        }
        if (directed_text_provided && directed_text.empty()) {
            fprintf(stderr, "Error: --message must not be empty\n");
            return 1;
        }
    } else if (command_provided || directed_text_provided) {
        fprintf(stderr, "Error: --cmd and --message require --directed\n");
        return 1;
    }
    if (snr_provided && (!directed || !command_provided)) {
        fprintf(stderr, "Error: --snr applies only to --directed --cmd\n");
        return 1;
    }
    if (type_overridden && raw12.empty()) {
        fprintf(stderr,
                "Error: --type only applies to --raw12; generated frames use JS8Call FIRST/LAST flags\n");
        return 1;
    }

    const Js8Mode& phy = js8_mode(speed);

    dbg_setlevel(1);
    std::vector<std::string> payloads;
    const char* payload_mode = "raw";
    try {
        if (!raw12.empty()) {
            payloads.push_back(raw12);
        } else if (text_provided) {
            payloads = js8call::pack_normal_huffman_frames(text);
            payload_mode = "text";
        } else if (!cq_type.empty()) {
            payloads.push_back(js8call::pack_heartbeat_frame(call, grid, cq_type));
            payload_mode = "cq";
        } else if (heartbeat) {
            payloads.push_back(js8call::pack_heartbeat_frame(call, grid));
            payload_mode = "heartbeat";
        } else if (directed_text_provided) {
            payloads = js8call::pack_directed_text_frames(from, to, directed_text);
            payload_mode = "directed-text";
        } else {
            payloads.push_back(
                js8call::pack_directed_frame(from, to, command, snr, snr_provided));
            payload_mode = "directed";
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "Encoding error: %s\n", e.what());
        return 1;
    }

    const size_t total_frames = payloads.size() * static_cast<size_t>(repeat);
    fprintf(stderr,
            "%s/%s %s @ %llu Hz, message_frames=%zu, repeat=%d, total_frames=%zu, "
            "baud=%.3f, spacing=%.3f Hz\n",
            phy.designator,
            phy.name,
            payload_mode,
            static_cast<unsigned long long>(rf_freq),
            payloads.size(),
            repeat,
            total_frames,
            phy.symbol_rate,
            phy.tone_spacing_hz);
    for (size_t i = 0; i < payloads.size(); ++i) {
        const uint8_t flags = frame_flags(i, payloads.size(), type3);
        fprintf(stderr, "  frame %zu/%zu flags=%u payload=%s\n",
                i + 1,
                payloads.size(),
                static_cast<unsigned>(flags),
                payloads[i].c_str());
    }

    if (dry_run) {
        fprintf(stderr,
                "Dry run: RF hardware not initialized; slot period=%lld ms, first-tone offset=%lld ms.\n",
                static_cast<long long>(phy.period_ms),
                static_cast<long long>(phy.tone_start_delay_ms));
        return 0;
    }

    const uint32_t fifo = 79 + 16;  // one JS8 frame plus margin; frames are sent separately

    padgpio pad;
    pad.setlevel(pad_level);

    // Initialize hardware before choosing the slot, so setup latency cannot make us miss it.
    fskburst tx(rf_freq,
                phy.symbol_rate,
                phy.tone_spacing_hz,
                14,
                fifo,
                phy.upsample,
                ramp_frac);

    const int64_t startup_lead_ns = librpitx_startup_lead_ns(phy);
    int64_t next_slot = 0;
    if (sync_slots) {
        try {
            next_slot = next_slot_ms(phy.period_ms);
        } catch (const std::exception& e) {
            fprintf(stderr, "Timing error: %s\n", e.what());
            tx.stop();
            return 1;
        }
        fprintf(stderr,
                "Slot sync enabled: mode=%s, period=%.3f s, nominal RF tone start=+%lld ms, "
                "PCM=%.3f Hz, librpitx startup lead~=%.3f ms, adjustment=%+d ms\n",
                phy.designator,
                static_cast<double>(phy.period_ms) / 1000.0,
                static_cast<long long>(phy.tone_start_delay_ms),
                static_cast<double>(phy.symbol_rate) * static_cast<double>(phy.upsample),
                static_cast<double>(startup_lead_ns) / 1000000.0,
                timing_adjust_ms);
    } else {
        fprintf(stderr,
                "Slot sync disabled: frames are transmitted immediately/back-to-back (diagnostic).\n");
    }

    size_t global_frame = 0;
    try {
        for (int r = 0; r < repeat; ++r) {
            for (size_t i = 0; i < payloads.size(); ++i, ++global_frame) {
                const uint8_t flags = frame_flags(i, payloads.size(), type3);
                const auto tones = js8call::encode_tones79(payloads[i], flags, phy.costas);
                std::array<unsigned char, 79> symbols{};
                for (size_t n = 0; n < tones.size(); ++n) {
                    symbols[n] = static_cast<unsigned char>(tones[n] & 7u);
                }

                if (sync_slots) {
                    // Target the mode-specific JS8 first-tone point. SetSymbols() must be
                    // entered slightly before that point because fskburst primes the PCM FIFO
                    // before enabling GPIO4.
                    int64_t rf_target_ns =
                        (next_slot + phy.tone_start_delay_ms + timing_adjust_ms) * 1000000LL;
                    int64_t dma_start_ns = rf_target_ns - startup_lead_ns;

                    // If the previous frame or scheduler made us miss this slot, never transmit
                    // a late JS8 frame. Advance by whole mode periods until the DMA start is in
                    // the future. The small 2 ms guard avoids starting on an already-expiring edge.
                    int skipped = 0;
                    const int64_t now_ns = realtime_ns();
                    while (dma_start_ns <= now_ns + 2000000LL) {
                        next_slot += phy.period_ms;
                        rf_target_ns += phy.period_ms * 1000000LL;
                        dma_start_ns += phy.period_ms * 1000000LL;
                        ++skipped;
                    }

                    if (skipped) {
                        fprintf(stderr,
                                "Warning: missed %d %s slot(s); realigned before frame %zu/%zu\n",
                                skipped,
                                phy.designator,
                                global_frame + 1,
                                total_frames);
                    }

                    fprintf(stderr,
                            "TX frame %zu/%zu: waiting for %s slot, RF first-tone target "
                            "epoch_ms=%.3f\n",
                            global_frame + 1,
                            total_frames,
                            phy.designator,
                            static_cast<double>(rf_target_ns) / 1000000.0);
                    wait_until_realtime_ns(dma_start_ns);
                } else {
                    fprintf(stderr, "TX frame %zu/%zu: immediate\n",
                            global_frame + 1, total_frames);
                }

                // Send exactly one JS8 frame. SetSymbols() blocks until this burst is complete.
                tx.SetSymbols(symbols.data(), static_cast<uint32_t>(symbols.size()));

                if (sync_slots) {
                    next_slot += phy.period_ms;
                }
            }
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "Transmission timing error: %s\n", e.what());
        tx.stop();
        return 1;
    }

    tx.stop();
    return 0;
}
