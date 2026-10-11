# Troubleshooting and incident log

Faults hit during this build, what caused them, and what changed as a result.
Recorded because the useful part of a hardware project's history is the
mistakes, and because a destroyed part is cheap tuition only if the lesson is
written down.

---

## Incident 1 — Reversed 5 V and GND destroyed a DFPlayer Mini

**Severity: one module destroyed. Not repairable, not reused.**

### What happened

During initial breadboard assembly, the XIAO ESP32C3's **5 V and GND
connections to the breadboard power rails were reversed** — 5 V was landed on
the rail the modules took as ground, and ground on the rail they took as
positive.

The DFPlayer Mini connected to those rails **emitted smoke.**

### Outcome

- The damaged DFPlayer Mini was **removed from the project permanently.** It
  is not functional and must not be reused, in this build or any other. A
  module that has smoked has had silicon or a passive component fail
  destructively; there is no basis for trusting it again, and a part that
  works intermittently in a robot you are trying to debug is worse than no
  part at all.
- The breadboard wiring was corrected: 5 V to the red (+) rail, GND to the
  blue (−) rail.
- A **replacement DFPlayer Mini (HW-247A)** was fitted and played all three
  MP3 files successfully. Every audio result in this repository refers to the
  replacement module.
- No other module is known to have been damaged. The XIAO, the display, the
  PIR, the speaker, and the microSD card all still function.

### Why it happened

Breadboard power rails carry no polarity marking that the eye enforces. The
red and blue stripes are a convention, not a key — nothing physically prevents
landing the supply on them backwards, and once it is backwards *every* module
on those rails is reverse-biased simultaneously. Unlike a single mis-seated
jumper, which usually breaks one signal, a reversed rail is a whole-bus fault.

The DFPlayer failed first because it is the module with the least input
protection and the largest current path: a 5 V part with an onboard amplifier
and no reverse-polarity diode.

### What changed as a result

The bring-up procedure in [wiring.md](wiring.md) now requires that rail
polarity be confirmed with a meter **before any module is connected**, and
[hardware.md](hardware.md) carries the warning alongside the power section.

### Lessons

1. **Verify rail polarity with a meter before seating a single module.** Red
   probe on the red rail, black on the blue; the reading must be positive.
   This takes five seconds and is the only check that would have caught this.
2. **Connect power rails first, with nothing else attached.** Confirm, then
   add modules one at a time.
3. **Reversed polarity is a bus-wide fault, not a per-module one.** After any
   polarity error, treat every module that was on the rails as suspect, not
   just the one that visibly failed.
4. **A module that has smoked is scrap.** Retire it and label it, so it cannot
   migrate back into the project from a parts drawer six months later.
5. **Most of these modules have no reverse-polarity protection.** The DFPlayer
   Mini, the HC-SR501, and the display module are all bare breakout boards.
   The XIAO's USB input is the only protected supply in the build.

---

## Reference: the display stays blank

Not an incident, but the most likely first failure when reproducing this
build. There is no read-back path on this ST7789 module — MISO is not wired —
so the firmware cannot detect or report a wiring fault. All diagnosis is
visual.

| Symptom | Check |
|---|---|
| Backlight off | Panel supply: `V` must be on **3V3**, not 5 V |
| White or blank screen, backlight on | `SCL` on D8 and `SDA` on D10. These are SPI clock and MOSI despite the I2C-style names; wiring them to the XIAO's I2C pins (D4/D5) gives exactly this symptom |
| Garbled or shifted image | `DC` on D2 and `CS` on D0 |
| Nothing at all, no serial banner | USB cable or port, and the Serial Monitor's baud rate (115200) |

---

## Reference: the DFPlayer does not respond

| Symptom | Check |
|---|---|
| No audio, no apparent response | Common ground between the XIAO and the module. A missing ground presents as a UART that silently does nothing |
| No audio, module powered | TX/RX orientation: the XIAO's transmit pin goes to the module's **RX** |
| Module ignores early commands | The DFPlayer needs time after power-up before it accepts commands; a command sent too early is simply lost |
| Card not read | FAT formatting, and `/mp3/NNNN.mp3` naming. Strip macOS `.DS_Store` and `._*` files from the card |
| Audible hiss | Grounding, and the 1 kΩ series resistor on the module's RX line |

---

## Reference: the PIR triggers constantly or not at all

| Symptom | Likely cause |
|---|---|
| Output high immediately and stays high | Still inside the 30–60 s warm-up period after power-up |
| Triggers with nobody present | Sensitivity pot set high, warm air currents, direct sunlight, or a nearby switching supply |
| Stops reporting while someone sits still | **Expected behaviour, not a fault.** The HC-SR501 detects motion, not presence. See [hardware.md](hardware.md) |
| Trigger lasts far longer than the motion | The time-delay pot sets how long the output stays high; it can be several minutes at one extreme |
