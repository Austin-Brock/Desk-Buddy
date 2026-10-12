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
                ST7789 Display    DFPlayer HW-247A
                      |                 |
                Animated Face        microSD
                   (working)             |
                                      Speaker
                                   (plays, but only
                                    from a standalone
                                    bench sketch)
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
            4-wire, write  |                     |  (verified on a
            only           |                     |   bench sketch, not
                           |                     |   in this firmware)
                           v                     v
          +-------------------------+   +-------------------------+
          |   ST7789 240x320 IPS    |   |   DFPlayer HW-247A      |
          |   IMPLEMENTED           |   |   WIRED, PLAYS          |
          +-------------------------+   +-------------------------+
                           |                     |
                           v                     v
          +-------------------------+   +-------------------------+
          |      Animated face      |   |   microSD, FAT          |
          |  eyes + smile, blink    |   |   /mp3/0001-0003.mp3    |
          |   WORKING               |   |   READ, 3 TRACKS PLAY   |
          +-------------------------+   +-------------------------+
                                                 |
                                                 v
                                     +-------------------------+
                                     |   3 W 8 ohm speaker     |
                                     |   AUDIO VERIFIED        |
                                     +-------------------------+

   - - - - -  not in the firmware; bench-verified or unbuilt  - - - - -

          +-------------------------+   +-------------------------+
          |   HC-SR501 PIR          |   |  Wi-Fi / central        |
          |   motion detection      |   |  service (V2)           |
          |   BENCH-VERIFIED        |   |  PLANNED                |
          |   on D1, not D5         |   |                         |
          +-------------------------+   +-------------------------+
```

| Edge | Transport | Status |
|---|---|---|
| MCU → display | 4-wire SPI, hardware peripheral | **Implemented** |
| Display → face | Adafruit GFX primitives | **Implemented** |
| MCU → DFPlayer | UART, 9600 8N1 | **Verified** (MCU transmits on D6); the modular `audio.cpp` is **written but never run** |
| DFPlayer → microSD | Internal to the module | **Verified** — card read, three tracks play |
| DFPlayer → speaker | Differential analog | **Verified** — audio audible |
| All three together | — | ✅ **Verified** — `firmware/combined-reference` runs display, motion, and audio simultaneously |
| PIR → MCU | Digital input | **Verified** (D1); the modular `motion.cpp` is **written but never run** |
| MCU → network | Wi-Fi | **Planned** (V2) |

---

## 2. Firmware layers

The firmware is structured in three layers, and each one is a file:

| Layer | Files | Owns |
|---|---|---|
| Behaviour | `desk-buddy.ino` | Boot order, the cooperative run loop, the motion-triggered audio policy |
| Expression | `face.h` / `face.cpp` | Face geometry, drawing, blink state machine |
| Hardware | `display.h` / `display.cpp` | Driver instance, ST7789 bring-up |
| Hardware | `motion.h` / `motion.cpp` | PIR pin, warm-up window, edge detection |
| Hardware | `audio.h` / `audio.cpp` | DFPlayer UART, command frames, readiness timing |
| *(shared)* | `config.h` | Pin map, panel geometry, palette, proportions, timing, volume |

The hardware modules report facts and perform actions; they do not decide
anything. `motion.cpp` reports that movement started, `audio.cpp` plays a
numbered clip, and the decision connecting the two lives in the behaviour
layer. That boundary is why the cooldown policy is in the sketch rather than
in either module.

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
     |-- faceBegin(now)                  face.cpp -- full-frame draw of the
     |                                   resting face, then schedules the
     |                                   first blink 2.6-6.4 s out
     |-- motionBegin(now)                motion.cpp -- pin as INPUT, starts
     |                                   the 60 s sensor warm-up window
     |-- audioBegin(now)                 audio.cpp -- opens UART1 on D7/D6,
     |                                   starts the module's 2 s readiness
     |                                   timer. Sends nothing yet
     |-- lastAudioMs backdated           so the first motion event is not
     |                                   swallowed by an unexpired cooldown
     v
  loop()                                  runs continuously, never blocks
     |
     |-- now = millis()
     |-- faceAnimationUpdate(now)        face.cpp -- at most one frame
     |-- motionUpdate(now)               motion.cpp -- sample the PIR every
     |                                   20 ms, detect a rising edge
     |-- audioUpdate(now)                audio.cpp -- deferred setup only
     |-- behaviourUpdate(now)            this file -- consume a motion event,
     |                                   apply the cooldown, play the next
     |                                   clip in rotation
     |
     +-- return, repeat

The animation is serviced first because it is the subsystem a human would
notice stuttering. Nothing else in the loop can starve it: each call does at
most one small unit of work, and the largest single action in the whole loop
is one eye-band repaint.
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
| `motionLastLevel`, `motionEventPending`, `motionWarmUpUntilMs`, `motionLastSampleMs`, `motionLastEdgeMs` | `motion.cpp` | `static` | Sensor level, latched event, timing |
| `audioReadyAtMs`, `audioLastCommandMs`, `audioVolumeSent` | `audio.cpp` | `static` | Module readiness and command pacing |
| `lastAudioMs`, `nextTrack` | `desk-buddy.ino` | `static` | Cooldown and clip rotation — behaviour policy |

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
| Interval reminders | A scheduler in `behaviourUpdate()` alongside the motion policy | Nothing structural — the audio module already plays on request |
| Playback completion polling | `audioIsBusy()` in `audio.cpp`, reading the BUSY pin or a status frame | Wiring the BUSY pin, or confirming the module's TX line is connected to D6 |
| Mouth animation during playback | A call from `behaviourUpdate()` into the expression layer | Completion polling, so the mouth knows when to stop |
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
