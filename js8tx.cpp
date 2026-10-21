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

// JS8A / NORMAL timing. Both JS8Call 2.2 and current JS8Call use a 15 s
// transmission period and start the actual tone sequence 500 ms into it.
constexpr int64_t kJs8aPeriodMs = 15000;
constexpr int64_t kJs8aToneStartDelayMs = 500;

// fskburst pre-fills the 64-word PCM FIFO with 65 DMA writes before enabling
// GPIO4. The first 64 writes can fill the FIFO immediately; the 65th waits for
// roughly one sample at the 200 Hz PCM pacing rate, i.e. about 5 ms. The exact
// GPIO-on-air latency is hardware/OS dependent, hence --timing-adjust-ms.
constexpr int64_t kLibrpitxStartupLeadMs = 5;

int64_t realtime_ms() {
    timespec ts{};
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        throw std::runtime_error(std::string("clock_gettime failed: ") + std::strerror(errno));
    }
    return static_cast<int64_t>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}

int64_t next_js8a_slot_ms() {
    const int64_t now = realtime_ms();
    return ((now / kJs8aPeriodMs) + 1) * kJs8aPeriodMs;
}

void wait_until_realtime_ms(int64_t target_ms) {
    timespec target{};
    target.tv_sec = static_cast<time_t>(target_ms / 1000);
    target.tv_nsec = static_cast<long>((target_ms % 1000) * 1000000);

    while (true) {
        const int rc = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &target, nullptr);
        if (rc == 0) return;
        if (rc == EINTR) continue;
        throw std::runtime_error(std::string("clock_nanosleep failed: ") + std::strerror(rc));
    }
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
            "Timing / frame options:\n"
            "  --no-sync                     Disable 15 s JS8A slot synchronization (diagnostic)\n"
            "  --timing-adjust-ms <ms>       Fine adjustment of nominal RF tone start (default 0)\n"
            "  --type <0..7>                 Override raw-frame flags (default 3 = FIRST|LAST)\n"
            "  --repeat <n>                  Repeat the complete message n times (default 1)\n"
            "  --level <0..7>                PAD drive level (default 7)\n"
            "  --ramp <0..0.5>               Symbol ramp fraction (default 0.1)\n"
            "  --dry-run                     Encode and print frames without touching RF hardware\n"
            "\n"
            "Examples:\n"
            "  %s --freq 7078000 --cq \"CQ DX\" --call DL1ABC --grid JO62\n"
            "  %s --freq 7078000 --directed --from DL1ABC --to K1ABC --message \"HELLO JOHN HW CPY?\"\n"
            "  %s --freq 7078000 --directed --from DL1ABC --to K1ABC --cmd SNR --snr -12\n"
            "  %s --freq 7078000 --directed --from DL1ABC --to K1ABC --cmd 73\n",
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

    dbg_setlevel(1);

    std::vector<std::string> payloads;
    const char* mode = "raw";
    try {
        if (!raw12.empty()) {
            payloads.push_back(raw12);
        } else if (text_provided) {
            payloads = js8call::pack_normal_huffman_frames(text);
            mode = "text";
        } else if (!cq_type.empty()) {
            payloads.push_back(js8call::pack_heartbeat_frame(call, grid, cq_type));
            mode = "cq";
        } else if (heartbeat) {
            payloads.push_back(js8call::pack_heartbeat_frame(call, grid));
            mode = "heartbeat";
        } else if (directed_text_provided) {
            payloads = js8call::pack_directed_text_frames(from, to, directed_text);
            mode = "directed-text";
        } else {
            payloads.push_back(
                js8call::pack_directed_frame(from, to, command, snr, snr_provided));
            mode = "directed";
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "Encoding error: %s\n", e.what());
        return 1;
    }

    fprintf(stderr,
            "JS8A %s @ %llu Hz, message_frames=%zu, repeat=%d, total_frames=%zu\n",
            mode,
            static_cast<unsigned long long>(rf_freq),
            payloads.size(),
            repeat,
            payloads.size() * static_cast<size_t>(repeat));

    for (size_t i = 0; i < payloads.size(); ++i) {
        const uint8_t flags = frame_flags(i, payloads.size(), type3);
        fprintf(stderr, "  frame %zu/%zu flags=%u payload=%s\n",
                i + 1,
                payloads.size(),
                static_cast<unsigned>(flags),
                payloads[i].c_str());
    }

    if (dry_run) {
        fprintf(stderr, "Dry run: RF hardware not initialized.\n");
        return 0;
    }

    // JS8A NORMAL: 6.25 baud, 6.25 Hz tone spacing.
    const float symbol_rate = 6.25f;
    const float deviation_hz = 6.25f;

    // Pi 4 / librpitx needs 32 samples per symbol here, yielding a 200 Hz PCM
    // DMA pacing clock while keeping the RF symbol rate at 6.25 baud.
    const size_t upsample = 32;
    const uint32_t fifo = 79 + 16;  // one JS8 frame plus margin; frames are sent separately

    padgpio pad;
    pad.setlevel(pad_level);

    // Initialize hardware before choosing the slot, so setup latency cannot make us miss it.
    fskburst tx(rf_freq, symbol_rate, deviation_hz, 14, fifo, upsample, ramp_frac);

    int64_t next_slot = 0;
    if (sync_slots) {
        try {
            next_slot = next_js8a_slot_ms();
        } catch (const std::exception& e) {
            fprintf(stderr, "Timing error: %s\n", e.what());
            tx.stop();
            return 1;
        }
        fprintf(stderr,
                "Slot sync enabled: period=15.000 s, nominal RF tone start=+500 ms, "
                "librpitx startup lead~=%lld ms, adjustment=%+d ms\n",
                static_cast<long long>(kLibrpitxStartupLeadMs),
                timing_adjust_ms);
    } else {
        fprintf(stderr, "Slot sync disabled: frames are transmitted immediately/back-to-back.\n");
    }

    size_t global_frame = 0;
    const size_t total_frames = payloads.size() * static_cast<size_t>(repeat);

    try {
        for (int r = 0; r < repeat; ++r) {
            for (size_t i = 0; i < payloads.size(); ++i, ++global_frame) {
                const uint8_t flags = frame_flags(i, payloads.size(), type3);
                const auto tones = js8call::encode_tones79(
                    payloads[i], flags, js8call::detail::CostasType::ORIGINAL);

                std::array<unsigned char, 79> symbols{};
                for (size_t n = 0; n < tones.size(); ++n) {
                    symbols[n] = static_cast<unsigned char>(tones[n] & 7u);
                }

                if (sync_slots) {
                    // Start SetSymbols slightly before the desired RF edge. fskburst fills the
                    // 64-word PCM FIFO and the 65th write typically costs about one 200 Hz
                    // pacing interval before GPIO4 is enabled. --timing-adjust-ms moves the
                    // desired RF target; use it to calibrate a particular Pi with an SDR.
                    int64_t dma_start_ms = next_slot + kJs8aToneStartDelayMs +
                                           timing_adjust_ms - kLibrpitxStartupLeadMs;

                    // If the previous frame or the OS made us miss the intended slot, do not
                    // transmit late. Skip whole periods until a valid future slot is available.
                    int skipped = 0;
                    const int64_t now = realtime_ms();
                    while (dma_start_ms <= now + 2) {
                        next_slot += kJs8aPeriodMs;
                        dma_start_ms += kJs8aPeriodMs;
                        ++skipped;
                    }
                    if (skipped) {
                        fprintf(stderr,
                                "Warning: missed %d JS8A slot(s); realigned before frame %zu/%zu\n",
                                skipped,
                                global_frame + 1,
                                total_frames);
                    }

                    const int64_t rf_target_ms = next_slot + kJs8aToneStartDelayMs +
                                                 timing_adjust_ms;
                    fprintf(stderr,
                            "TX frame %zu/%zu: waiting for slot, RF first-tone target epoch_ms=%lld\n",
                            global_frame + 1,
                            total_frames,
                            static_cast<long long>(rf_target_ms));
                    wait_until_realtime_ms(dma_start_ms);
                } else {
                    fprintf(stderr, "TX frame %zu/%zu: immediate\n",
                            global_frame + 1, total_frames);
                }

                tx.SetSymbols(symbols.data(), static_cast<uint32_t>(symbols.size()));

                if (sync_slots) {
                    next_slot += kJs8aPeriodMs;
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
