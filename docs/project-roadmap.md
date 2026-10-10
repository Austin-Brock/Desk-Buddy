# Project roadmap

Where Desk-Buddy is, what is next, and why in that order.

**No dates.** This is a personal engineering project built in evenings, and
inventing delivery dates for it would be fiction. Items are ordered by
dependency instead: each phase unblocks the next.

---

## Current state

| | |
|---|---|
| **Working** | ST7789 display bring-up, minimalist face, non-blocking randomised blink animation |
| **Working, separately** | DFPlayer audio: three MP3 files played sequentially from microSD through the speaker — verified on a standalone bench sketch, **not** in the project firmware |
| **In progress** | Running display and audio together, and porting the audio code into the firmware with non-blocking scheduling |
| **Planned next** | Reminder scheduling, then presence detection |
| **Power** | USB-C only |
| **Enclosure** | None — bare modules on jumper wires |

---

## V1 — Expressive desk robot with audio reminders

The goal of V1: a robot that sits on a desk, looks alive, and tells you
something useful at intervals you chose.

### Display and animation — **complete**

- [x] ST7789 240×320 SPI panel initialised on the XIAO's hardware SPI pins
- [x] Minimalist face: two rounded-rectangle eyes, a smile, white on black
- [x] Non-blocking blink animation using partial eye-band redraw (no flicker)
- [x] Randomised blink interval so the robot does not read as a metronome
- [x] Cooperative scheduling model established — no `delay()` in the run loop

### Hardware consolidation — **next**

- [ ] Move from Dupont-jumper-to-module wiring onto the breadboard
- [ ] Shared 5 V rail and a single common ground point
- [ ] Verify the display still runs unchanged after the move

This is first because the audio module needs a 5 V rail and a clean ground
that module-to-module jumpers cannot provide. See
[hardware.md](hardware.md).

### Audio — **standalone playback verified, integration in progress**

Full plan with per-step acceptance tests in [audio.md](audio.md).

Verified on hardware:

- [x] Connect the DFPlayer module (HW-247A) to the XIAO ESP32C3
- [x] Connect the 3 W 8 Ω speaker across SPK_1 and SPK_2
- [x] Prepare the microSD card
- [x] Verify MP3 playback
- [x] Verify sequential playback of three audio files

Remaining:

- [ ] Integrate audio into the main firmware
- [ ] Replace blocking delays with `millis()`-based scheduling
- [ ] Verify display animations during audio playback
- [ ] Integrate the motion sensor
- [ ] Record the as-built DFPlayer pin assignment in [wiring.md](wiring.md)
- [ ] Measure clip durations, so a reminder interval can be chosen sensibly
- [ ] Assign each track number to a robot behaviour
- [ ] Measure idle and peak current draw; replace the datasheet estimates in
      [hardware.md](hardware.md) with real figures

The ordering caveat: the module was proven on a standalone bench sketch before
the build moved to the breadboard, so breadboard consolidation above is still
open and is the immediate next task — the display and the DFPlayer need a
shared 5 V rail and one common ground point to run together at all.

### Reminder behaviour — **planned**

- [ ] Interval-driven reminder scheduler in the behaviour layer
- [ ] Configurable intervals (compile-time constants to begin with)
- [ ] Clip selection per reminder type
- [ ] A face reaction while a clip plays, gated on the BUSY signal

### V1 exit criteria

V1 is done when all of the following are true at once, on hardware:

1. The robot boots from USB with no computer attached and shows an animated
   face.
2. It plays a reminder clip on a configured interval.
3. The face keeps animating smoothly during playback.
4. It runs for 24 hours without a reset, a hang, or a visible animation
   artefact.

Criterion 4 is included because nothing in this project has yet been left
running long enough to find out.

---

## V1.1 — Presence detection

Depends on: V1 complete.

- [ ] Wire the HC-SR501 PIR on D5 (GPIO7) — see [wiring.md](wiring.md)
- [ ] Debounced presence state in the behaviour layer
- [ ] Suppress reminders when nobody is at the desk
- [ ] An "awake vs idle" face distinction driven by presence

The known limitation, recorded now so it is not a surprise later: the
HC-SR501 detects *motion*, not *presence*. Somebody sitting still and working
— precisely the person this robot exists for — stops triggering it. If
debouncing and hold-time tuning cannot paper over that, the answer is a
different sensor, not more firmware.

- [ ] Evaluate an mmWave presence sensor (e.g. LD2410) as a replacement

---

## V2 — Standalone product-shaped robot

Depends on: V1.1, and on several unresolved hardware questions.

### Expressions

- [ ] A mood enum in the expression layer selecting alternative face geometry
- [ ] Happy, sleepy, focused, and alert faces
- [ ] Transition animations between moods rather than hard cuts
- [ ] Mouth animation synchronised to playback

This is the only V2 item that is **unblocked today** — it needs no new
hardware. It is scheduled after audio anyway, because a robot that speaks and
has one expression is more useful than a robot that is silent and has five.

### Battery power

- [ ] Measure the assembled robot's actual current draw
- [ ] Select a LiPo cell and connector for the XIAO's onboard charger
- [ ] Solve the backlight problem

The backlight is the blocker, not the battery. This display module exposes no
backlight enable pin, so the panel cannot be dimmed or switched off in
firmware and draws its backlight current for as long as the robot has power.
No amount of MCU sleep logic changes that. The realistic options are a panel
with a BL pin or a MOSFET cut into the backlight supply — a hardware change
either way. See [hardware.md](hardware.md).

- [ ] Battery-level reporting on the face

### Enclosure

- [ ] 3D printed shell sized around the 2.0" panel
- [ ] Internal mounts for the XIAO, DFPlayer, and speaker
- [ ] Speaker grille and a sealed chamber — a bare 8 Ω driver on a desk sounds
      like a bare 8 Ω driver on a desk
- [ ] Routed USB-C access
- [ ] Publish the STLs alongside the firmware

### Multi-robot coordination

- [ ] Wi-Fi provisioning without reflashing (captive portal or BLE)
- [ ] A central service the robots check in with
- [ ] Shared reminder schedules across several robots
- [ ] Over-the-air firmware updates

This is the reason the project started on a Wi-Fi/BLE-capable board even
though V1 does not use the radio: it costs nothing now and avoids a board
swap later.

---

## Backlog — ideas not yet committed to a version

Captured so they are not lost, explicitly not scheduled:

- Persistent configuration in NVS so settings survive a reflash
- A physical input — button or rotary encoder — so settings can be changed
  without a computer, which is what makes persistent config worth having
- Pomodoro mode with the session state shown on the face
- Calendar integration: a reminder before the next meeting
- A reactive microphone so the robot responds to being spoken to
- Ambient light sensing to adapt the face's brightness
- Capacitive touch so the robot reacts to being petted
- Eye tracking of a detected person via mmWave angle data
- A watchdog and a health check, since right now a peripheral failure
  presents as a frozen face

---

## Explicit non-goals

Worth stating, because "could this do X" has a cost:

- **No voice assistant.** Speech recognition and generated speech are a
  different project with a different architecture and a different budget. The
  DFPlayer path plays prerecorded clips, by design.
- **No cloud account or app.** The V2 central service is for coordinating
  robots on one network, not a hosted product.
- **No screen-based UI.** The display shows a face, not menus. Configuration
  belongs on a button, a web page, or a recompile.
- **No motion or actuators.** It is an expressive desk object, not an
  animatronic.

---

## Dependency graph

```
  display + blink  (DONE)          audio standalone  (DONE)
         |                                  |
         +----------------+-----------------+
                          v
              breadboard consolidation
                          |
                          v
          display + audio together  <-- YOU ARE HERE
                          |
                          v
     non-blocking audio in firmware ----> reminder scheduling ----> V1 COMPLETE
         |                                                  |
         |                                                  v
         |                                        presence detection (PIR)
         |                                                  |
         |                                                  v
         |                                          mmWave evaluation
         v                                                  |
   mouth animation <---------------------------------------- +
         |                                                  |
         v                                                  v
    mood system                                    battery + enclosure
         |                                                  |
         +------------------> multi-robot <-----------------+
```
