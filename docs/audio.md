# Audio subsystem

> ## Status: STANDALONE PLAYBACK VERIFIED — NOT INTEGRATED
>
> **Verified on hardware:** the DFPlayer module plays MP3 files from its
> microSD card through the speaker, driven over UART by the XIAO ESP32C3.
>
> **Not verified:** audio and the animated display running at the same time.
> The two subsystems have each been proven on their own and have never been
> run together.
>
> **Not implemented:** there is no audio code in the project firmware. The
> verification below was done with a separate standalone bench sketch that
> uses blocking `delay()` calls, which the firmware's scheduling model does
> not permit. See [§1](#1-hardware-verification-october-2026).
>
> The design sections of this document (§2 onward) remain the plan;
> items still marked **proposed** there have not been built.

---

## 1. Hardware verification (October 2026)

**Status: standalone audio playback verified.**

### Verified functionality

- DFPlayer module (board marking **HW-247A**, a DFPlayer Mini variant)
  connected to the XIAO ESP32C3. **This is a replacement unit** — the original
  DFPlayer Mini was destroyed by a reversed breadboard supply and has been
  retired. See [troubleshooting.md](troubleshooting.md). Every result below
  refers to the replacement.
- UART communication operating at 9600 baud.
- 3 W 8 Ω speaker connected across SPK_1 and SPK_2.
- microSD card successfully read by the DFPlayer.
- Three MP3 files played sequentially:
  - `/mp3/0001.mp3`
  - `/mp3/0002.mp3`
  - `/mp3/0003.mp3`
- 20-second intervals between playback commands.
- Audio playback completed successfully.
- Module powered from the **shared breadboard 5 V rail**, with the HC-SR501
  PIR connected to the same rails. Audio played with both modules on the
  supply.
- UART on **D6** (GPIO21) carrying MCU transmit → DFPlayer RX. See
  [wiring.md](wiring.md).

### What the 20 seconds actually means

The test issued a playback command, waited 20 seconds, then issued the next
one. That is **20 seconds between the start of each clip, not 20 seconds of
silence after each clip ends.** The silence between clips is 20 seconds minus
each clip's own duration, and the clip durations were not measured during this
test. If a clip were longer than 20 seconds, the next command would arrive
while it was still playing — which did not happen here, but is not something
this test ruled out.

This distinction matters for the reminder scheduler: a reminder interval has
to be measured from playback *completion*, polled via the BUSY signal or a
status query, not from the previous command.

### What sharing power rails does and does not prove

The PIR and the DFPlayer were on the same 5 V and GND rails while audio
played. That establishes the supply holds up with both modules attached — a
real result, given the amplifier's current peaks.

It does **not** establish that the two subsystems work together: nothing in
the test had motion trigger audio, and the firmware servicing both while
animating a face is a different question again. **The display was not
connected.**

### Implementation details

The standalone test drives the DFPlayer with direct UART commands:

| Command | Function |
|---|---|
| `0x06` | Set volume |
| `0x12` | Play numbered file from `/mp3/` |

It uses `delay(20000)` between playback commands. **That is acceptable for a
standalone bench test and is not acceptable in the firmware.** A 20-second
blocking wait would freeze the blink animation entirely — this is precisely
the failure the cooperative scheduling model exists to prevent. See
[architecture.md](architecture.md).

The bench sketch is not committed to this repository.

### Remaining work

- Integrate audio into the modular firmware.
- Verify simultaneous display animation and audio.
- Implement non-blocking playback scheduling.
- Add reminder functionality.
- Integrate HC-SR501 motion detection.

---

## 2. Why DFPlayer Mini

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

## 3. Wiring

See [wiring.md](wiring.md) for the full table and diagram.

The module is **wired and playback is verified**. The table below is the
wiring this document proposed; the pin assignment and series resistor actually
used in the verified build still need to be recorded here and in
[wiring.md](wiring.md) — see [§9](#9-open-questions).

| DFPlayer | XIAO | Note |
|---|---|---|
| VCC | 5V | Not the 3V3 rail — the amplifier's peaks do not belong on the MCU's regulator |
| GND | GND | Direct, not daisy-chained through the display |
| RX | D3 (GPIO5) | Through a 1 kΩ series resistor |
| TX | D1 (GPIO3) | |
| SPK_1 / SPK_2 | — | Differential to the speaker; neither side grounded |
| BUSY | optional | Active-low while playing |

---

## 4. microSD card layout

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

| Track | File | Plays | Intended purpose |
|---|---|---|---|
| 1 | `/mp3/0001.mp3` | ✅ verified | *not yet assigned* |
| 2 | `/mp3/0002.mp3` | ✅ verified | *not yet assigned* |
| 3 | `/mp3/0003.mp3` | ✅ verified | *not yet assigned* |

All three files have been played successfully by command `0x12`, which
confirms the card layout, the filename-to-track-number mapping, and the
DFPlayer's ability to read the card.

The purpose column is deliberately unfilled: which robot behaviour each clip
belongs to has not been decided, and guessing it here would put a fiction into
the documentation. Clip durations were not measured either — see
[§1](#1-hardware-verification-october-2026).

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

## 5. Control protocol

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

## 6. Firmware interface — **implemented, unverified**

`firmware/desk-buddy/audio.h`. The code exists and has been statically
checked; **it has never been run on hardware.**

```c
void audioBegin(unsigned long now);   /* setup(); opens UART1, non-blocking */
void audioUpdate(unsigned long now);  /* loop(); deferred setup only        */

bool audioPlayTrack(unsigned long now, uint16_t track);  /* command 0x12 */
bool audioSetVolume(unsigned long now, uint8_t volume);  /* command 0x06 */
bool audioIsReady(unsigned long now);
```

Two differences from the interface originally proposed here, both forced by
what the hardware turned out to allow:

- **`audioBegin()` does not block.** The original design assumed a library
  handshake that waits for an acknowledgement frame. Without the module's TX
  line confirmed connected, no acknowledgement is available — so instead of
  waiting, `audioBegin()` records when the module's power-up window expires
  and `audioUpdate()` sends the volume command once it has.
- **There is no `audioIsBusy()`.** Playback completion is not observable on
  this build: BUSY is unwired and no status frame is read. `audioIsReady()`
  reports elapsed time, not module state, and says so. This is the gap that
  the reminder scheduler needs closed.

Design constraints the implementation satisfies:

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
- **Volume gets a conservative default.** `AUDIO_VOLUME` is 18 of 30. The
  amplifier can pull more current than a 500 mA USB port has spare, and the
  robot shares that supply with the display — see [hardware.md](hardware.md).

The frame builder's checksum arithmetic was verified against published
reference frames for commands `0x06` and `0x12` before the code was
committed, which is the one part of this module that can be checked without
hardware.

---

## 7. Integration plan

Ordered, with an explicit acceptance test per step. No step is complete until
its test passes on hardware.

| | Step | Work | Acceptance test |
|---|---|---|---|
| ✅ | 1 | Audit the microSD card and confirm `/mp3/` playback | Three files played by track number |
| ✅ | 3 | Wire the DFPlayer | Module powers up and responds over UART |
| ✅ | 4 | Speaker across SPK_1/SPK_2 | Audio audible through the speaker |
| ✅ | 5 | UART driver and handshake | Module accepts commands at 9600 baud |
| ✅ | 6 | Play a track at a set volume | Three clips played sequentially |
| ✅ | 2 | Move the build to the breadboard; shared 5 V rail and common ground | PIR and DFPlayer both on the rails; audio played. **Display not yet reconnected to the shared build** |
| 🟠 | 7 | Audio alongside the animation | **Partly met.** Audio and a blinking face do run together (`firmware/combined-reference`). The rest of the test — no visible hitch in the animation during playback — is **not characterised**, and the modular `audio.cpp` has never run |
| 🟠 | 8 | A trigger selects and plays a clip | **Motion-triggered playback works.** Interval-driven reminders are still unbuilt |
| ⬜ | 9 | Measure current draw at idle and at playback peak | Figures recorded in [hardware.md](hardware.md), replacing the datasheet estimates |

Steps 3–6 were completed out of order with step 2 — the module was proven on a
standalone bench sketch before the build moved to the breadboard — and step 2
has since been done for the PIR and the DFPlayer. The display has not been
brought onto the shared build yet, which is what step 7 now requires.

**Step 7 is the one that actually matters.** Steps 1–6 proved the module
works. Step 7 proves the two subsystems coexist, which is the real question
the cooperative scheduling model was designed to answer, and it is the step
that replaces the bench sketch's `delay(20000)` with `millis()`-based
scheduling.

---

## 8. Identified risks

| Risk | Status | Why it is plausible | Mitigation |
|---|---|---|---|
| Audible hiss or SPI clock bleeding into the speaker | **Open** — not assessable yet | Shared ground return with a fast SPI bus is a classic cause; the display was not running during the audio test, so this has not been exercised | 1 kΩ on DFPlayer RX; DFPlayer ground direct to the XIAO; keep speaker wiring away from the SPI lines |
| Brownout or reset during loud playback | **Open** | Amplifier peaks (~600 mA) against a 500 mA USB port; the test volume and the panel's concurrent draw are both unrecorded | Conservative volume; bulk capacitance; a supply with headroom; measure in step 9 |
| DFPlayer does not acknowledge commands | **Retired** | TX/RX swapped, missing common ground, or module not ready after power-up | Resolved: the module accepts commands at 9600 baud |
| Clone modules behave differently | **Reduced** | DFPlayer Mini is widely cloned and variants differ in timing and command support | The module in hand is marked **HW-247A** and responds correctly to `0x06` and `0x12`; other commands, notably status queries and the BUSY behaviour, are still unproven on this variant |
| Card not recognised | **Retired** | Wrong format, exFAT, or stray metadata files | Resolved: the card is read and `/mp3/NNNN.mp3` addressing works |
| Animation stutters during playback | **Open — still the main open risk** | The subsystems now run together, but smoothness was not characterised. The reference sketch redraws the full frame per blink and calls `Serial1.flush()`, so it is the worst case for this | Watch the eyes during playback; the modular firmware's partial redraw is the mitigation |
| A clip outlasts its reminder interval | **Open** | The bench test spaced *commands* 20 s apart without measuring clip length, so overlap was never ruled out | Schedule from playback completion (BUSY or status query), not from the previous command |

---

## 9. Open questions

- **Is the 1 kΩ series resistor on DFPlayer RX present in the as-built
  circuit, and is there bulk capacitance across VCC?** Both were in the
  original design and neither is recorded. They are noise and transient
  mitigations rather than functional requirements — consistent with playback
  working — but they matter once the amplifier shares a board with the
  display's SPI lines. *(The pin assignment question is now answered: the
  module's RX is on D6, settled by the working combined sketch.)*
- **Is the DFPlayer's TX line connected to D7 at all?** One-way control works
  without it, but status queries and completion polling need it.
- **What volume value was used**, and what is the actual current draw at that
  volume?
- **How long is each clip?** Needed before any reminder interval can be
  chosen sensibly, and needed to know whether command-spaced playback can
  overlap.
- What are the clip contents, and which robot behaviour does each belong to?
- **Is the BUSY pin worth a GPIO on this HW-247A variant, or is a status query
  enough?** This was a minor question before; the 20-second finding promotes
  it, because non-blocking scheduling needs a reliable completion signal.
- Should reminder intervals be compile-time constants for V1, or stored in
  NVS so they survive a reflash? (Related: there is no input device on the
  robot yet, so there is nothing to configure them *with* — see the
  [roadmap](project-roadmap.md).)
- Should the face react to playback — a mouth animation while a clip plays,
  gated on the BUSY signal? Cheap to do and a large gain in expressiveness.
