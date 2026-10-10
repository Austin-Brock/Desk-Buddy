# Hardware

Bill of materials, electrical notes, and the board-level constraints that shape
the firmware.

> **Status legend used throughout this repository**
>
> | Marker | Meaning |
> |---|---|
> | **Implemented** | Built, wired, and verified running on hardware |
> | **In progress** | Parts on hand, wiring and firmware not yet done |
> | **Planned** | Design intent only; nothing wired, nothing written |

---

## 1. Bill of materials

| # | Part | Qty | Interface | Status |
|---|---|---|---|---|
| 1 | Seeed Studio XIAO ESP32C3 | 1 | USB-C | **Implemented** |
| 2 | 2.0" 240x320 ST7789 SPI IPS TFT | 1 | 4-wire SPI | **Implemented** |
| 3 | DFPlayer module (HW-247A) | 1 | UART, 9600 8N1 | **Playback verified** |
| 4 | 3 W 8 Ω speaker | 1 | Analog, differential | **Playback verified** |
| 5 | 16 GB microSD card (FAT) | 1 | Hosted by DFPlayer | **Playback verified** |
| 6 | HC-SR501 PIR motion sensor | 1 | Digital, 3.3 V out | **Planned** |
| 7 | Breadboard | 1 | — | Ordered, not yet incorporated |
| 8 | Dupont jumper wires | — | — | In use |

Everything on the implemented list is currently assembled with Dupont jumpers
directly between modules. The display and the audio subsystem have been
verified *separately*, each on its own; moving to the breadboard is the
prerequisite for running them together, because sharing a 5 V rail and a
single common ground point between three modules is not something
jumper-to-jumper wiring distributes cleanly.

---

## 2. Microcontroller: Seeed Studio XIAO ESP32C3

| Property | Value |
|---|---|
| SoC | Espressif ESP32-C3, single-core 32-bit RISC-V @ 160 MHz |
| SRAM | 400 KB |
| Flash | 4 MB |
| Logic level | 3.3 V |
| Radio | Wi-Fi 802.11 b/g/n, Bluetooth LE 5 |
| USB | USB-C, native USB Serial/JTAG |
| Exposed GPIO | 11 (D0–D10) |
| Power rails exposed | 5 V (from USB), 3V3 (onboard regulator), GND |
| Battery support | Charge circuit with BAT+/BAT− pads on the underside |

### Why this board

- **It is small enough to disappear into the robot.** At 21 × 17.8 mm the board
  fits behind a 2.0" panel with room left for the DFPlayer, which an ESP32
  DevKit-sized board would not.
- **Native USB serial.** No USB-UART bridge chip, so flashing and the serial
  console need no driver installation and no external adapter.
- **Wi-Fi and BLE are already on the die.** The V2 goal of several robots
  reporting to a central service needs a radio; choosing a board with one now
  avoids a re-layout later, even though V1 does not use it.
- **A battery charger is already on the board.** The V2 goal of running on
  battery needs only a cell and a connector, not a separate charge board.

### GPIO map and constraints

| Silkscreen | GPIO | Notes | Used for |
|---|---|---|---|
| D0 | 2 | **Strapping pin** | Display CS — **Implemented** |
| D1 | 3 | ADC capable | DFPlayer TX → MCU RX — **Planned** |
| D2 | 4 | ADC capable | Display DC — **Implemented** |
| D3 | 5 | ADC capable | MCU TX → DFPlayer RX — **Planned** |
| D4 | 6 | Default I2C SDA | free |
| D5 | 7 | Default I2C SCL | PIR OUT — **Planned** |
| D6 | 21 | Default UART0 TX | free |
| D7 | 20 | Default UART0 RX | free |
| D8 | 8 | **Strapping pin**, hardware SPI SCK | Display SCL — **Implemented** |
| D9 | 9 | **Strapping pin**, wired to the BOOT button | avoid |
| D10 | 10 | Hardware SPI MOSI | Display SDA — **Implemented** |

Three constraints follow from this table and they drive most of the pin
choices:

1. **GPIO2, GPIO8 and GPIO9 are ESP32-C3 strapping pins.** Their state is
   sampled at reset to select the boot mode. Nothing may hold them low while
   the board is resetting. The display's CS (GPIO2) and SCL (GPIO8) are safe
   because the panel presents high-impedance inputs on those lines and never
   drives them. A sensor that actively pulls one of them low at power-on would
   put the board into download mode instead of running the firmware.
2. **GPIO9 is also the BOOT button.** It is treated as unavailable rather than
   as a pin with a caveat.
3. **SPI must land on GPIO8 and GPIO10.** These are the ESP32-C3's hardware
   SPI SCK and MOSI. Any other pair would fall back to bit-banged software
   SPI, which is roughly an order of magnitude slower and would make the eye
   repaints visibly sluggish. UART, by contrast, is routed through the
   ESP32-C3 GPIO matrix and can use almost any pins, which is why the planned
   DFPlayer link is free to sit on D1/D3 and leave the default UART0 pins
   alone.

---

## 3. Display: 2.0" ST7789 240×320 IPS

| Property | Value |
|---|---|
| Controller | Sitronix ST7789 |
| Resolution | 240 × 320 |
| Colour depth | 16-bit RGB565 |
| Interface | 4-wire SPI, write-only in this design |
| Logic / supply | 3.3 V |
| Exposed pins | G, V, SCL, SDA, DC, CS |

Two properties of this specific module matter to the firmware:

- **There is no RST pin.** The module does not break the controller's reset
  line out to the header, so the panel is reset by its SPI software reset
  command during initialisation. In practice this is reliable; the cost is
  that a panel wedged by a malformed SPI transaction cannot be recovered
  without a full power cycle.
- **There is no backlight enable pin.** The backlight is tied on internally
  and cannot be dimmed or switched off in firmware. This is the single largest
  obstacle to battery operation: the panel draws its backlight current
  continuously whenever the robot is powered, and no amount of firmware sleep
  logic can change that. A future battery version will likely need a different
  panel or a MOSFET cut into the backlight supply.

Electrically the panel is an easy load — it is 3.3 V logic driven by a 3.3 V
MCU, so no level shifting is involved anywhere in the display path.

---

## 4. Audio: DFPlayer Mini — **standalone playback verified**

The module plays MP3 files from its microSD card through the speaker, driven
over UART by the XIAO. The module in hand carries the board marking
**HW-247A**, a DFPlayer Mini variant.

What is **not** verified: audio and the display running together, and
therefore none of the interference or power hazards below. Those were
identified while planning and remain unassessed. See [audio.md](audio.md) for
the verification record and the integration plan.

| Property | Value |
|---|---|
| Module | DFPlayer Mini variant, board marking HW-247A |
| Supply | 5 V recommended (3.2–5.0 V range) |
| Control | UART, 9600 baud, 8N1 |
| Control logic level | 3.3 V tolerant |
| Storage | microSD, FAT16/FAT32, up to 32 GB |
| Amplifier | Onboard, ~3 W into 4–8 Ω, differential output |

Hazards to design around:

- **It is a 5 V module behind a 3.3 V MCU.** The DFPlayer runs from the XIAO's
  5 V pin (which is USB bus voltage), while its UART accepts 3.3 V logic
  directly. No level shifter is needed, but the two devices must share a
  ground for the UART to work at all.
- **A series resistor belongs on the DFPlayer's RX line.** The widely
  recommended 1 kΩ in series with DFPlayer RX limits current into the module's
  input and measurably reduces the audible hiss that otherwise couples from
  the UART line into the amplifier. Treat as planned, not proven.
- **The amplifier is the current hog in this project, not the MCU.** 3 W into
  8 Ω at 5 V implies peaks around 600 mA. A 500 mA USB 2.0 port cannot supply
  that alongside the MCU and the backlight. Mitigations, in order of
  preference: keep the configured volume well below maximum, add bulk
  capacitance (100 µF or more) across the DFPlayer's supply to absorb
  transients, and power the robot from a supply with real headroom rather
  than from a laptop port.
- **Audio ground must not share a return path with the display.** The display
  pushes fast SPI edges; running its return current through the same wire as
  the amplifier's is a reliable way to make the SPI clock audible in the
  speaker. The DFPlayer's ground should go to the XIAO's ground directly.
- **The speaker is differential.** It connects across SPK_1 and SPK_2. Neither
  side may be grounded — that would short half the bridged output stage.

---

## 5. Presence detection: HC-SR501 PIR — **planned**

| Property | Value |
|---|---|
| Supply | 4.5–20 V (5 V here) |
| Output | Digital, ~3.3 V when triggered |
| Adjustments | Sensitivity and hold-time trim pots, retrigger jumper |

The 3.3 V output level is directly compatible with an ESP32-C3 input, so the
sensor needs no interface circuitry. The planned pin is D5 (GPIO7), chosen
because it is not a strapping pin and because an unpredictable sensor output
must never be able to influence the boot mode.

The known weakness of this part is that it reports *motion*, not *presence*. A
person sitting still at a desk stops triggering it, which is the exact
situation the robot cares about. This is why mmWave presence sensing is listed
as an alternative in the [roadmap](project-roadmap.md) rather than the PIR
being treated as a finished answer.

---

## 6. Power

Current configuration: **USB-C from a host computer or wall adapter, 5 V bus.**

| Consumer | Rail | Rough draw | Notes |
|---|---|---|---|
| XIAO ESP32C3 (radio idle) | 3V3 | tens of mA | Wi-Fi unused in V1 |
| ST7789 panel + backlight | 3V3 | tens of mA, continuous | Cannot be gated — no BL pin |
| DFPlayer, idle | 5 V | ~20 mA | Not measured |
| DFPlayer + amplifier, peaks | 5 V | up to ~600 mA | Not measured; playback volume during the verification test was not recorded |

The figures above are order-of-magnitude expectations from the parts'
datasheets, not measurements taken on this build. Measuring the assembled
robot's actual draw is a roadmap item, and it is the gate on any serious
battery work.

The 3V3 rail comes from the XIAO's onboard regulator. Seeed does not publish a
headline current rating for it, so this project treats it as adequate for
small logic loads — the display — and deliberately powers the DFPlayer and the
PIR from 5 V instead of leaning on it.

---

## 7. Assembly notes

- All modules must share a common ground. Every signal in this project is
  referenced to it, and a missing ground presents as a display that works and
  a UART that silently does not.
- Wire power before signals and verify 3V3 and 5 V with a meter before
  connecting a module, so a wiring slip burns a jumper instead of the panel.
- The display's SCL/SDA labels are I2C names on an SPI device. SCL is the SPI
  clock, SDA is MOSI. Wiring them to the XIAO's I2C pins (D4/D5) because the
  names match would produce a dead display.

See [wiring.md](wiring.md) for the exact connection tables.
