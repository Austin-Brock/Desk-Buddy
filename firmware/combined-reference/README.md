# Combined reference sketch

**This is the code that proved all three subsystems work together.** Display,
PIR motion detection, and DFPlayer audio, running on one build at the same
time, on hardware.

It is committed **verbatim**, exactly as it was flashed. It is not tidied,
refactored, or corrected, because its value is that this specific text is
known to work — editing it would discard the only verification this project
has of the three subsystems coexisting.

| | |
|---|---|
| Status | ✅ **Verified on hardware** |
| Role | Reference implementation and regression baseline |
| Relationship to `../desk-buddy/` | Proves the hardware and the approach; the modular firmware is the intended destination and is **not yet verified** |

---

## What it establishes

- The ST7789 display, the HC-SR501 PIR, and the DFPlayer (HW-247A) operate
  **simultaneously** on one build, sharing the breadboard's 5 V and GND rails.
- A face renders on the display while the other two subsystems are active.
- **Motion triggers audio.** This follows from the code rather than from a
  separate observation: `playNextAudio()` is called from exactly one place,
  the new-motion branch of `updateMotion()`, so audio being heard at all means
  a motion edge caused it.
- The DFPlayer command frames are correct in practice, not just on paper.

## What it does *not* establish

- **Whether the blink animation stays smooth during playback.** Not
  characterised. This matters, and see the full-frame redraw note below for
  why it is specifically at risk here.
- **Whether the three-clip rotation completes** across repeated triggers
  (1 → 2 → 3 → 1).
- **Whether the 20-second cooldown behaves** as intended over a long session.
- Long-running stability. Nothing has been left running for an extended
  period.

---

## The pin assignment this sketch settles

```c
#define DF_RX D7
#define DF_TX D6
Serial1.begin(9600, SERIAL_8N1, DF_RX, DF_TX);
```

Arduino-ESP32's signature is `begin(baud, config, rxPin, txPin)`, so this
makes **D6 the MCU's transmit pin** and D7 its receive pin. The DFPlayer's RX
input must therefore be wired to **D6**.

That is also the natural assignment: the XIAO silkscreen labels D6 as TX and
D7 as RX, and GPIO21/GPIO20 are UART0's native TX/RX. Earlier notes in this
repository recorded the DFPlayer's RX on D7, which is the opposite; this
sketch working is the evidence that resolves it. The modular firmware's
`config.h` had the same inversion and has been corrected to match.

---

## How this differs from the modular firmware

Both are legitimate; they make different trade-offs. The differences are
listed because they are what has to be reconciled before
`../desk-buddy/` can replace this sketch.

| | This sketch | `../desk-buddy/` |
|---|---|---|
| Blink | Two states, open/closed, 180 ms | Four-step squint, 214 ms |
| Repaint | **Full `fillScreen` + full face redraw on every blink transition** | Eye band only, 160×72 |
| Data per blink frame | ~150 KB ×2 | ~22.5 KB |
| PIR warm-up | None — triggers accepted immediately at boot | 60 s window, events discarded |
| PIR debounce | None, raw `digitalRead()` every loop | 20 ms sampling, 150 ms edge guard |
| UART write | `Serial1.flush()` — blocks ~10 ms per frame | Buffered, returns immediately |
| Structure | Single file | `config.h` + per-layer modules |
| Verified | **Yes** | No |

### The full-frame redraw is the notable one

This sketch calls `drawFace()` on every blink transition, which clears all
240×320 pixels and redraws the whole face — about 150 KB over SPI, twice per
blink. The modular firmware repaints only the eye band specifically to avoid
that, because a full-frame erase is visible as a black flash.

That makes "does the animation stay smooth during playback" a sharper question
for *this* sketch than it would be for the modular one: the full redraw is the
most expensive thing in the loop, and `Serial1.flush()` blocks on top of it.

### The cooldown handles the first trigger differently

```c
bool cooldownComplete = !hasPlayed || (now - lastAudioTime >= AUDIO_COOLDOWN);
```

A `hasPlayed` flag, rather than the modular firmware's backdated timestamp.
Both work; this one is arguably clearer.

---

## Building it

Same toolchain as the main firmware — see the repository README. Board
`XIAO_ESP32C3`, with **USB CDC On Boot enabled**: this sketch uses D6/D7 for
the DFPlayer, which are also UART0's default pins, and with USB CDC disabled
the serial console would contend with them.

Libraries: Adafruit GFX, Adafruit ST7735/ST7789. No DFPlayer library — the
command frames are assembled inline.
