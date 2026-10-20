# js8pitx JS8A/NORMAL-mode transmitter for librpitx

`js8pitx` creates and transmits a JS8A (normal-speed) frame sequence with librpitx on a Raspberry Pi.

It sends through pin GPIO4 and does not require a transmitter. See https://github.com/F5OEO/librpitx for details.

## JS8Call compatibility

The implementation ports the JS8Call v2.2 framing and channel encoder logic needed for JS8A frame sequences:

- JS8Call's 64-character payload alphabet and 72-bit payload packing
- CRC-12 using the same Boost `augmented_crc<12, 0xC06>` recurrence, followed by JS8Call's XOR 42
- JS8Call's 87-bit parity matrix
- ORIGINAL Costas sequences and the 79-symbol arrangement
- Normal-mode free-text Huffman packing from JS8Call's legacy `Varicode::packDataMessage` path
- Structured heartbeat/CQ frames using JS8Call's `[type|call|grid|subtype]` layout
- Structured directed frames using JS8Call's `[type|from|to|command|portable flags|number]` layout
- JS8Call `FIRST`/middle/`LAST` transmission flags for generated frame sequences

The transmitter uses `fskburst` and its frequency-step/ramp mechanism. On a Pi 4, `js8pitx` uses 32 samples per symbol: the PCM DMA sample clock is therefore 200 Hz while the transmitted symbol rate remains 6.25 baud. This is necessary because 8 samples per symbol requests a 50 Hz PCM clock, which produces an invalid PCM divider with this librpitx implementation.

## Supported payloads

Structured and raw payload modes send one 79-symbol frame. `--text` may produce multiple consecutive frames for long messages. Those frames use the same JS8Call transmission flags as the upstream client: `FIRST` on the first frame, no boundary flag on intermediate frames, and `LAST` on the final frame. `--repeat` repeats the complete encoded message sequence.

### Free text

`--text` produces legacy JS8Call normal-mode **Huffman** data frames. Input is converted to uppercase. Long text is split at whole-character boundaries into as many 72-bit payloads as necessary, using JS8Call's legacy Huffman prefix, capacity, and padding convention. Text must use JS8Call's legacy Huffman character table.

Stock JS8Call also has an optional dictionary-based dense-compression encoder and chooses it when it can carry more text than Huffman. That large compression dictionary is not included in this lightweight transmitter; the emitted Huffman frames remain standard, decodable JS8Call frames, but may require more frames than the desktop client for some text.

```sh
# Single-frame text
sudo ./js8pitx --freq 7078000 --text "CQ CQ" --repeat 1

# Longer text: automatically sent as a FIRST/middle/LAST frame sequence
sudo ./js8pitx --freq 7078000 --text "THIS IS A LONGER JS8CALL MESSAGE THAT SPANS MULTIPLE NORMAL-MODE FRAMES"
```

### CQ and heartbeat

`--cq` and `--heartbeat` produce standard JS8Call heartbeat-format frames. Both require the transmitting station's callsign; `--grid` is optional. When omitted, the frame uses JS8Call's empty-grid sentinel (`nmaxgrid` / `32767`), which JS8Call decodes as no grid.

```sh
# General CQ without a grid
sudo ./js8pitx --freq 7078000 --cq "CQ" --call N0CALL

# General CQ with a grid
sudo ./js8pitx --freq 7078000 --cq "CQ" --call N0CALL --grid EM73

# Calling DX
sudo ./js8pitx --freq 7078000 --cq "CQ DX" --call N0CALL --grid EM73

# Heartbeat
sudo ./js8pitx --freq 7078000 --heartbeat --call N0CALL --grid EM73
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

### Directed QSO/control frames

`--directed` produces a standard directed frame with a sender callsign, recipient callsign, and one JS8Call command.

```sh
# Send a signal report
sudo ./js8pitx --freq 7078000 \
  --directed --from N0CALL --to K1ABC --cmd SNR --snr -12

# Send common QSO acknowledgements / closing traffic
sudo ./js8pitx --freq 7078000 --directed --from N0CALL --to K1ABC --cmd RR
sudo ./js8pitx --freq 7078000 --directed --from N0CALL --to K1ABC --cmd QSL
sudo ./js8pitx --freq 7078000 --directed --from N0CALL --to K1ABC --cmd 73
sudo ./js8pitx --freq 7078000 --directed --from N0CALL --to K1ABC --cmd SK

# Ask for or announce the grid
sudo ./js8pitx --freq 7078000 --directed --from N0CALL --to K1ABC --cmd "GRID?"
sudo ./js8pitx --freq 7078000 --directed --from N0CALL --to K1ABC --cmd GRID
```

`--snr` is accepted only with `--cmd SNR` or `--cmd "HEARTBEAT SNR"` and must be in the range `-30` through `+31` dB.

The following standard directed commands are currently packable:

```text
SNR?         DIT DIT      NACK         HEARING?      GRID?
>            STATUS?      STATUS       HEARING       MSG
MSG TO:      QUERY        QUERY MSGS   QUERY CALL    ACK
GRID         INFO?        INFO         FB            HW CPY?
SK           RR           QSL?         QSL           CMD
SNR          NO           YES          73            HEARTBEAT SNR
AGN?
```

Standard directed frames accept a normal callsign of two to six characters, optionally suffixed with `/P`. Compound callsigns and multi-frame directed free-text messages are not yet supported by the CLI.

## Build

```text
Build on a Raspberry Pi or compatible Linux host:

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
--freq <rf_hz>                 RF carrier frequency in Hz (required)
--text <TEXT>                  Free-text data message; splits into frames as needed
--cq <TYPE>                    CQ frame; requires --call; --grid is optional
--heartbeat                    Heartbeat frame; requires --call; --grid is optional
--call CALL                    Originating station callsign for CQ/heartbeat
--grid GRID                    Optional four-character Maidenhead locator for CQ/heartbeat
--directed                     Directed command frame; requires --from, --to, --cmd
--from CALL                    Sending callsign for a directed frame
--to CALL                      Destination callsign for a directed frame
--cmd COMMAND                  Directed JS8Call command
--snr DB                       SNR for directed SNR frames (-30 through +31)
--raw12 <PAYLOAD12>            Pre-packed diagnostic payload
--type <0..7>                  Raw frame flags only; default 3 = FIRST|LAST
--repeat <n>                   Repeat frame n times; default 1
--level <0..7>                 PAD drive level; default 7
--ramp <0..0.5>                Symbol transition ramp fraction; default 0.1
```

