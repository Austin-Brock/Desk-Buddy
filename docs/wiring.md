# Wiring

Exact connection tables for Desk-Buddy. Anything marked **planned** or
**in progress** has not been wired or verified — do not read those tables as a
record of a working circuit.

---

## 1. Display — ST7789 → XIAO ESP32C3 · **Implemented and verified**

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

Matching firmware definitions (`firmware/desk-buddy/desk-buddy.ino`):

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

## 2. Audio — DFPlayer Mini → XIAO ESP32C3 · **Planned, not yet wired**

Proposed wiring for the next hardware milestone. Unverified.

| DFPlayer pin | XIAO pin | GPIO | Notes |
|---|---|---|---|
| VCC | 5V | — | USB bus voltage; **not** the 3V3 rail |
| GND | GND | — | Direct to XIAO ground, not daisy-chained via the display |
| RX | D3 | 5 | MCU transmit → module receive, **via 1 kΩ series resistor** |
| TX | D1 | 3 | Module transmit → MCU receive |
| SPK_1 | — | — | Speaker + (differential — do not ground) |
| SPK_2 | — | — | Speaker − (differential — do not ground) |
| BUSY | *optional* | — | Low while playing; useful later for gating animation |

```
   XIAO ESP32C3                          DFPlayer Mini
  +--------------+                      +---------------+
  |           5V |----------------------| VCC           |
  |          GND |----------------------| GND           |
  |   D3  (GPIO5)|------[ 1k ohm ]------| RX            |
  |   D1  (GPIO3)|----------------------| TX            |
  +--------------+                      |               |
                                        | SPK_1  SPK_2  |
                                        +---+-------+---+
                        100 uF bulk cap     |       |
                        across VCC/GND    +-+-------+-+
                                          | 3W 8ohm   |
                                          | speaker   |
                                          +-----------+
```

Design notes behind these choices:

- **D1/D3 rather than the default UART0 pins (D6/D7).** The ESP32-C3 routes
  UARTs through its GPIO matrix, so a second UART can be assigned to almost
  any pins. Keeping off GPIO20/21 avoids any contention with the default
  console UART, which matters because debugging audio integration over the
  serial monitor is exactly when a pin conflict would be most confusing.
- **Neither pin is a strapping pin.** The module drives its TX line at
  power-on; pointing that at GPIO2, 8 or 9 could alter the boot mode.
- **1 kΩ in series with DFPlayer RX.** Standard practice with this module; it
  limits current into the input and reduces the UART hiss that otherwise
  couples into the amplifier.
- **5 V, not 3V3.** The amplifier's current peaks (~600 mA into 8 Ω) do not
  belong on the XIAO's small onboard regulator.
- **Ground goes straight to the XIAO.** Sharing a return wire with the display
  puts SPI switching current through the amplifier's ground reference, which
  makes the SPI clock audible in the speaker.

See [audio.md](audio.md) for the protocol and the microSD layout.

---

## 3. Presence detection — HC-SR501 PIR → XIAO ESP32C3 · **Planned**

Not wired. Not implemented in firmware.

| HC-SR501 pin | XIAO pin | GPIO | Notes |
|---|---|---|---|
| VCC | 5V | — | Module needs 4.5 V or more |
| GND | GND | — | Common ground |
| OUT | D5 | 7 | ~3.3 V when triggered; safe for an ESP32-C3 input |

```
   XIAO ESP32C3                       HC-SR501 PIR
  +--------------+                   +--------------+
  |           5V |-------------------| VCC          |
  |          GND |-------------------| GND          |
  |   D5  (GPIO7)|-------------------| OUT          |
  +--------------+                   +--------------+
                                      sensitivity + hold-time
                                      trim pots, retrigger jumper
```

D5 is chosen because it is not a strapping pin. The sensor drives its output
high on motion, including during power-up; a strapping pin must not be exposed
to that.

---

## 4. Pin allocation summary

| XIAO | GPIO | Assigned to | Status |
|---|---|---|---|
| D0 | 2 | Display CS | **Implemented** |
| D1 | 3 | DFPlayer TX → MCU RX | **Planned** |
| D2 | 4 | Display DC | **Implemented** |
| D3 | 5 | MCU TX → DFPlayer RX | **Planned** |
| D4 | 6 | *free* | — |
| D5 | 7 | PIR OUT | **Planned** |
| D6 | 21 | *free* (default UART0 TX) | — |
| D7 | 20 | *free* (default UART0 RX) | — |
| D8 | 8 | Display SCL (SPI SCK) | **Implemented** |
| D9 | 9 | *reserved* — BOOT button, strapping pin | avoid |
| D10 | 10 | Display SDA (SPI MOSI) | **Implemented** |

With the display, audio, and PIR all connected, three pins (D4, D6, D7) remain
free for expansion.

---

## 5. Bring-up checklist

For reproducing the current build from parts:

1. Connect GND and 3V3 from the XIAO to the display's G and V. Confirm 3.3 V
   at the panel header with a meter before going further.
2. Connect SCL → D8, SDA → D10, DC → D2, CS → D0. Leave RST unconnected.
3. Flash `firmware/desk-buddy/desk-buddy.ino` (see the README for the toolchain
   setup).
4. Open the serial monitor at 115200 baud. The boot banner reports the pin map
   and the initialised resolution.
5. Expected result: the backlight is on, the screen is black, two white
   rounded-rectangle eyes and a white smile are drawn, and the eyes blink
   every few seconds.

If the screen stays white or blank: check that SCL is on D8 and SDA on D10,
and that the panel is on 3V3 rather than 5 V. There is no read-back path on
this module, so the firmware cannot detect or report a wiring fault — all
diagnosis here is visual.
