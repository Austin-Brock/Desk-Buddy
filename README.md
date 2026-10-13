<div align="center">

# Desk-Buddy

**A small expressive desk productivity robot.**

An animated face on a 2" IPS panel, driven by a Seeed Studio XIAO ESP32C3,
that blinks like something alive and plays a prerecorded clip when it notices
you walk past.

[![Platform](https://img.shields.io/badge/platform-Arduino%20%2F%20C%2B%2B-00979D)](https://www.arduino.cc/)
[![Board](https://img.shields.io/badge/board-XIAO%20ESP32C3-0088CC)](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/)
[![Display](https://img.shields.io/badge/display-ST7789%20240x320-4C4C4C)](docs/display.md)
[![Status](https://img.shields.io/badge/status-working-brightgreen)](docs/README.md)

</div>

---

## 1. Overview

Desk-Buddy is a desk companion built around a single idea: a device that *looks
alive* earns attention that a notification popup does not. A face that blinks
on its own irregular rhythm sits in peripheral vision and registers as a
presence rather than as a screen.

**The robot is built and it works.** Powered over USB, it renders an animated
blinking face, watches for motion with a PIR sensor, and plays a prerecorded
clip when someone walks past — all three subsystems running together on one
build, verified on hardware.

What remains is a 3D printed case to put it in. Everything electrical and
functional is done.

This repository is both the working firmware and the engineering record: what
was built, what was chosen over what, and how each claim was verified.

---

## 2. Current status

| Subsystem | Status | Detail |
|---|---|---|
| Display bring-up | ✅ **Working** | ST7789 240×320, hardware SPI, verified on hardware |
| Animated face | ✅ **Working** | Rounded-rectangle eyes, smile, white on black |
| Blink animation | ✅ **Working** | Non-blocking, randomised interval, no flicker |
| Scheduling model | ✅ **Working** | Cooperative, no `delay()` in the run loop |
| DFPlayer audio playback | ✅ **Working** | HW-247A over UART, speaker, microSD |
| Sequential MP3 playback | ✅ **Working** | `/mp3/0001-0003.mp3` played by track number |
| Motion detection | ✅ **Working** | HC-SR501 on D1 |
| Breadboard power distribution | ✅ **Working** | Shared 5 V / GND rails |
| **Display + audio + motion together** | ✅ **Working** | All three simultaneously — [`firmware/combined-reference`](firmware/combined-reference) |
| **Motion-triggered audio** | ✅ **Working** | A motion event plays the next clip in rotation |
| 3D printed enclosure | ⬜ **The one thing left** | Final part layout |

> **This README distinguishes what runs from what is designed.** ✅ means
> verified on hardware. ⬜ means intent only.
>
> What ran is [`firmware/combined-reference`](firmware/combined-reference) —
> committed verbatim as the verified reference. The modular firmware under
> `firmware/desk-buddy/` is a tidier reorganisation of the same behaviour and
> **has never been flashed**, so it is documented as written, not as working.

---

## 3. Demo

No photo or video has been captured yet — see
[docs/media](docs/media/README.md). What the robot does, in words:

```
  +----------------------------+        +----------------------------+
  |                            |        |                            |
  |      ####      ####        |        |                            |
  |    ########  ########      |        |                            |
  |    ########  ########      |  --->  |    ========  ========      |
  |    ########  ########      |  blink |                            |
  |      ####      ####        |        |                            |
  |                            |        |                            |
  |         \________/         |        |         \________/         |
  |                            |        |                            |
  +----------------------------+        +----------------------------+
        resting  (~2.6-6.4 s)              closed  (70 ms)
```

A black 240×320 portrait panel shows two solid white rounded-rectangle eyes
above a white arc smile. Every few seconds the eyes squint shut and reopen.

**And it reacts.** Walk past it and it plays a clip — the next of three in
rotation, with a 20-second cooldown so a person working at a desk does not
retrigger it constantly. The face stays on screen the whole time.

The one thing not yet characterised is whether the blink animation stays
perfectly smooth while a clip plays. The reference sketch redraws the whole
frame on every blink, which is the thing the modular firmware's partial-redraw
animation exists to avoid. See [docs/audio.md](docs/audio.md).

### Verification status

| Claim | How it was verified |
|---|---|
| Display, motion, and audio run simultaneously | Observed on hardware, `firmware/combined-reference` |
| Motion triggers audio playback | Follows from the code: playback is called from exactly one place, the new-motion branch |
| Display initialises at 240×320 | Observed on hardware |
| Face renders as described | Observed on hardware |
| Eyes blink without flicker | Observed on hardware |
| Blink state machine never under/overflows | State machine ported to a simulation, 400 s of virtual time, 83 blinks |
| Documented constants match the firmware | Scripted cross-check of every constant quoted in `docs/` against the sketch |
| SPI transfer timings | **Calculated** from transfer size, not measured |
| Current draw figures | **Datasheet estimates**, not measured on this build |

There is no CI and no automated compile check in this repository — the
toolchain lives on the author's machine. See [§10](#10-setup-and-build).

---

## 4. Hardware

| Part | Qty | Interface | Status |
|---|---|---|---|
| Seeed Studio XIAO ESP32C3 | 1 | USB-C | ✅ In use |
| 2.0" 240×320 ST7789 SPI IPS TFT | 1 | 4-wire SPI | ✅ In use |
| DFPlayer module (HW-247A) | 1 | UART 9600 8N1 | ✅ In use *(replacement unit — see [troubleshooting.md](docs/troubleshooting.md))* |
| 3 W 8 Ω speaker | 1 | Differential analog | ✅ In use |
| 16 GB microSD card (FAT) | 1 | Hosted by DFPlayer | ✅ In use — three tracks play |
| HC-SR501 PIR sensor | 1 | Digital, 3.3 V out | ✅ In use |
| Breadboard | 1 | — | ✅ In use — shared 5 V / GND rails |
| Dupont jumper wires | — | — | ✅ In use |

Full electrical notes, GPIO constraints, and the power budget:
**[docs/hardware.md](docs/hardware.md)**

> ⚠️ **Reversing the breadboard's 5 V and GND rails destroyed a DFPlayer Mini
> on this build.** None of these modules has reverse-polarity protection.
> Confirm rail polarity with a meter before seating anything —
> [docs/troubleshooting.md](docs/troubleshooting.md).

---

## 5. Software stack

| Layer | Choice |
|---|---|
| Language | C++ (Arduino framework) |
| Toolchain | Arduino IDE 2.x or `arduino-cli` |
| Board package | `esp32` by Espressif → **XIAO_ESP32C3** |
| Graphics | [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library) |
| Display driver | [Adafruit ST7735 and ST7789 Library](https://github.com/adafruit/Adafruit-ST7735-Library) |
| Bus | `SPI` (Arduino core) |
| Audio | **No library** — DFPlayer command frames assembled directly |

The DFPlayer protocol is ten bytes with a checksum and this firmware uses two
of its commands, so a library dependency would cost more than it saves. The
frame format is documented in [docs/audio.md](docs/audio.md) and verified
against published reference frames.

Why Adafruit GFX rather than a faster alternative such as TFT_eSPI: the face
is a handful of filled primitives, not a sprite engine. GFX is the better
documented and more portable of the two, and the animation's cost is dominated
by SPI transfer time, not by the drawing library.

---

## 6. Architecture

```
                         Desk Buddy
                              |
                       XIAO ESP32C3
                    /         |        \
         ST7789 Display   HC-SR501   DFPlayer HW-247A
                |            PIR            |
         Animated Face    (motion)       microSD
           (blinking)                       |
                                         Speaker
```

The firmware is organised in three layers:

```
   Behaviour layer    when things happen
                      randomised blink scheduling
                      motion-triggered clip playback with cooldown
          |
   Expression layer   how the robot shows it
                      resting face, blink animation
          |
   Hardware layer     pins, peripherals, drivers
                      SPI + ST7789, PIR input, UART + DFPlayer
```

Scheduling is **cooperative and non-blocking**. Every subsystem is called once
per `loop()` with the current `millis()`, does at most one small unit of work,
and returns. There is no `delay()` in the run loop, no RTOS task, and no timer
interrupt. On a single-core part with a 214 ms blink and audio playback, this
is a requirement rather than a simplification — the full reasoning is in
**[docs/architecture.md](docs/architecture.md)**.

---

## 7. Wiring

### Display → XIAO ESP32C3

| ST7789 pin | XIAO pin | GPIO | Note |
|---|---|---|---|
| G | GND | — | Common ground |
| V | 3V3 | — | 3.3 V logic and supply |
| SCL | **D8** | 8 | SPI clock — hardware SPI SCK |
| SDA | **D10** | 10 | SPI data — hardware SPI MOSI |
| DC | D2 | 4 | Data / command select |
| CS | D0 | 2 | Chip select, active low |
| RST | *not connected* | — | Module exposes no RST pin |

```
   XIAO ESP32C3                       ST7789 2.0" 240x320
  +--------------+                   +-------------------+
  |          GND |-------------------| G                 |
  |          3V3 |-------------------| V                 |
  |   D8  (GPIO8)|-------------------| SCL  (SPI clock)  |
  |  D10 (GPIO10)|-------------------| SDA  (SPI MOSI)   |
  |   D2  (GPIO4)|-------------------| DC                |
  |   D0  (GPIO2)|-------------------| CS                |
  +--------------+                   +-------------------+
```

Two traps worth repeating: **SCL/SDA are SPI, not I2C** — wiring them to the
XIAO's I2C pins gives a dead display — and **the clock and data pins are not
interchangeable**, because D8/D10 are the only pins that reach the ESP32-C3's
hardware SPI peripheral.

### PIR → XIAO ESP32C3

| HC-SR501 pin | XIAO pin | GPIO | Note |
|---|---|---|---|
| VCC | 5 V rail | — | Shared breadboard rail; needs 4.5 V or more |
| GND | GND rail | — | Shared breadboard rail |
| OUT | **D1** | 3 | ~3.3 V when triggered; safe for an ESP32-C3 input |

### DFPlayer → XIAO ESP32C3

| DFPlayer pin | XIAO pin | GPIO | Note |
|---|---|---|---|
| VCC | 5 V rail | — | Shared breadboard rail; **not** 3V3 |
| GND | GND rail | — | Shared breadboard rail |
| RX | **D6** | 21 | MCU transmit → module receive; D6 is the XIAO's labelled TX |
| TX | D7 | 20 | Module transmit → MCU receive; unused, nothing reads from the module |
| SPK_1 / SPK_2 | — | — | Speaker red / black — differential, do not ground either |
| BUSY | *not connected* | — | Low while playing; not used |

The UART direction is settled by a working build, not by inference:
Arduino-ESP32's `Serial1.begin(baud, config, rxPin, txPin)` makes D6 the MCU's
transmit pin, so the DFPlayer's RX goes to D6. Full tables, the pin allocation
for the whole board, and the rail-polarity warning:
**[docs/wiring.md](docs/wiring.md)**

---

## 8. Features

Everything in this section runs on hardware.

- **ST7789 240×320 bring-up** over the ESP32-C3's hardware SPI peripheral,
  with no hardware reset line (the module has none) and no read-back path.
- **Minimalist animated face** — two solid white rounded-rectangle eyes and a
  white arc smile on black, all geometry defined as named constants derived
  from the panel width.
- **Non-blocking blink animation** advanced one frame per `loop()` call,
  never blocking.
- **Randomised blink interval** (2.6–6.4 s) so the robot reads as alive rather
  than as a metronome.
- **PIR motion detection** on D1, polled in the run loop.
- **DFPlayer audio output** over UART1 on D6/D7, with command frames
  assembled directly and checksums verified against published reference
  frames.
- **Motion-triggered playback** — a new motion event plays the next of three
  clips in rotation, with a 20-second cooldown. Triggers during the cooldown
  are discarded rather than deferred, so the robot never reacts to movement
  from twenty seconds ago.
- **Rollover-safe timing** — unsigned elapsed-time arithmetic throughout, so
  the animation survives the `millis()` wrap at ~49 days of uptime.
- **Serial boot diagnostics** at 115200 baud reporting the pin map and the
  initialised resolution.

### Refinements written but not yet flashed

The modular firmware under `firmware/desk-buddy/` reorganises the above into
per-layer modules and adds refinements that have never been run:
partial-region redraw (22.5 KB per frame instead of 150 KB), a 20 ms PIR
sampling interval with a 150 ms edge guard, a 60-second sensor warm-up window,
and buffered non-blocking UART writes. Treat it as a design, not a feature
list — see [`firmware/combined-reference/README.md`](firmware/combined-reference/README.md)
for a side-by-side comparison with what actually ran.

---

## 9. Repository structure

```
Desk-Buddy/
├── README.md
├── .gitignore
├── firmware/
│   ├── combined-reference/       ✅ VERIFIED: all three subsystems together
│   │   └── combined-reference.ino    committed verbatim, as flashed
│   └── desk-buddy/               modular firmware — written, not yet flashed
│       ├── desk-buddy.ino        entry point: boot, run loop, behaviour
│       ├── config.h              pins, geometry, palette, timing, volume
│       ├── display.h / .cpp      hardware layer: ST7789 bring-up
│       ├── motion.h / .cpp       hardware layer: PIR input, edge detection
│       ├── audio.h / .cpp        hardware layer: DFPlayer UART frames
│       └── face.h / .cpp         expression layer: face + blink animation
└── docs/
    ├── architecture.md           system diagram, layers, scheduling model
    ├── hardware.md               BOM, GPIO constraints, power budget
    ├── wiring.md                 connection tables, pin allocation, bring-up
    ├── display.md                ST7789 config, animation architecture
    ├── audio.md                  DFPlayer protocol, microSD layout
    ├── project-roadmap.md        how the build was sequenced
    ├── troubleshooting.md        incident log and fault symptoms
    └── media/                    photos and GIFs (none captured yet)
```

### Where to look for what

| If you want to… | Open |
|---|---|
| See code that is **known to work on hardware** | `firmware/combined-reference/` |
| Change the face's proportions, colours, or blink rhythm | `firmware/desk-buddy/config.h` |
| Change the pin map, the volume, or the motion cooldown | `firmware/desk-buddy/config.h` |
| See how the panel is brought up | `firmware/desk-buddy/display.cpp` |
| See how the face is drawn and animated | `firmware/desk-buddy/face.cpp` |
| See how motion is sampled and debounced | `firmware/desk-buddy/motion.cpp` |
| See the DFPlayer command frames | `firmware/desk-buddy/audio.cpp` |
| See the boot order, scheduler, and motion→audio policy | `firmware/desk-buddy/desk-buddy.ino` |
| Understand *why* any of it is that way | `docs/` |

The Arduino IDE requires a sketch's folder name to match its `.ino` file name,
which is why the firmware is nested as `firmware/desk-buddy/desk-buddy.ino`
rather than sitting directly under `firmware/`. Every `.cpp` beside it is
compiled into the sketch automatically.

---

## 10. Setup and build

### Prerequisites

- A Seeed Studio XIAO ESP32C3 and a USB-C cable
- The display, PIR, and DFPlayer wired as in [§7](#7-wiring)
- A FAT-formatted microSD card with your clips (see [Audio](#audio) below)
- Arduino IDE 2.x **or** `arduino-cli`

### Which sketch to flash

Flash `firmware/combined-reference/combined-reference.ino` — that is the build
verified on hardware. `firmware/desk-buddy/` is the modular reorganisation and
has never been run.

### Arduino IDE

1. **Add the ESP32 board package.** *Preferences → Additional boards manager
   URLs*:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
   Then *Boards Manager* → install **esp32** by Espressif Systems.
2. **Select the board.** *Tools → Board → ESP32 Arduino → XIAO_ESP32C3*.
   **Ensure *USB CDC On Boot* is enabled.** The DFPlayer uses D6/D7, which are
   also UART0's default pins; with USB CDC disabled the serial console lands
   on those same pins and contends with the audio module.
3. **Install the libraries.** *Library Manager* →
   **Adafruit GFX Library** and **Adafruit ST7735 and ST7789 Library**
   (accept the Adafruit BusIO dependency prompt).
4. **Open the sketch**: `firmware/combined-reference/combined-reference.ino`.
5. **Select the port** and click Upload.
6. **Open the Serial Monitor at 115200 baud.** The boot banner reports the pin
   map and the initialised resolution.

### arduino-cli

```bash
# Board package
arduino-cli core update-index \
  --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core install esp32:esp32 \
  --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

# Libraries
arduino-cli lib install "Adafruit GFX Library" "Adafruit ST7735 and ST7789 Library"

# Confirm the FQBN for your core version
arduino-cli board listall | grep -i xiao

# Build and upload
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C3 firmware/combined-reference
arduino-cli upload -p /dev/cu.usbmodem1101 --fqbn esp32:esp32:XIAO_ESP32C3 firmware/combined-reference

# Serial console
arduino-cli monitor -p /dev/cu.usbmodem1101 -c baudrate=115200
```

Substitute your own serial port. On macOS it is typically
`/dev/cu.usbmodem*`; on Linux, `/dev/ttyACM*`.

### Audio

Format the microSD card **FAT**, and put nothing on it but a top-level folder
named exactly `mp3` holding four-digit-numbered files:

```
/mp3/
  0001.mp3
  0002.mp3
  0003.mp3
```

Tracks are addressed by number, so `0003.mp3` is track 3 regardless of when it
was copied onto the card. Stray files — including macOS `._` metadata —
confuse the DFPlayer's indexing, so keep the card to nothing else. The clips
themselves are **not** committed to this repository; supply your own. Protocol
details and the full rationale: [docs/audio.md](docs/audio.md).

### Expected result

The backlight comes on, the screen clears to black, the face is drawn, and the
eyes begin blinking every few seconds. Walk in front of the PIR and a clip
plays. If the panel stays white or blank, check that SCL is on D8 and SDA on
D10 and that the panel is on 3V3 rather than 5 V — there is no read-back path,
so the firmware cannot detect or report a wiring fault.

---

## 11. Design decisions

The choices worth defending, with their costs.

| Decision | Why | What it costs |
|---|---|---|
| **XIAO ESP32C3** | 21×17.8 mm fits behind a 2" panel; native USB serial needs no driver; Wi-Fi/BLE and a battery charger are already on the board | Single core, no DAC, 11 GPIO |
| **Display clock/data on D8/D10** | The only pins that reach the ESP32-C3 hardware SPI peripheral; anything else falls back to bit-banged SPI, ~10× slower | Those two pins are not available for anything else |
| **`TFT_RST = -1`** | The module exposes no reset pin, so the driver uses the panel's SPI software reset | A wedged panel needs a power cycle; no firmware recovery |
| **White on black** | Suits the IPS contrast, and the erase colour equals the background — clearing a region is one `fillRect` with nothing to restore | A patterned background would need a 150 KB framebuffer the part cannot spare |
| **Solid eyes, no pupils** | Reads friendly and alert from across a desk; cheap to repaint | No gaze direction, which the hardware could not support anyway |
| **Non-blocking state machine** | A 214 ms blink must not stall audio, and audio must not freeze a blink mid-close | Every subsystem must be written as a state machine |
| **Randomised blink interval** | A fixed interval reads as a metronome; this is the cheapest thing in the firmware that makes the robot feel alive | None |
| **Integer-only interpolation** | The ESP32-C3 has no FPU, and single-pixel precision is sufficient | None |
| **DFPlayer for audio** | The ESP32-C3 has **no DAC**; the DFPlayer offloads decoding off the single core and brings its own storage and amplifier | Audio is a black box — no mixing, synthesis, or generated speech |
| **DFPlayer frames hand-assembled, no library** | Ten bytes with a checksum, two commands used; a dependency would cost more than it saves | The frame format and checksum are this project's to get right — verified against published reference frames rather than by trusting a library |
| **Cooldown measured from command send, not playback end** | The DFPlayer's BUSY line is not wired and clip durations are unmeasured, so completion is not observable | A clip longer than the cooldown would be interrupted by the next trigger |
| **Motion events dropped during cooldown, not queued** | A deferred trigger would fire the instant the cooldown ended, making the robot react to movement from twenty seconds earlier | Genuine motion at the end of a cooldown is ignored until the next movement |
| **Audio files not committed** | Redistribution rights of the converted clips are not established, and git stores every binary revision forever | Anyone cloning must supply their own clips |
| **Adafruit GFX over TFT_eSPI** | Better documented and more portable; the cost here is SPI transfer time, not drawing | Leaves performance on the table if the face ever needs sprites |
| **Reference sketch committed verbatim** | Its value is that this exact text is known to work; editing it would discard the only verification of the three subsystems coexisting | Two copies of the behaviour live in the repository |

