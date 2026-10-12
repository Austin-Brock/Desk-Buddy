# Wiring

Exact connection tables for Desk-Buddy. Anything marked **planned** or
**in progress** has not been wired or verified — do not read those tables as a
record of a working circuit.

---

> ## ⚠️ Rail polarity — read before connecting anything
>
> Reversing the XIAO's 5 V and GND connections to the breadboard rails
> **destroyed a DFPlayer Mini on this build.** The module emitted smoke and
> was retired. Breadboard rails carry no polarity key — the red and blue
> stripes are a convention, nothing physically prevents landing the supply
> backwards, and when it is backwards every module on those rails is
> reverse-biased at once.
>
> **Connect the rails first with no modules attached, confirm polarity with a
> meter, then add modules one at a time.** Full account in
> [troubleshooting.md](troubleshooting.md).

---

## 1. Power distribution — XIAO → breadboard rails · **In use**

| XIAO pin | Breadboard rail |
|---|---|
| 5V | Red (+) rail |
| GND | Blue (−) rail |

```
   XIAO ESP32C3
  +--------------+
  |           5V |------------> red (+) rail  ====================
  |          GND |------------> blue (-) rail ====================
  +--------------+                                |          |
                                            HC-SR501     DFPlayer
                                               VCC/GND    VCC/GND
```

The PIR and the DFPlayer both take VCC and GND from these shared rails, and
both have been exercised with that arrangement. The display remains on the
XIAO's 3V3 pin, not the 5 V rail.

---

## 2. Display — ST7789 → XIAO ESP32C3 · **Implemented and verified**

This is the wiring the robot currently runs on.

| ST7789 pin | Label meaning | XIAO pin | GPIO | Notes |
|---|---|---|---|---|
| G | Ground | GND | — | Common ground for the whole build |
| V | Supply | 3V3 | — | Panel is 3.3 V logic and 3.3 V supply |
| SCL | SPI clock (**not** I2C) | D8 | 8 | Hardware SPI SCK; strapping pin |
| SDA | SPI data / MOSI (**not** I2C) | D10 | 10 | Hardware SPI MOSI |
| DC | Data / command select | D2 | 4 | Low = command, high = pixel data |
| CS | Chip select | D0 | 2 | Active low; strapping pin |
| — | Reset | *not connected* | — | Module exposes no RST pin |
| — | Backlight enable | *not present* | — | Backlight is tied on internally |

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
                                      no RST pin on module
                                      no backlight enable pin
```

Matching firmware definitions (`firmware/desk-buddy/config.h`):

```c
#define TFT_CS    D0   /* GPIO2  */
#define TFT_DC    D2   /* GPIO4  */
#define TFT_SCLK  D8   /* GPIO8  -- hardware SPI SCK  */
#define TFT_MOSI  D10  /* GPIO10 -- hardware SPI MOSI */
#define TFT_RST   -1   /* no reset pin on this module */
```

Two mistakes this table is written to prevent:

- **SCL/SDA are SPI, not I2C.** They are named after I2C but this is a 4-wire
  SPI panel. Wiring them to the XIAO's I2C pins (D4/D5) gives a dead display.
- **The clock and data pins are not interchangeable with other GPIOs.** D8 and
  D10 are the ESP32-C3's hardware SPI pins. Moving them elsewhere still works
  electrically, but drops the panel onto bit-banged software SPI and the
  animation becomes visibly slow.

---

## 3. Audio — DFPlayer → XIAO ESP32C3 · **Verified independently**

The DFPlayer (board marking **HW-247A**) is wired and playback is verified:
three MP3 files played from the microSD card through the speaker. See
[audio.md](audio.md).

> **This is the replacement module.** The original DFPlayer Mini was destroyed
> by a reversed breadboard supply and has been retired — see
> [troubleshooting.md](troubleshooting.md). All audio results in this
> repository refer to the replacement.

### As-built wiring

| DFPlayer pin | XIAO pin | GPIO | Status | Notes |
|---|---|---|---|---|
| VCC | 5 V rail | — | **Verified** | Shared breadboard rail; **not** the 3V3 rail |
| GND | GND rail | — | **Verified** | Shared breadboard rail |
| RX | **D6** | 21 | **Verified** | MCU transmit → module receive. D6 is the XIAO's labelled TX pin |
| TX | **D7** | 20 | *Unconfirmed* | Module transmit → MCU receive. One-way control works without it, and nothing reads from the module yet |
| SPK_1 | — | — | **Verified** | Speaker **red** (differential — do not ground) |
| SPK_2 | — | — | **Verified** | Speaker **black** (differential — do not ground) |
| BUSY | *not connected* | — | — | Low while playing; not used yet |

```
   XIAO ESP32C3                          DFPlayer HW-247A
  +--------------+                      +---------------+
  |           5V |---> red (+) rail --->| VCC           |
  |          GND |---> blue (-) rail -->| GND           |
  |  D6 (GPIO21) |----------------------| RX            |
  |   (XIAO "TX")|                      |               |
  |  D7 (GPIO20) |- - - - - - - - - - - | TX  (unconfirmed)
  +--------------+                      |               |
                                        | SPK_1  SPK_2  |
                                        +---+-------+---+
                                            |       |
                                          +-+-------+-+
                                          | 3W 8ohm   |
                                          | red  black|
                                          +-----------+
```

### Three notes on this as-built wiring

- **The direction here is settled by a working build, not by inference.**
  `firmware/combined-reference` — the sketch that demonstrably plays audio —
  configures UART1 as `begin(9600, SERIAL_8N1, rxPin=D7, txPin=D6)`, so the
  MCU transmits on **D6** and the module's RX input is on D6. This is also the
  natural assignment: the XIAO silkscreen labels D6 as TX and D7 as RX. An
  earlier revision of this document recorded the module's RX on D7, which was
  the wrong way round.
- **⚠️ D6/D7 are the ESP32-C3's default UART0 pins** (GPIO21 = TX,
  GPIO20 = RX), which is also where the Arduino serial console lands **if
  "USB CDC On Boot" is disabled.** With USB CDC enabled, `Serial` goes over
  the native USB port and these pins are free for a second UART — which is
  how the firmware configures them. If the console is ever moved back to
  UART0, the DFPlayer traffic and the serial monitor will collide on the same
  two pins. Enable "USB CDC On Boot".
- **This resolves the earlier D1 collision.** An earlier revision of this
  document proposed the DFPlayer on D1/D3, which clashed with the PIR's
  as-built pin. With the DFPlayer on D6/D7 the PIR keeps D1 and nothing
  overlaps.
- **The 1 kΩ series resistor on RX and the bulk capacitor across VCC were
  recommended in the original design; whether they are present in the
  as-built circuit is unrecorded.** Both are noise and transient mitigations
  rather than functional requirements, which is consistent with playback
  working without them, but they remain worth adding before the amplifier
  shares a board with the display's SPI lines.

See [audio.md](audio.md) for the protocol and the microSD layout.

---

## 4. Presence detection — HC-SR501 PIR → XIAO ESP32C3 · **Verified independently**

Motion detection is verified on hardware with a standalone bench sketch. The
PIR is **not** in the project firmware, and it has never been run alongside
the display or the DFPlayer.

### Bench-test wiring — as built

| HC-SR501 pin | XIAO pin | GPIO | Notes |
|---|---|---|---|
| VCC | 5 V rail | — | Shared breadboard rail; module needs 4.5 V or more |
| GND | GND rail | — | Shared breadboard rail |
| OUT | **D1** | **3** | ~3.3 V when triggered; safe for an ESP32-C3 input |

```
   XIAO ESP32C3                       HC-SR501 PIR
  +--------------+                   +--------------+
  |           5V |---> red rail ---->| VCC          |
  |          GND |---> blue rail --->| GND          |
  |   D1  (GPIO3)|-------------------| OUT          |
  +--------------+                   +--------------+
                                      sensitivity + hold-time
                                      trim pots, retrigger jumper
```

Motion detection has been re-confirmed with this breadboard arrangement, with
the DFPlayer sharing the same power rails.

The bench sketch configured D1 as a digital input and printed to the Serial
Monitor at 115200 baud whenever the sensor's output changed state. Observed
output, repeating as motion started and stopped:

```
MOTION DETECTED!
No motion.
MOTION DETECTED!
No motion.
```

That confirms the sensor drives clean HIGH/LOW transitions into an ESP32-C3
GPIO input. It confirms nothing about timing, sensitivity, or reliability.

### D1 is now the adopted pin — the earlier collision is resolved

An earlier revision of this document proposed the DFPlayer on D1/D3, which
clashed with the PIR's as-built pin. **The DFPlayer is now wired on D6/D7**
(section 3), so nothing contends for D1 and the PIR keeps it.

D5 (GPIO7) was the originally proposed production PIR input and is **no longer
needed**; it stays free. D1 (GPIO3) is not a strapping pin, so it is a safe
home for a sensor that drives its output high at power-up — the same property
that made D5 acceptable.

### Unknown hardware settings

None of the following was recorded during the test, and all of them affect how
the sensor behaves in firmware:

- Retrigger mode jumper (H = repeat trigger, L = single trigger): **unknown**
- Sensitivity trim pot: **unknown**
- Time-delay (hold) trim pot: **unknown**
- Measured duration the output stays HIGH after a trigger: **unmeasured**
- False-trigger behaviour: **not established**
- Whether the sensor keeps triggering for a person sitting still: **not
  tested** — this is the limitation that matters most for this robot, see
  [hardware.md](hardware.md)

### Integration status

- Breadboard consolidation is **done**: the PIR and the DFPlayer share the 5 V
  and GND rails, and audio played with both connected.
- Motion detection and audio playback have **not** been demonstrated
  responding to each other — sharing power is not the same as working
  together.
- **The display has never been connected at the same time as these two.**
- The bench sketch read the pin with a raw `digitalRead()` and no debouncing,
  and paced its loop with `delay(100)` — acceptable on the bench, not
  acceptable in the firmware's non-blocking run loop.

---

## 5. Pin allocation summary

| XIAO | GPIO | Assigned to | Status |
|---|---|---|---|
| D0 | 2 | Display CS | **Verified** |
| D1 | 3 | PIR OUT | **Verified** |
| D2 | 4 | Display DC | **Verified** |
| D3 | 5 | *free* | — |
| D4 | 6 | *free* | — |
| D5 | 7 | *free* | — |
| D6 | 21 | MCU TX → DFPlayer RX (UART0's native TX pin) | **Verified** |
| D7 | 20 | MCU RX ← DFPlayer TX (UART0's native RX pin) | *Unconfirmed* — may not be wired |
| D8 | 8 | Display SCL (SPI SCK) | **Verified** |
| D9 | 9 | *reserved* — BOOT button, strapping pin | avoid |
| D10 | 10 | Display SDA (SPI MOSI) | **Verified** |

No pin is contended. Three pins (D3, D4, D5) are free for expansion — D5 was
previously reserved for the PIR and is released now that the sensor is settled
on D1.

Every assignment above has been verified *individually*. **No two of the three
subsystems have been verified operating together**, and the display has never
shared the board with the other two.

---

## 6. What has been tested together, and what has not

The distinction matters more than the individual pin tables, because every
remaining risk in this project lives in the interactions.

| Combination | State |
|---|---|
| Display alone | ✅ Verified — runs in the project firmware |
| DFPlayer alone | ✅ Verified — three tracks played |
| PIR alone | ✅ Verified — motion reported over serial |
| PIR + DFPlayer **sharing power rails** | ✅ Verified — audio played with both on the rails |
| PIR + DFPlayer **interacting** (motion triggers audio) | ⬜ Firmware written, **never run** |
| Display + DFPlayer | ⬜ Never connected together |
| Display + PIR | ⬜ Never connected together |
| **All three together** | ⬜ Never connected together — the full-system goal |

Sharing power rails is not the same as working together: it proves the supply
holds up with two modules attached, not that the firmware can service both
while animating a face.

---

## 7. Bring-up checklist

For reproducing the current build from parts. **Steps 1 and 2 are the ones
that cost a DFPlayer Mini when skipped** — see
[troubleshooting.md](troubleshooting.md).

1. **Rails first, nothing else attached.** XIAO 5 V → red (+) rail,
   GND → blue (−) rail.
2. **Confirm polarity with a meter before seating any module.** Red probe on
   the red rail, black on the blue; the reading must be positive, about 5 V.
3. Connect GND and 3V3 from the XIAO to the display's G and V. Confirm 3.3 V
   at the panel header before going further. The display runs from 3V3, not
   the 5 V rail.
4. Connect SCL → D8, SDA → D10, DC → D2, CS → D0. Leave RST unconnected.
5. Add the remaining modules **one at a time**, checking the display still
   runs after each: PIR (VCC/GND to the rails, OUT → D1), then the DFPlayer
   (VCC/GND to the rails, **RX → D6**, speaker across SPK_1/SPK_2).
6. Flash the sketch at `firmware/desk-buddy/` (see the README for the
   toolchain setup). Ensure **"USB CDC On Boot" is enabled**, or the serial
   console will contend with the DFPlayer on D6/D7.
7. Open the serial monitor at 115200 baud. The boot banner reports the pin map
   and the initialised resolution.
8. Expected result: the backlight is on, the screen is black, two white
   rounded-rectangle eyes and a white smile are drawn, and the eyes blink
   every few seconds.

Fault symptoms and what to check for each are collected in
[troubleshooting.md](troubleshooting.md). Note that there is no read-back path
on the display module, so the firmware cannot detect or report a wiring fault
— all display diagnosis is visual.
