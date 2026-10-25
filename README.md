# js8pitx - JS8 transmitter for librpitx

`js8pitx` creates and transmits JS8Call-compatible JS8 frame sequences with
[`librpitx`](https://github.com/F5OEO/librpitx) on a Raspberry Pi.

This allows a Raspberry Pi to be used as a low-power HF transmitter, with an output of roughly 10 mW.
Combined with a separate receiver, it can be used for JS8Call QSOs with other amateur radio operators and,
with favorable propagation and a good antenna, even for DX contacts.

The Pi will create a square wave signal, so always use an appropriate low- or band-pass filter between the Raspberry Pi and the antenna when transmitting;
observe the applicable amateur-radio regulations and band plan.

The signal is generated directly on GPIO4. `--freq` is the actual RF carrier
frequency in Hz, not a transceiver dial frequency plus an audio offset.

## JS8 speed modes

NORMAL speed mode is the default. The other standard, enabled JS8Call speed modes are
selected explicitly:

| Flag | JS8 submode | Symbol rate / tone spacing | UTC period | Nominal first tone | Costas |
| --- | --- | ---: | ---: | ---: | --- |
| *(none)* / `--normal` | JS8A NORMAL | 6.25 baud / 6.25 Hz | 15 s | +500 ms | ORIGINAL |
| `--fast` | JS8B FAST | 10 baud / 10 Hz | 10 s | +200 ms | MODIFIED |
| `--turbo` | JS8C TURBO | 20 baud / 20 Hz | 6 s | +100 ms | MODIFIED |
| `--slow` | JS8E SLOW | 3.125 baud / 3.125 Hz | 30 s | +500 ms | MODIFIED |

Only one speed flag may be specified. JS8I/ULTRA is intentionally not exposed,
because it is currently disabled in upstream JS8Call.

## Timing and slot synchronization

Slot synchronization is enabled by default for every speed mode. The program
initializes `librpitx` first, chooses the next UTC period boundary for the
selected mode, waits until the proper transmit point, and then sends exactly one
79-symbol frame.

For example:

```text
NORMAL / JS8A
slot boundary     00.000
first tone        00.500
next frame        15.500

FAST / JS8B
slot boundary     00.000
first tone        00.200
next frame        10.200

TURBO / JS8C
slot boundary     00.000
first tone        00.100
next frame         6.100

SLOW / JS8E
slot boundary     00.000
first tone        00.500
next frame        30.500
```

Multi-frame messages and repetitions are **not** transmitted continuously when
synchronization is enabled. Each 79-symbol frame gets the next period of the
selected submode. If execution is too late for the intended slot, the program
skips whole periods instead of transmitting a late frame.

`fskburst` fills its 64-word PCM FIFO before enabling GPIO4. The transmitter
therefore enters `SetSymbols()` roughly one PCM sample before the desired RF
first-tone edge. The compensation is derived from the selected symbol rate and
upsampling factor. `--timing-adjust-ms` is retained for calibration of a
particular Raspberry Pi with an SDR.

```sh
# Start the RF edge 20 ms earlier than the nominal mode timing
sudo ./js8pitx --freq 7078000 --cq "CQ" --call DL1ABC --timing-adjust-ms -20

# Start it 20 ms later
sudo ./js8pitx --freq 7078000 --cq "CQ" --call DL1ABC --timing-adjust-ms 20
```

A system clock synchronized by NTP or GPS is recommended.

`--no-sync` disables this behavior and sends frames immediately/back-to-back.
It is intended for diagnostics rather than normal JS8Call interoperability.

## JS8Call compatibility

The implementation provides the framing and channel-encoding paths required for
JS8Call-compatible transmission:

- JS8Call's 64-character payload alphabet and 72-bit payload packing
- CRC-12 and JS8Call's XOR 42
- JS8Call's 87-bit parity matrix
- ORIGINAL and MODIFIED Costas sequences and the 79-symbol arrangement
- legacy Huffman free-text packing
- CQ/heartbeat structured frames
- directed QSO/control frames
- directed keyboard-to-keyboard free text
- JS8Call `FIRST` / middle / `LAST` flags across multi-frame messages

## Supported payloads

### Undirected free text

```sh
# NORMAL is the default
sudo ./js8pitx --freq 7078000 --text "CQ CQ"

sudo ./js8pitx --freq 7078000 --fast --text "FAST TEST"
sudo ./js8pitx --freq 7078000 --turbo --text "TURBO TEST"
sudo ./js8pitx --freq 7078000 --slow --text "SLOW TEST"
```

Long text is split into multiple payload frames. With synchronization enabled,
each resulting frame occupies the next slot of the selected mode.

### CQ and heartbeat

```sh
sudo ./js8pitx --freq 7078000 --cq "CQ" --call DL1ABC
sudo ./js8pitx --freq 7078000 --fast --cq "CQ DX" --call DL1ABC --grid JO62
sudo ./js8pitx --freq 7078000 --slow --heartbeat --call DL1ABC --grid JO62
```

Supported CQ types include `CQ`, `CQ CQ`, `CQ CQ CQ`, `CQ DX`, `CQ QRP`,
`CQ CONTEST`, `CQ FIELD`, and `CQ FD`.

### Directed free text

```sh
sudo ./js8pitx --freq 7078000 \
  --directed --from DL1ABC --to K1ABC \
  --message "HELLO JOHN HW CPY?"
```

The directed command-31 marker and following Huffman data frames retain the
normal JS8Call FIRST/middle/LAST message-boundary behavior. The selected speed
changes the PHY timing/Costas sequence, not the payload packing.

### Directed QSO/control commands

```sh
sudo ./js8pitx --freq 7078000 --turbo \
  --directed --from DL1ABC --to K1ABC --cmd SNR --snr -12

sudo ./js8pitx --freq 7078000 --directed --from DL1ABC --to K1ABC --cmd RR
sudo ./js8pitx --freq 7078000 --directed --from DL1ABC --to K1ABC --cmd QSL
sudo ./js8pitx --freq 7078000 --directed --from DL1ABC --to K1ABC --cmd 73
sudo ./js8pitx --freq 7078000 --directed --from DL1ABC --to K1ABC --cmd SK
```

## Frequency selection

`--freq` specifies the actual RF carrier frequency in Hz. With a conventional
transceiver, JS8Call normally displays an audio offset above a USB dial
frequency. Since `js8pitx` generates RF directly, use the desired actual RF
frequency.

## Diagnostics

`--dry-run` performs payload generation and prints the selected PHY parameters
without initializing RF hardware:

```sh
./js8pitx --freq 7078000 --fast --text "TEST" --dry-run
```

`--raw12` accepts an already-packed 12-character payload. `--type` is valid only
with `--raw12` and defaults to `3` (`FIRST|LAST`).

## Build

```sh
sudo apt-get install git

git clone https://github.com/matthias-straub/js8pitx.git
cd js8pitx

git clone https://github.com/F5OEO/librpitx.git
cd librpitx/src
make
cd ../../

make
```

## CLI options

```text
--freq <rf_hz>                 Actual RF carrier frequency in Hz (required)

Payload modes -- choose exactly one:
--text <TEXT>                  Undirected free-text data; splits into frames as needed
--cq <TYPE>                    CQ frame; requires --call; --grid is optional
--heartbeat                    Heartbeat frame; requires --call; --grid is optional
--directed                     Directed mode; requires --from, --to and exactly one
                               of --cmd or --message
--raw12 <PAYLOAD12>            Pre-packed 12-character diagnostic payload

CQ / heartbeat:
--call CALL                    Originating callsign
--grid GRID                    Optional four-character Maidenhead locator

Directed traffic:
--from CALL                    Sending callsign
--to CALL                      Destination callsign
--cmd COMMAND                  Directed JS8Call command
--message TEXT                 Directed keyboard-to-keyboard free text
--snr DB                       SNR value for SNR / HEARTBEAT SNR

Speed:
--normal                       JS8A NORMAL (default)
--fast                         JS8B FAST
--turbo                        JS8C TURBO
--slow                         JS8E SLOW

Timing / transmission:
--repeat <n>                   Repeat the complete message n times; default 1
--no-sync                      Disable selected-mode UTC slot synchronization
--timing-adjust-ms <ms>        RF first-tone timing adjustment, -5000..+5000; default 0
--level <0..7>                 PAD drive level; default 7
--ramp <0..0.5>                Symbol transition ramp fraction; default 0.1

Diagnostics:
--dry-run                      Encode/print without initializing RF hardware
--type <0..7>                  Raw-frame flags; only valid with --raw12; default 3
--help                         Show command-line help
```
