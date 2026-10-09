# Audio subsystem — integration plan

> ## Status: NOT IMPLEMENTED
>
> No audio code exists in the firmware. The DFPlayer Mini is not wired to the
> XIAO. No audio has been played by this robot.
>
> What *is* done: the module, speaker, and a 16 GB microSD card are on hand,
> and the card has been formatted FAT and populated with MP3 files under
> `/mp3/`.
>
> This document is the design and the integration plan. Every interface,
> frame, and constant below is **proposed**. Nothing here has been verified
> against hardware.

---

## 1. Why DFPlayer Mini

The decision that forces this choice: **the ESP32-C3 has no DAC.** Unlike the
original ESP32, it has no digital-to-analog converter on the die, so there is
no way to produce analog audio directly from a GPIO beyond PWM tricks that
sound like PWM tricks.

That leaves three real options:

| Option | Decoding | Amplifier | Storage | Pins | Verdict |
|---|---|---|---|---|---|
| DFPlayer Mini | On the module | On the module, ~3 W | Own microSD | 2 (UART) | **Chosen** |
| I2S DAC + amp (e.g. MAX98357A) | On the MCU, in firmware | External | MCU flash or an SD card on the MCU's SPI bus | 3 (I2S) + SPI | Rejected for V1 |
| PWM / PDM straight from a GPIO | On the MCU | External | MCU flash | 1–2 | Rejected |

Why DFPlayer wins for V1:

- **It offloads MP3 decoding entirely.** An I2S path needs an MP3 decoder
  running on the MCU, competing with the animation for the single core. The
  blink animation is the whole point of the robot; making it compete with
  audio decoding for CPU is the wrong trade.
- **It brings its own storage.** The audio lives on the DFPlayer's own
  microSD card, so clips are not limited by the XIAO's 4 MB flash and do not
  need to share the display's SPI bus.
- **It brings its own amplifier.** Driving the 3 W 8 Ω speaker needs no
  additional parts.
- **It costs two pins and a UART.** The display already owns the hardware SPI
  pins; UART routes anywhere through the ESP32-C3 GPIO matrix.

What this costs: audio is a black box. The firmware can ask for a track and
read a busy flag, but it cannot mix, synthesise, generate speech, or know
anything about what is playing. If the robot ever needs generated or streamed
speech, the I2S path becomes the right answer and this subsystem gets
replaced rather than extended. That is an accepted V1 trade-off.

---

## 2. Wiring

See [wiring.md](wiring.md) for the full table and diagram. Summary of the
**planned** connections:

| DFPlayer | XIAO | Note |
|---|---|---|
| VCC | 5V | Not the 3V3 rail — the amplifier's peaks do not belong on the MCU's regulator |
| GND | GND | Direct, not daisy-chained through the display |
| RX | D3 (GPIO5) | Through a 1 kΩ series resistor |
| TX | D1 (GPIO3) | |
| SPK_1 / SPK_2 | — | Differential to the speaker; neither side grounded |
| BUSY | optional | Active-low while playing |

---

## 3. microSD card layout

The card is formatted FAT and currently holds:

```
/mp3/
  0001.mp3
  0002.mp3
  ...
```

This is deliberate, not arbitrary. The DFPlayer firmware recognises a
top-level folder named exactly `mp3` whose files are named with a four-digit
zero-padded number, and exposes them through a dedicated command that plays
"track N from the mp3 folder". The alternative scheme — numbered folders
`01/`, `02/` holding `001.mp3`, `002.mp3` — addresses tracks by folder *and*
index and is worth moving to only if the clips ever need grouping by
category.

Two properties of this module's file handling that the layout has to respect:

- **Track numbers come from the filename, not from the directory order.**
  `0003.mp3` is track 3 regardless of when it was copied onto the card. This
  is what makes the numbering a stable contract between the card and the
  firmware.
- **Copy order can still matter on some module revisions** for the
  non-`/mp3/` addressing modes, and stray files confuse indexing. Keeping the
  card to nothing but `/mp3/NNNN.mp3` avoids the whole class of problem. macOS
  in particular writes `.DS_Store` and `._*` resource forks to removable
  media; these should be deleted from the card before use.

### Track allocation

The numbering is the firmware's contract with the card, so it belongs in
version control even though the audio files do not.

| Track | Intended purpose |
|---|---|
| 0001 | *to be confirmed against the card* |
| 0002 | *to be confirmed against the card* |
| 000N | *to be confirmed against the card* |

This table is deliberately unfilled. Several clips have been converted and
copied to the card, but their mapping to robot behaviours has not been
decided, and guessing it here would put a fiction into the documentation.
Filling this in is step 1 of the integration plan below.

### Why no audio files are in this repository

`.gitignore` excludes `*.mp3` and friends. Two reasons:

1. **Redistribution rights are not established.** The clips were converted
   from existing sources; until each one's licence is known, publishing them
   in a public repository is not something this project will do.
2. **Git is a poor fit for binary media.** Every revision of a clip would be
   stored in full, forever, in everyone's clone.

The repository documents the *layout and numbering* the firmware expects, so
anyone can populate a card with their own clips.

---

## 4. Control protocol

The DFPlayer speaks a fixed 10-byte frame over UART at **9600 baud, 8N1**:

```
  byte  0    1     2     3    4     5      6      7      8     9
       $S   VER   LEN   CMD   FB  PARA_H PARA_L CHK_H  CHK_L   $O
       7E   FF    06    xx    xx    xx     xx     xx     xx    EF
                                          \__ checksum: -(sum of bytes 1..6)
```

Commands this project expects to use:

| CMD | Function | Parameter |
|---|---|---|
| `0x03` | Play track by global index | Track number |
| `0x06` | Set volume | 0–30 |
| `0x0E` | Pause | — |
| `0x0D` | Resume | — |
| `0x16` | Stop | — |
| `0x12` | **Play track N from the `/mp3/` folder** | Track number |
| `0x42` | Query current status | — |

`0x12` is the command the chosen card layout exists to use.

The firmware will drive this through the **DFRobotDFPlayerMini** library
rather than hand-assembling frames, for the same reason it uses Adafruit GFX
rather than writing ST7789 command sequences: the protocol is incidental to
the project. The frame format is documented here anyway, because the first
thing to do when the module does not respond is to put a logic analyser on
the line and check the bytes.

---

## 5. Proposed firmware interface

A sketch of the intended module boundary. **This code does not exist yet.**

```c
/* audio.h -- PROPOSED, NOT IMPLEMENTED */

bool audioBegin();                  /* called from setup(); may block */
void audioUpdate(unsigned long now);/* called from loop(); must not block */

void audioPlayTrack(uint16_t track);/* /mp3/NNNN.mp3 via command 0x12 */
void audioSetVolume(uint8_t vol);   /* 0-30 */
bool audioIsBusy();                 /* true while a clip is playing */
```

Design constraints this interface has to satisfy:

- **`audioUpdate()` must never block.** It follows the same contract as
  `faceAnimationUpdate()`: at most one small unit of work per call. See
  [architecture.md](architecture.md).
- **`audioBegin()` is allowed to block, and will.** The DFPlayer needs time
  after power-up before it accepts commands, and the library's handshake
  waits for an acknowledgement frame. `setup()` is the right place for that;
  `loop()` is not.
- **A command frame takes about 10 ms to clock out** at 9600 baud. That is
  handed to the UART's transmit buffer rather than waited on, so it does not
  stall the animation — but it is why issuing commands in a tight loop is not
  acceptable.
- **Playback completion is polled, not awaited.** Either the BUSY pin or a
  status query; never a blocking wait for a clip to finish. A 10-second clip
  must not freeze the face for 10 seconds.
- **Volume gets a conservative default.** The amplifier can pull more current
  than a 500 mA USB port has spare. The default will be set well below
  maximum, with the headroom problem documented in
  [hardware.md](hardware.md) rather than discovered as a brownout.

---

## 6. Integration plan

Ordered, with an explicit acceptance test per step. No step is complete until
its test passes on hardware.

| Step | Work | Acceptance test |
|---|---|---|
| 1 | Audit the microSD card: confirm FAT32, list `/mp3/`, delete macOS metadata, fill in the track allocation table above | Card contains only `/mp3/NNNN.mp3`; the table in this document matches reality |
| 2 | Move the build to the breadboard; establish a shared 5 V rail and a common ground | Display still runs; 5 V and 3V3 confirmed with a meter |
| 3 | Wire the DFPlayer per [wiring.md](wiring.md), including the 1 kΩ on RX and bulk capacitance on VCC | Module powers up; display unaffected |
| 4 | Speaker across SPK_1/SPK_2 | Continuity confirmed; neither terminal grounded |
| 5 | Add the UART driver and `audioBegin()`; log the handshake result over USB serial | Serial reports a successful DFPlayer handshake |
| 6 | Play track 1 from `setup()` at low volume | Audio is audible and recognisable |
| 7 | Add `audioUpdate()` and non-blocking completion polling | A clip plays through while the eyes keep blinking on schedule, with no visible hitch in the animation |
| 8 | Add the reminder scheduler: configurable interval, selects a clip | A reminder fires on its interval while the face animates normally |
| 9 | Measure current draw at idle and at playback peak | Figures recorded in [hardware.md](hardware.md), replacing the datasheet estimates |

Step 7 is the one that actually matters. Everything before it proves the
module works; step 7 proves the two subsystems coexist, which is the real
question the cooperative scheduling model was designed to answer.

---

## 7. Identified risks

| Risk | Why it is plausible | Planned mitigation |
|---|---|---|
| Audible hiss or SPI clock bleeding into the speaker | Shared ground return with a fast SPI bus is a classic cause | 1 kΩ on DFPlayer RX; DFPlayer ground direct to the XIAO; keep speaker wiring away from the SPI lines |
| Brownout or reset during loud playback | Amplifier peaks (~600 mA) against a 500 mA USB port | Conservative default volume; bulk capacitance; a supply with headroom |
| DFPlayer does not acknowledge the handshake | TX/RX swapped, missing common ground, or module not yet ready after power-up | Check ground first, then swap TX/RX, then add a post-power-up delay before the handshake |
| Clone modules behave differently | DFPlayer Mini is widely cloned; YX5200 and MH2024K variants differ in timing and command support | Verify against the actual module; prefer `/mp3/` addressing, which is the best supported across variants |
| Animation stutters during playback | Serial handling competing with SPI transfers on one core | Step 7's acceptance test exists specifically to catch this |
| Card not recognised | Wrong format, exFAT, or stray metadata files | FAT32, `/mp3/NNNN.mp3` only, metadata stripped |

---

## 8. Open questions

- What are the actual clip contents, and which robot behaviour does each one
  belong to?
- Should reminder intervals be compile-time constants for V1, or stored in
  NVS so they survive a reflash? (Related: there is no input device on the
  robot yet, so there is nothing to configure them *with* — see the
  [roadmap](project-roadmap.md).)
- Should the face react to playback — a mouth animation while a clip plays,
  gated on the BUSY pin? Cheap to do and a large gain in expressiveness, but
  it depends on this subsystem existing first.
- Is the BUSY pin worth a GPIO, or is polling the status query enough?
