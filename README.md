<div align="center">

# Desk-Buddy

**A small expressive desk productivity robot.**

An animated face on a 2" IPS panel, driven by a Seeed Studio XIAO ESP32C3,
that blinks like something alive and — once the audio subsystem lands — nudges
you with prerecorded reminders.

[![Platform](https://img.shields.io/badge/platform-Arduino%20%2F%20C%2B%2B-00979D)](https://www.arduino.cc/)
[![Board](https://img.shields.io/badge/board-XIAO%20ESP32C3-0088CC)](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/)
[![Display](https://img.shields.io/badge/display-ST7789%20240x320-4C4C4C)](docs/display.md)
[![Status](https://img.shields.io/badge/status-V1%20in%20progress-orange)](docs/project-roadmap.md)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

</div>

---

## 1. Overview

Desk-Buddy is a desk companion built around a single idea: a device that *looks
alive* earns attention that a notification popup does not. A face that blinks
on its own irregular rhythm sits in peripheral vision and registers as a
presence rather than as a screen.

V1 is an animated face plus prerecorded audio reminders at intervals you
choose, powered over USB. Later versions add presence detection, battery power,
an enclosure, and several robots coordinating through a central service.

This repository is both the working firmware and the engineering record: what
was built, what was chosen over what, and what remains unbuilt.

---

## 2. Current status

| Subsystem | Status | Detail |
|---|---|---|
| Display bring-up | ✅ **Working** | ST7789 240×320, hardware SPI, verified on hardware |
| Animated face | ✅ **Working** | Rounded-rectangle eyes, smile, white on black |
| Blink animation | ✅ **Working** | Non-blocking, randomised interval, partial redraw, no flicker |
| Scheduling model | ✅ **Working** | Cooperative, no `delay()` in the run loop |
| DFPlayer audio playback | ✅ **Verified independently** | HW-247A over UART, speaker, microSD — standalone bench sketch |
| Sequential MP3 playback | ✅ **Three files verified** | `/mp3/0001-0003.mp3` played by track number, commands 20 s apart |
| Motion detection | ✅ **Verified independently** | HC-SR501 on D1, reported over serial |
| Breadboard power distribution | ✅ **Verified** | Shared 5 V / GND rails; audio played with the PIR on the same rails |
| Motion-triggered audio | 🟠 **In progress** | Firmware written for the modular architecture; **not yet run on hardware** |
| Display + audio + motion together | ⬜ **Not tested** | No two subsystems have operated together |
| Reminder scheduling | ⬜ **Planned** | Interval-driven reminders, after motion-triggered audio works |
| Long-running stability | ⬜ **Not tested** | Nothing has been left running for an extended period |
| Battery power | ⬜ **Planned** | Blocked on the display's backlight — see [hardware.md](docs/hardware.md) |
| 3D printed enclosure | ⬜ **Planned** | — |
| Multi-robot coordination | ⬜ **Planned** | V2 |

> **This README distinguishes what runs from what is designed.** ✅ means
> verified on hardware. 🟠 means partially proven or in progress. ⬜ means
> intent only — no wiring, no code.
>
> Note the gap between "verified independently" and "integrated". Three
> subsystems work — display, audio, and motion detection — each proven on its
> own. **No two of them have been verified operating together.** The PIR and
> the DFPlayer have shared breadboard power rails, which proves the supply
> holds up, not that the subsystems cooperate. The display has never been
> connected alongside the other two.

---

## 3. Demo

No photo or video has been captured yet — see
[docs/media](docs/media/README.md). What the robot does right now, in words:

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
above a white arc smile. Every 2.6–6.4 seconds the eyes squint shut over four
frames, hold for 70 ms, and reopen — 214 ms end to end. Only the 160×72 region
containing the eyes is repainted, so there is no flicker and the smile never
visibly redraws.

**The firmware makes no sound.** Audio has been verified separately — the
DFPlayer plays three MP3 files from its microSD card through the speaker — but
that was a standalone bench sketch, not this firmware. Running the face and
the audio together is the current milestone. See [docs/audio.md](docs/audio.md).

### Verification status

| Claim | How it was verified |
|---|---|
| Display initialises at 240×320 | Observed on hardware |
| Face renders as described | Observed on hardware |
| Eyes blink without flicker | Observed on hardware |
| Blink state machine never under/overflows | State machine ported to a simulation, 400 s of virtual time, 83 blinks |
| Documented constants match the firmware | Scripted cross-check of every constant quoted in `docs/` against the sketch |
| SPI transfer timings | **Calculated** from transfer size, not measured |
| Current draw figures | **Datasheet estimates**, not measured on this build |

There is no CI and no automated compile check in this repository yet — the
toolchain lives on the author's machine. See [§12](#12-setup-and-build).

---

## 4. Hardware

| Part | Qty | Interface | Status |
|---|---|---|---|
| Seeed Studio XIAO ESP32C3 | 1 | USB-C | ✅ In use |
| 2.0" 240×320 ST7789 SPI IPS TFT | 1 | 4-wire SPI | ✅ In use |
| DFPlayer module (HW-247A) | 1 | UART 9600 8N1 | ✅ Wired, playback verified *(replacement unit — see [troubleshooting.md](docs/troubleshooting.md))* |
| 3 W 8 Ω speaker | 1 | Differential analog | ✅ Connected, audio verified |
| 16 GB microSD card (FAT) | 1 | Hosted by DFPlayer | ✅ Read by the DFPlayer, three tracks play |
| HC-SR501 PIR sensor | 1 | Digital, 3.3 V out | ✅ Motion detection verified |
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
| Planned | `DFRobotDFPlayerMini` for audio — not yet a dependency |

The firmware is split into an entry point, a configuration header, and one
module per layer; see [§11](#11-repository-structure).

Why Adafruit GFX rather than a faster alternative such as TFT_eSPI: the face
is a handful of filled primitives, not a sprite engine. GFX is the better
documented and more portable of the two, and the animation's cost is dominated
by SPI transfer time, not by the drawing library. If the face later needs
sprites or DMA-driven updates, that is the point to revisit this.

---

## 6. Current architecture

```
                         Desk Buddy
                              |
                       XIAO ESP32C3
                       /            \
                ST7789 Display    DFPlayer Mini   <-- integration
                      |                 |             in progress
                Animated Face        microSD
                   (working)             |
                                      Speaker
```

The firmware is organised in three layers:

```
   Behaviour layer    when things happen
                      implemented: randomised blink scheduling
                      planned:     reminder intervals, presence reactions
          |
   Expression layer   how the robot shows it
                      implemented: resting face, blink animation
                      planned:     moods, mouth animation
          |
   Hardware layer     pins, peripherals, drivers
                      implemented: SPI + ST7789
                      planned:     UART + DFPlayer, PIR input
```

Scheduling is **cooperative and non-blocking**. Every subsystem is called once
per `loop()` with the current `millis()`, does at most one small unit of work,
and returns. There is no `delay()` in the run loop, no RTOS task, and no timer
interrupt. On a single-core part with a 214 ms blink and audio playback coming,
this is a requirement rather than a simplification — the full reasoning is in
**[docs/architecture.md](docs/architecture.md)**.

---

## 7. Wiring

### Display → XIAO ESP32C3 — ✅ verified

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

Audio and PIR wiring is **planned, not yet built**; the proposed tables and the
whole-board pin allocation are in **[docs/wiring.md](docs/wiring.md)**.

---

## 8. Features implemented

Everything in this section runs on hardware today.

- **ST7789 240×320 bring-up** over the ESP32-C3's hardware SPI peripheral,
  with no hardware reset line (the module has none) and no read-back path.
- **Minimalist animated face** — two solid white rounded-rectangle eyes and a
  white arc smile on black, all geometry defined as named constants derived
  from the panel width.
- **Non-blocking blink animation** — a four-state machine (resting → closing →
  closed → opening) advanced one frame per `loop()` call, never blocking.
- **Partial-region redraw** — each frame repaints only the 160×72 eye band,
  22.5 KB instead of 150 KB, which is why there is no visible flicker.
- **Randomised blink interval** (2.6–6.4 s) so the robot reads as alive rather
  than as a metronome.
- **Rollover-safe timing** — unsigned elapsed-time arithmetic throughout, so
  the animation survives the `millis()` wrap at ~49 days of uptime.
- **Serial boot diagnostics** at 115200 baud reporting the pin map and the
  initialised resolution.

---

## 9. Planned features

Nothing in this section is wired or written.

| Feature | Status | Blocked on |
|---|---|---|
| DFPlayer audio in the project firmware | 🟠 Verified standalone, not integrated — [docs/audio.md](docs/audio.md) | Breadboard consolidation, non-blocking port |
| Non-blocking playback scheduling | 🟠 Next task | Replacing the bench sketch's `delay(20000)` with `millis()` scheduling |
| Prerecorded audio reminders | ⬜ Planned | Non-blocking playback |
| Configurable reminder intervals | ⬜ Planned | Reminder scheduler |
| Mouth animation during playback | ⬜ Planned | Audio subsystem (BUSY signal) |
| Expressions / moods | ⬜ Planned | Nothing — unblocked today |
| PIR in the project firmware | 🟠 Verified standalone, not integrated — [docs/wiring.md](docs/wiring.md) | Non-blocking port; resolving the D1 pin collision |
| Motion-triggered reminders | ⬜ Planned | PIR integration |
| mmWave presence sensing | ⬜ Planned | Testing whether the PIR holds a trigger for someone sitting still |
| Battery power | ⬜ Planned | Display backlight cannot be gated in firmware |
| 3D printed enclosure | ⬜ Planned | Final part layout |
| Multi-robot coordination | ⬜ Planned | V1.1, a central service, a protocol |
| Over-the-air updates | ⬜ Planned | Wi-Fi provisioning |

---

## 10. Roadmap

| Phase | Contents | State |
|---|---|---|
| **V1** | Animated face, audio reminders, configurable intervals, USB powered | Display done; audio verified standalone; integration next |
| **V1.1** | PIR presence detection, reminder suppression when away, awake/idle faces | Motion detection verified standalone; integration pending |
| **V2** | Moods and transitions, battery, enclosure, multi-robot + central service, OTA | Planned |

V1 is complete when the robot boots standalone from USB, plays a reminder on a
configured interval, keeps animating smoothly during playback, and survives 24
hours without a reset.

Dependency ordering, exit criteria, backlog, and explicit non-goals:
**[docs/project-roadmap.md](docs/project-roadmap.md)**

All engineering documentation is indexed at **[docs/](docs/README.md)**, and
faults hit along the way are logged in
**[docs/troubleshooting.md](docs/troubleshooting.md)**.

---

## 11. Repository structure

```
Desk-Buddy/
├── README.md
├── LICENSE                       MIT
├── .gitignore
├── firmware/
│   └── desk-buddy/               Arduino sketch (folder name must match .ino)
│       ├── desk-buddy.ino        entry point: boot order + run loop
│       ├── config.h              pin map, geometry, palette, timing
│       ├── display.h / .cpp      hardware layer: ST7789 bring-up
│       └── face.h / .cpp         expression layer: face + blink animation
└── docs/
    ├── architecture.md           system diagram, layers, scheduling model
    ├── hardware.md               BOM, GPIO constraints, power budget
    ├── wiring.md                 connection tables, pin allocation, bring-up
    ├── display.md                ST7789 config, animation architecture
    ├── audio.md                  DFPlayer integration plan (not implemented)
    ├── project-roadmap.md        phases, exit criteria, non-goals
    ├── troubleshooting.md        incident log and fault symptoms
    └── media/                    photos and GIFs (none captured yet)
```

### Where to look for what

| If you want to… | Open |
|---|---|
| Change the face's proportions, colours, or blink rhythm | `firmware/desk-buddy/config.h` |
| Change the pin map | `firmware/desk-buddy/config.h` |
| See how the panel is brought up | `firmware/desk-buddy/display.cpp` |
| See how the face is drawn and animated | `firmware/desk-buddy/face.cpp` |
| See the boot order and the scheduler | `firmware/desk-buddy/desk-buddy.ino` |
| Understand *why* any of it is that way | `docs/` |

The Arduino IDE requires a sketch's folder name to match its `.ino` file name,
which is why the firmware is nested as `firmware/desk-buddy/desk-buddy.ino`
rather than sitting directly under `firmware/`. Every `.cpp` beside it is
compiled into the sketch automatically.

---

## 12. Setup and build

### Prerequisites

- A Seeed Studio XIAO ESP32C3 and a USB-C cable
- The ST7789 panel wired as in [§7](#7-wiring)
- Arduino IDE 2.x **or** `arduino-cli`

### Arduino IDE

1. **Add the ESP32 board package.** *Preferences → Additional boards manager
   URLs*:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
   Then *Boards Manager* → install **esp32** by Espressif Systems.
2. **Select the board.** *Tools → Board → ESP32 Arduino → XIAO_ESP32C3*.
3. **Install the libraries.** *Library Manager* →
   **Adafruit GFX Library** and **Adafruit ST7735 and ST7789 Library**
   (accept the Adafruit BusIO dependency prompt).
4. **Open the sketch**: `firmware/desk-buddy/desk-buddy.ino`.
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
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C3 firmware/desk-buddy
arduino-cli upload -p /dev/cu.usbmodem1101 --fqbn esp32:esp32:XIAO_ESP32C3 firmware/desk-buddy

# Serial console
arduino-cli monitor -p /dev/cu.usbmodem1101 -c baudrate=115200
```

Substitute your own serial port. On macOS it is typically
`/dev/cu.usbmodem*`; on Linux, `/dev/ttyACM*`.

### Expected result

The backlight comes on, the screen clears to black, the face is drawn, and the
eyes begin blinking every few seconds. If the panel stays white or blank,
check that SCL is on D8 and SDA on D10 and that the panel is on 3V3 rather than
5 V — there is no read-back path, so the firmware cannot detect or report a
wiring fault.

### Audio

No setup. The audio subsystem does not exist yet. The microSD layout the
firmware will expect is documented in [docs/audio.md](docs/audio.md).

---

## 13. Design decisions

The choices worth defending, with their costs.

| Decision | Why | What it costs |
|---|---|---|
| **XIAO ESP32C3** | 21×17.8 mm fits behind a 2" panel; native USB serial needs no driver; Wi-Fi/BLE and a battery charger are already on the board for V2 | Single core, no DAC, 11 GPIO |
| **Display clock/data on D8/D10** | The only pins that reach the ESP32-C3 hardware SPI peripheral; anything else falls back to bit-banged SPI, ~10× slower | Those two pins are not available for anything else |
| **`TFT_RST = -1`** | The module exposes no reset pin, so the driver uses the panel's SPI software reset | A wedged panel needs a power cycle; no firmware recovery |
| **White on black** | Suits the IPS contrast, and the erase colour equals the background — clearing a region is one `fillRect` with nothing to restore | A patterned background would need a 150 KB framebuffer the part cannot spare |
| **Solid eyes, no pupils** | Reads friendly and alert from across a desk; cheap to repaint | No gaze direction, which the hardware could not support anyway |
| **Partial eye-band redraw** | 22.5 KB per frame instead of 150 KB; full-frame repaints flash visibly | The repaint region is hand-reasoned, not computed — fine for one animated element |
| **Non-blocking state machine** | A 214 ms blink must not stall audio, and audio must not freeze a blink mid-close | Every subsystem must be written as a state machine |
| **Randomised blink interval** | A fixed interval reads as a metronome; this is the cheapest thing in the firmware that makes the robot feel alive | None |
| **Integer-only interpolation** | The ESP32-C3 has no FPU, and single-pixel precision is sufficient | None |
| **DFPlayer Mini for audio** | The ESP32-C3 has **no DAC**; DFPlayer offloads decoding off the single core and brings its own storage and amplifier | Audio is a black box — no mixing, synthesis, or generated speech |
| **Audio files not committed** | Redistribution rights of the converted clips are not established, and git stores every binary revision forever | Anyone cloning must supply their own clips |
| **Adafruit GFX over TFT_eSPI** | Better documented and more portable; the cost here is SPI transfer time, not drawing | Leaves performance on the table if the face ever needs sprites |

---

## 14. Future ideas

Captured, deliberately unscheduled:

- Persistent configuration in NVS so settings survive a reflash
- A button or rotary encoder, so settings can change without a computer —
  which is what would make persistent configuration worth having
- Pomodoro mode with session state on the face
- Calendar integration: a reminder before the next meeting
- A microphone so the robot reacts to being spoken to
- Ambient light sensing to adapt face brightness
- Capacitive touch, so the robot reacts to being petted
- Eye tracking toward a detected person using mmWave angle data
- A watchdog and peripheral health checks — today a failure presents as a
  frozen face

---

## License

[MIT](LICENSE) © 2026 Austin Brock.

The MIT licence covers the firmware and documentation in this repository. It
does **not** cover any audio clips placed on the microSD card — those are not
distributed here. See [docs/audio.md](docs/audio.md).
