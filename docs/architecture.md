# Architecture

System-level view of Desk-Buddy: what the pieces are, which of them exist, and
how control flows between them.

---

## 1. System diagram

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

Expanded, with current status on every edge:

```
                        +---------------------------+
                        |        Desk Buddy         |
                        +---------------------------+
                                     |
                                 USB-C 5 V
                                     |
                        +---------------------------+
                        |      XIAO ESP32C3         |
                        |  RISC-V @ 160 MHz         |
                        |  Arduino / C++ firmware   |
                        +---------------------------+
                           |                     |
            hardware SPI   |                     |  UART 9600 8N1
            4-wire, write  |                     |  (PLANNED)
            only           |                     |
                           v                     v
          +-------------------------+   +-------------------------+
          |   ST7789 240x320 IPS    |   |    DFPlayer Mini        |
          |   IMPLEMENTED           |   |    NOT YET WIRED        |
          +-------------------------+   +-------------------------+
                           |                     |
                           v                     v
          +-------------------------+   +-------------------------+
          |      Animated face      |   |   microSD, FAT32        |
          |  eyes + smile, blink    |   |   /mp3/0001.mp3 ...     |
          |   WORKING               |   |   card prepared         |
          +-------------------------+   +-------------------------+
                                                 |
                                                 v
                                     +-------------------------+
                                     |   3 W 8 ohm speaker     |
                                     |   NOT YET CONNECTED     |
                                     +-------------------------+

   - - - - - - - - - - -  planned, nothing built  - - - - - - - - - - -

          +-------------------------+   +-------------------------+
          |   HC-SR501 PIR          |   |  Wi-Fi / central        |
          |   presence detection    |   |  service (V2)           |
          |   PLANNED               |   |  PLANNED                |
          +-------------------------+   +-------------------------+
```

| Edge | Transport | Status |
|---|---|---|
| MCU → display | 4-wire SPI, hardware peripheral | **Implemented** |
| Display → face | Adafruit GFX primitives | **Implemented** |
| MCU → DFPlayer | UART, 9600 8N1 | **Planned** — not wired, no firmware |
| DFPlayer → microSD | Internal to the module | Card prepared, untested in situ |
| DFPlayer → speaker | Differential analog | **Planned** — not connected |
| PIR → MCU | Digital input | **Planned** |
| MCU → network | Wi-Fi | **Planned** (V2) |

---

## 2. Firmware layers

The firmware is structured in three layers, and each one is a file:

| Layer | Files | Owns |
|---|---|---|
| Behaviour | `desk-buddy.ino` | Boot order, the cooperative run loop |
| Expression | `face.h` / `face.cpp` | Face geometry, drawing, blink state machine |
| Hardware | `display.h` / `display.cpp` | Driver instance, ST7789 bring-up |
| *(shared)* | `config.h` | Pin map, panel geometry, palette, proportions, timing |

`config.h` sits outside the layering on purpose: it is data, not behaviour,
and it has no `#include` of its own so any layer can read it without pulling
in a dependency.

```
   +----------------------------------------------------------+
   |  Behaviour layer                                         |
   |  What the robot should do and when.                       |
   |  Implemented : randomised blink scheduling                |
   |  Planned     : reminder intervals, presence reactions     |
   +----------------------------------------------------------+
                                |
   +----------------------------------------------------------+
   |  Expression layer                                        |
   |  How the robot shows it. Owns face geometry, the          |
   |  animation state machine, and repaint regions.            |
   |  Implemented : resting face, blink animation              |
   |  Planned     : moods, mouth animation, audio cues         |
   +----------------------------------------------------------+
                                |
   +----------------------------------------------------------+
   |  Hardware layer                                          |
   |  Pin map, peripheral bring-up, driver instances.          |
   |  Implemented : SPI + ST7789 init                          |
   |  Planned     : UART + DFPlayer, PIR input                 |
   +----------------------------------------------------------+
```

The layering exists to keep one specific thing true: **the behaviour layer
must never know about pixels, and the hardware layer must never know about
faces.** When the audio subsystem arrives it plugs into the behaviour layer as
a second output alongside the face, rather than being threaded through the
drawing code.

---

## 3. Control flow

```
  power on
     |
     v
  setup()                                 desk-buddy.ino
     |-- Serial.begin(115200)            bounded wait for USB CDC, max 1.5 s
     |-- displayBegin()                  display.cpp -- tft.init(240,320),
     |                                   rotation 0, clear to black
     |-- randomSeed(micros())
     |-- faceBegin(millis())             face.cpp -- full-frame draw of the
     |                                   resting face, then schedules the
     |                                   first blink 2.6-6.4 s out
     v
  loop()                                  runs continuously, never blocks
     |
     |-- now = millis()
     |-- faceAnimationUpdate(now)        face.cpp -- at most one frame
     |
     |   (planned) audioUpdate(now)      poll DFPlayer, dispatch reminders
     |   (planned) presenceUpdate(now)   sample PIR, update presence state
     |
     +-- return, repeat
```

`faceDraw()` is the only routine that touches the whole frame, and it runs
exactly once, at boot. Everything after that repaints only the eye band. See
[display.md](display.md).

---

## 4. Scheduling model

**Cooperative, non-blocking, single-threaded.** Every subsystem gets called
once per `loop()` iteration with the current `millis()` value, does at most one
small unit of work, and returns.

There is no RTOS task, no timer interrupt, and no `delay()` in the run loop.
This is a deliberate constraint rather than an accident of simplicity:

- **A blink spans 214 ms.** Sleeping through it in `delay()` would make the
  robot deaf and blind for a fifth of a second at a time, several times a
  minute. Once audio reminders exist, a blink would be able to delay a
  reminder, and a reminder would be able to freeze a blink mid-close.
- **The ESP32-C3 is single-core.** There is no second core to hide blocking
  work on, as there would be on an ESP32-S3 or classic ESP32.
- **Cooperative scheduling keeps all state ownership explicit.** Each
  subsystem owns its own timers and state variables; nothing is shared, so
  there is nothing to race.

The cost of this choice is that every subsystem must be written as a state
machine, which is more code than a linear sequence with sleeps. The blink is
the worked example of the pattern that audio and presence detection will
follow.

### Timing budget per `loop()` iteration

| Work | Cost | Frequency |
|---|---|---|
| State check, no frame due | microseconds | Every iteration |
| Eye band repaint, 22.5 KB over SPI | ~7.5 ms (calculated) | 8 frames per blink |
| Full face draw, 150 KB over SPI | ~50 ms (calculated) | Once, at boot |

The SPI costs are calculated from the transfer size at a nominal 24 MHz clock,
not measured. Nothing in the firmware currently competes for that time, so the
budget has slack; the figures are recorded now because audio integration is
what will start consuming it.

---

## 5. State ownership

| State | Owner | Linkage | Notes |
|---|---|---|---|
| `faceState` | `face.cpp` | `static` | Which of the four blink states is active |
| `blinkStep` | `face.cpp` | `static` | 0 = open, `BLINK_STEPS` = shut |
| `lastFrameMs` | `face.cpp` | `static` | Frame pacing |
| `nextBlinkMs` | `face.cpp` | `static` | When the next blink is due |
| `tft` | `display.cpp` | `extern` via `display.h` | Driver instance |

Every piece of animation state is file-local to the module that owns it, so no
other translation unit can reach it. `tft` is the one deliberate exception:
`face.cpp` issues GFX primitives against it directly, because wrapping every
primitive would add indirection without adding anything.

Nothing is modified from an interrupt context, so no synchronisation
primitives are required anywhere in the firmware.

---

## 6. Extension points

Where the unimplemented subsystems are designed to attach:

| Subsystem | Attaches as | Depends on |
|---|---|---|
| Audio reminders | `audio.h` / `audio.cpp` beside the existing modules, plus an `audioUpdate(now)` call in `loop()` | DFPlayer wiring, UART driver — see [audio.md](audio.md) |
| Presence detection | `presence.h` / `presence.cpp`, plus a `presenceUpdate(now)` call in `loop()` feeding a flag to the behaviour layer | PIR wiring, debounce logic |
| Expressions / moods | A mood enum in `face.cpp` selecting alternative geometry from `config.h` | Nothing — this one is unblocked today |
| Multi-robot coordination | A network module, polled from `loop()` like everything else | Wi-Fi, a central service, a protocol — all V2 |

The ordering in the [roadmap](project-roadmap.md) follows the dependency
column: audio is next because it is the only item whose hardware is already on
hand.

---

## 7. What this architecture does not yet do

- **No persistent configuration.** Reminder intervals, volume, and blink
  timing are compile-time constants. Nothing is stored in NVS or on the SD
  card, so every setting requires a reflash to change.
- **No error recovery.** There is no read-back path on the display and no
  health check on any peripheral. A failure presents as a frozen face.
- **No power management.** The MCU never sleeps, and the display backlight
  cannot be gated in firmware because the module exposes no enable pin.
- **No network layer.** The radio is unused in V1.
