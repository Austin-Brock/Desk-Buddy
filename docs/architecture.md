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

The firmware is structured in three conceptual layers. They are *conceptual*
at this commit — all of the implemented code currently lives in the single
sketch file `firmware/desk-buddy/desk-buddy.ino`, separated by section rather
than by file. Splitting it into modules is a planned refactor, and the layer
boundaries below are what that split will follow.

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
  setup()
     |-- Serial.begin(115200)            bounded wait for USB CDC, max 1.5 s
     |-- displayBegin()                  tft.init(240,320), rotation 0, clear
     |-- faceDraw()                      full-frame draw of the resting face
     |-- randomSeed(micros())
     |-- faceScheduleNextBlink()         first blink in 2.6-6.4 s
     v
  loop()                                  runs continuously, never blocks
     |
     |-- now = millis()
     |-- faceAnimationUpdate(now)        at most one animation frame
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

| State | Owner | Notes |
|---|---|---|
| `faceState` | Expression layer | Which of the four blink states is active |
| `blinkStep` | Expression layer | 0 = open, `BLINK_STEPS` = shut |
| `lastFrameMs` | Expression layer | Frame pacing |
| `nextBlinkMs` | Behaviour layer | When the next blink is due |
| `tft` | Hardware layer | Driver instance, written only via draw helpers |

No global mutable state is shared between layers, and nothing is modified from
an interrupt context, so no synchronisation primitives are required anywhere
in the firmware.

---

## 6. Extension points

Where the unimplemented subsystems are designed to attach:

| Subsystem | Attaches as | Depends on |
|---|---|---|
| Audio reminders | A behaviour-layer scheduler plus an `audioUpdate(now)` call in `loop()` | DFPlayer wiring, UART driver — see [audio.md](audio.md) |
| Presence detection | A `presenceUpdate(now)` call in `loop()` feeding a presence flag into the behaviour layer | PIR wiring, debounce logic |
| Expressions / moods | A mood enum in the expression layer selecting alternative geometry | Nothing — this one is unblocked today |
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
