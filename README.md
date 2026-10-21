# js8pitx - JS8 NORMAL-mode transmitter for librpitx

`js8pitx` creates and transmits JS8 (JS8A, normal-speed) frame sequences with
[`librpitx`](https://github.com/F5OEO/librpitx) on a Raspberry Pi.

The signal is generated directly on GPIO4. `--freq` is the actual RF carrier
frequency in Hz, not a transceiver dial frequency plus an audio offset.
Appropriate RF filtering is required before feeding the signal to an antenna;
observe the applicable amateur-radio regulations and band plan.

## JS8Call compatibility

The implementation provides the JS8A/NORMAL framing and channel-encoding paths
needed for interoperable transmission to JS8Call receivers:

- JS8Call's 64-character payload alphabet and 72-bit payload packing
- CRC-12 using the same Boost `augmented_crc<12, 0xC06>` recurrence, followed
  by JS8Call's XOR 42
- JS8Call's 87-bit parity matrix
- ORIGINAL Costas sequences and the 79-symbol NORMAL-mode arrangement
- 6.25 baud symbol rate and 6.25 Hz tone spacing
- Normal-mode free-text Huffman packing from JS8Call's legacy
  `Varicode::packDataMessage` path
- Structured heartbeat/CQ frames using JS8Call's
  `[type|call|grid|subtype]` layout
- Structured directed frames using JS8Call's
  `[type|from|to|command|portable flags|number]` layout
- Directed keyboard-to-keyboard free text: a directed command-31 marker followed
  by standard Huffman data frames
- JS8Call `FIRST`/middle/`LAST` transmission flags across complete frame
  sequences
- 15-second JS8A slot synchronization with the nominal first-tone start 500 ms
  into each slot

Only JS8A/NORMAL is implemented. Other JS8Call speed modes are not generated.

The transmitter uses `fskburst` and its frequency-step/ramp mechanism. On a
Pi 4, `js8pitx` uses 32 samples per symbol: the PCM DMA sample clock is 200 Hz
while the transmitted symbol rate remains 6.25 baud. This avoids requesting a
50 Hz PCM clock, which produces an invalid PCM divider with this librpitx
implementation.

## Timing

Normal JS8A transmissions use 15-second UTC periods. By default `js8pitx`
initializes the RF hardware first, waits for the next usable period, and starts
each frame at the nominal JS8A first-tone point:

```text
UTC slot boundary          00.000 s
nominal first JS8A tone    00.500 s
next frame                 15.500 s
next frame                 30.500 s
...
```

Each frame of a multi-frame message occupies its own 15-second period. Frames
are not transmitted continuously back-to-back during normal synchronized
operation.

`librpitx` has a small startup latency while the PCM/DMA path is primed. The
transmitter accounts for an approximately 5 ms startup lead and provides
`--timing-adjust-ms` for calibration on a particular Raspberry Pi:

```sh
# Move the nominal RF first-tone edge 20 ms earlier
sudo ./js8pitx --freq 7078000 --cq "CQ" --call DL1ABC \
  --timing-adjust-ms -20

# Move it 20 ms later
sudo ./js8pitx --freq 7078000 --cq "CQ" --call DL1ABC \
  --timing-adjust-ms 20
```

The allowed adjustment range is `-5000` through `+5000` ms. A system clock
synchronized by NTP or GPS is recommended.

`--no-sync` disables slot synchronization and sends frames immediately. It is
intended for diagnostics; normal JS8Call interoperability should use the
default synchronized mode.

## Supported payloads

Generated messages use JS8Call transmission-boundary flags automatically:

- a one-frame message uses `FIRST|LAST`
- the first frame of a multi-frame message uses `FIRST`
- intermediate frames have no boundary flag
- the final frame uses `LAST`

`--repeat` repeats the complete encoded message sequence. With synchronization
enabled, every repeated frame is placed in the next 15-second period.

### Undirected free text

`--text` produces legacy JS8Call NORMAL-mode Huffman data frames. Input is
converted to uppercase. Long text is split at whole-character boundaries into
as many 72-bit payloads as necessary, using JS8Call's legacy Huffman prefix,
capacity, and padding convention.

Text must use characters present in JS8Call's legacy Huffman table.

```sh
# Short free text
sudo ./js8pitx --freq 7078000 --text "CQ CQ"

# Longer text: automatically sent as FIRST / middle / LAST frames
sudo ./js8pitx --freq 7078000 \
  --text "THIS IS A LONGER JS8CALL MESSAGE THAT SPANS MULTIPLE NORMAL MODE FRAMES"
```

Stock JS8Call can also use dictionary-based dense compression when it is more
efficient. That dictionary is not included here. The Huffman frames generated
by `js8pitx` remain standard JS8Call frames, but some messages require more
frames than they would in the desktop client.

Undirected text is not addressed to a callsign. For a normal keyboard-to-
keyboard QSO, use directed free text instead.

### CQ and heartbeat

`--cq` and `--heartbeat` produce JS8Call heartbeat-format frames. Both require
the transmitting station's callsign; `--grid` is optional. When omitted, the
frame uses JS8Call's empty-grid sentinel and decodes as no grid.

```sh
# General CQ without a grid
sudo ./js8pitx --freq 7078000 --cq "CQ" --call DL1ABC

# General CQ with a grid
sudo ./js8pitx --freq 7078000 --cq "CQ" --call DL1ABC --grid JO62

# Calling DX
sudo ./js8pitx --freq 7078000 --cq "CQ DX" --call DL1ABC --grid JO62

# Heartbeat
sudo ./js8pitx --freq 7078000 --heartbeat --call DL1ABC --grid JO62
```

Supported CQ types are:

- `CQ`
- `CQ CQ`
- `CQ CQ CQ`
- `CQ DX`
- `CQ QRP`
- `CQ CONTEST`
- `CQ FIELD`
- `CQ FD`

### Directed free text

A normal keyboard-to-keyboard JS8Call message is sent with `--directed`,
`--from`, `--to`, and `--message`:

```sh
sudo ./js8pitx --freq 7078000 \
  --directed --from DL1ABC --to K1ABC \
  --message "HELLO JOHN HW CPY?"
```

The encoded sequence consists of a standard `FrameDirected` containing command
31 (the JS8Call directed-freetext marker), followed by one or more normal
Huffman data frames. `FIRST` and `LAST` are applied across the complete
sequence.

For example, if the text requires two data frames, the transmission sequence is:

```text
frame 1: directed DL1ABC -> K1ABC, command 31   FIRST
frame 2: Huffman data                           middle
frame 3: Huffman data                           LAST
```

### Directed QSO/control commands

Structured JS8Call commands use `--cmd` instead of `--message`:

```sh
# Signal report
sudo ./js8pitx --freq 7078000 \
  --directed --from DL1ABC --to K1ABC --cmd SNR --snr -12

# Common QSO acknowledgements / closing traffic
sudo ./js8pitx --freq 7078000 --directed --from DL1ABC --to K1ABC --cmd RR
sudo ./js8pitx --freq 7078000 --directed --from DL1ABC --to K1ABC --cmd QSL
sudo ./js8pitx --freq 7078000 --directed --from DL1ABC --to K1ABC --cmd 73
sudo ./js8pitx --freq 7078000 --directed --from DL1ABC --to K1ABC --cmd SK

# Ask for or announce the grid
sudo ./js8pitx --freq 7078000 --directed --from DL1ABC --to K1ABC --cmd "GRID?"
sudo ./js8pitx --freq 7078000 --directed --from DL1ABC --to K1ABC --cmd GRID
```

`--snr` is accepted only with `--cmd SNR` or `--cmd "HEARTBEAT SNR"` and must
be in the range `-30` through `+31` dB.

The following directed commands are packable:

```text
SNR?         DIT DIT      NACK         HEARING?      GRID?
>            STATUS?      STATUS       HEARING       MSG
MSG TO:      QUERY        QUERY MSGS   QUERY CALL    ACK
GRID         INFO?        INFO         FB            HW CPY?
SK           RR           QSL?         QSL           CMD
SNR          NO           YES          73            HEARTBEAT SNR
AGN?
```

Directed frames use JS8Call's compact standard-callsign field. Callsigns must be
encodable by that format; `/P` is supported. Compound callsigns are not
supported for directed frames.

CQ/heartbeat frames use the separate 50-bit alphanumeric callsign field.

## Frequency selection

`--freq` specifies the actual transmitted RF frequency in Hz.

With a conventional transceiver, JS8Call commonly displays an audio-frequency
offset above a dial frequency. `js8pitx` does not use an audio chain, so those
two values must be combined when choosing the GPIO RF carrier.

Conceptually:

```text
RF carrier = USB dial frequency + JS8Call audio offset
```

Use the exact RF frequency appropriate for the station you intend to contact and
for the applicable band plan.

## Diagnostic modes

### Dry run

`--dry-run` performs payload generation and prints the resulting payloads and
frame flags without initializing `librpitx` or transmitting RF:

```sh
./js8pitx --freq 7078000 \
  --directed --from DL1ABC --to K1ABC \
  --message "HELLO JOHN HW CPY?" \
  --dry-run
```

Example output has the form:

```text
JS8A directed-text @ 7078000 Hz, message_frames=3, repeat=1, total_frames=3
  frame 1/3 flags=1 payload=............
  frame 2/3 flags=0 payload=............
  frame 3/3 flags=2 payload=............
Dry run: RF hardware not initialized.
```

The exact payload depends on the callsigns and message.

### Raw payload

`--raw12` accepts an already-packed 12-character JS8Call payload for diagnostic
use. `--type` selects its three transmission flags (`0..7`) and defaults to `3`
(`FIRST|LAST`). `--type` is only valid together with `--raw12`.

```sh
sudo ./js8pitx --freq 7078000 --raw12 0123456789AB --type 3
```

## Build

Build on a Raspberry Pi or compatible Linux host:

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
--snr DB                       SNR value for SNR / HEARTBEAT SNR (-30 through +31)

Timing / transmission:
--repeat <n>                   Repeat the complete message n times; default 1
--no-sync                      Disable 15 s JS8A slot synchronization
--timing-adjust-ms <ms>        RF first-tone timing adjustment, -5000..+5000; default 0
--level <0..7>                 PAD drive level; default 7
--ramp <0..0.5>                Symbol transition ramp fraction; default 0.1

Diagnostics:
--dry-run                      Encode and print frames without initializing RF hardware
--type <0..7>                  Raw-frame flags; only valid with --raw12; default 3
--help                         Show command-line help
```

## Example QSO transmit sequence

The following commands illustrate the transmit side of a simple JS8Call QSO.
Receive-side operation and operator decisions are outside the scope of
`js8pitx`.

```sh
# Call CQ
sudo ./js8pitx --freq 7078000 --cq "CQ" --call DL1ABC --grid JO62

# Send keyboard-to-keyboard text to K1ABC
sudo ./js8pitx --freq 7078000 \
  --directed --from DL1ABC --to K1ABC \
  --message "GE OM TNX CALL UR SNR -12"

# Structured signal report
sudo ./js8pitx --freq 7078000 \
  --directed --from DL1ABC --to K1ABC --cmd SNR --snr -12

# Acknowledge and close
sudo ./js8pitx --freq 7078000 --directed --from DL1ABC --to K1ABC --cmd RR
sudo ./js8pitx --freq 7078000 --directed --from DL1ABC --to K1ABC --cmd 73
```
