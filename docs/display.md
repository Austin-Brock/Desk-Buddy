# Display and animation architecture

How the ST7789 panel is driven and how the face is animated.
Everything in this document is **implemented and verified on hardware**
unless explicitly marked otherwise.

| Where the code lives | |
|---|---|
| `firmware/desk-buddy/config.h` | Pin map, panel geometry, palette, face proportions, animation timing — every constant quoted in this document |
| `firmware/desk-buddy/display.cpp` | Driver instance and `displayBegin()` |
| `firmware/desk-buddy/face.cpp` | Drawing routines, derived repaint regions, blink state machine |

---

## 1. Panel configuration

```c
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

tft.init(240, 320);     /* native portrait resolution */
tft.setRotation(0);     /* keep portrait */
tft.fillScreen(ST77XX_BLACK);
```

| Setting | Value | Reason |
|---|---|---|
| Resolution | 240 × 320 | The panel's native geometry |
| Rotation | 0 | Portrait suits a tall face: eyes above a mouth |
| Colour | RGB565, 16 bpp | The controller's native format |
| Reset pin | `-1` | This module exposes no RST pin |
| Palette in use | White on black | See below |

### Why the reset pin is `-1`

The module does not break the ST7789's reset line out to its header. Passing
`-1` tells the Adafruit driver that there is no hardware reset line, and it
resets the controller with the SPI software reset command (`SWRESET`) during
`init()` instead.

This works reliably on this module. The trade-off is that there is no way to
recover a panel that has been wedged by a malformed SPI transaction without
cycling power to the whole robot — a firmware-initiated recovery is not
possible.

### Why the pins are passed explicitly

The constructor is given all four pins rather than relying on the board's
default `SPI` object. Because `TFT_SCLK` (D8/GPIO8) and `TFT_MOSI`
(D10/GPIO10) are the ESP32-C3's hardware SPI pins, the transfers still go
through the SPI peripheral — the explicit form costs nothing and keeps the
wiring documented at the point where it matters.

Had the clock and data been wired to any other pins, the driver would have
fallen back to bit-banged software SPI, roughly an order of magnitude slower.
The eye-band repaint described below would then take tens of milliseconds and
the blink would visibly tear.

### Why white on black

Beyond looking right for a small robot face, the choice has a concrete
engineering payoff: **the erase colour is the background colour.** Clearing a
region is one `fillRect` in black with nothing underneath to restore. A face
on a patterned or gradient background would require either keeping a copy of
the background in RAM — the ESP32-C3 has 400 KB of SRAM and a full 240 × 320
RGB565 framebuffer alone is 150 KB — or re-deriving it procedurally on every
frame.

### No read-back path

Only MOSI is wired; the panel's MISO line is not connected. The firmware
therefore cannot read controller registers, cannot confirm that
initialisation succeeded, and cannot detect a wiring fault. All display
diagnosis on this build is visual. The serial boot banner reports what the
firmware *asked* for, not what the panel acknowledged.

---

## 2. Face geometry

All coordinates are named constants in `config.h`, derived from the panel
width where possible, so the face can be re-proportioned in one place and the
animation code can compute its repaint region from the same source of truth.

```
         x=40         x=144
           +----+       +----+        y=96    eye top
           |    |       |    |
           |    |       |    |
           +----+       +----+        y=168   eye bottom

                \______/              y=242   mouth bottom
            240 x 320 portrait frame
```

| Constant | Value | Meaning |
|---|---|---|
| `EYE_W` | 56 | Eye width |
| `EYE_H` | 72 | Eye height, fully open |
| `EYE_RADIUS` | 18 | Corner radius |
| `EYE_GAP` | 48 | Gap between the eyes |
| `EYE_TOP_Y` | 96 | Top edge of both eyes |
| `EYE_LEFT_X` | 40 | Derived: `(TFT_WIDTH - (2*EYE_W + EYE_GAP)) / 2` |
| `EYE_RIGHT_X` | 144 | Derived: `EYE_LEFT_X + EYE_W + EYE_GAP` |
| `MOUTH_CENTER_X` | 120 | Derived: `TFT_WIDTH / 2` |
| `MOUTH_CENTER_Y` | 196 | Arc centre |
| `MOUTH_RADIUS` | 46 | Arc radius |
| `MOUTH_THICKNESS` | 3 | Concentric arcs drawn |

The face occupies y = 96 to 242, centred at y = 169 against a panel centre of
160 — biased very slightly low, which keeps the eyes nearer the optical centre
of the face than a strict geometric centring would.

### Eyes: `fillRoundRect`, no pupils

Solid rounded rectangles with no pupils read as friendly and alert from across
a desk, and they are cheap to repaint — which matters because the blink
repaints them several times per blink. Pupils would also raise the question of
where the robot is looking, which implies gaze tracking the hardware cannot
support.

### Mouth: arc rather than line segments

The smile is the bottom half of a circle, drawn with
`drawCircleHelper(x, y, r, cornerMask, colour)`. The corner mask is a bitfield:
`1` = top-right, `2` = top-left, `4` = bottom-right, `8` = bottom-left, so the
smile is `4 | 8`.

Adafruit_GFX has no stroke width, so thickness comes from drawing
`MOUTH_THICKNESS` concentric arcs at decreasing radii. A curve stitched from
straight segments would need either visible faceting or many more draw calls
for the same result.

---

## 3. Animation architecture

Two rules govern all animation in this firmware:

1. **Never repaint the whole frame.** Repaint only the region that changed.
2. **Never block.** No `delay()` anywhere in the animation path.

### Rule 1: partial redraw

| Approach | Pixels | Bytes at 16 bpp | Result |
|---|---|---|---|
| `fillScreen` + redraw full face | 76 800 | 150 KB | Visible black flash per frame; the mouth is needlessly redrawn |
| Repaint the eye band only | 11 520 | 22.5 KB | No visible flicker |

The eye band is the bounding box of both eyes together, derived in `face.cpp`
from the eye geometry in `config.h` rather than hard-coded. It is an
implementation detail of the animation, not a tunable:

```c
#define EYE_BAND_X  EYE_LEFT_X                          /*  40      */
#define EYE_BAND_Y  EYE_TOP_Y                           /*  96      */
#define EYE_BAND_W  ((EYE_RIGHT_X + EYE_W) - EYE_LEFT_X) /* 160      */
#define EYE_BAND_H  EYE_H                               /*  72      */
```

That is 15% of a full-frame repaint. Each animation frame clears the band to
black and redraws both eyes at their current height.

The band is cleared and redrawn in one pass rather than erasing only the
difference between the previous and current eye shapes. The difference region
of a rounded rectangle is fiddly to compute and the saving would be a few
hundred microseconds, so the simpler version wins. If the face later gains a
background that is not flat black, this is the decision that would have to be
revisited first.

**Transfer cost.** 22.5 KB per frame works out to roughly 7.5 ms at a 24 MHz
SPI clock. The firmware does not override the Adafruit library's default SPI
frequency, and this cost has been calculated rather than measured — the figure
is an expectation, not a benchmark.

### Rule 2: non-blocking state machine

A blink spans about 214 ms. Spending that in `delay()` would stall everything
else the firmware does — and will have to do once audio and presence detection
are added. So the blink is a state machine advanced by
`faceAnimationUpdate(now)`, which performs at most one frame of work per call
and returns immediately.

```
                    now >= nextBlinkMs
     +------------+ ----------------->  +--------------------+
     |  RESTING   |                     |  BLINK_CLOSING     |
     |  eyes open |                     |  step 0 -> 4       |
     +------------+                     +--------------------+
            ^                                     |
            |                                     | step == BLINK_STEPS
            | step == 0                            v
     +--------------------+              +--------------------+
     |  BLINK_OPENING     | <----------- |  BLINK_CLOSED      |
     |  step 4 -> 0       |   hold 70 ms |  eyes shut         |
     +--------------------+              +--------------------+
```

| State | Exit condition | Action on exit |
|---|---|---|
| `FACE_RESTING` | `now >= nextBlinkMs` | Begin closing at step 0 |
| `FACE_BLINK_CLOSING` | step reaches `BLINK_STEPS` | Hold shut |
| `FACE_BLINK_CLOSED` | `BLINK_CLOSED_HOLD_MS` elapsed | Begin opening |
| `FACE_BLINK_OPENING` | step returns to 0 | Schedule the next blink |

### How the eye closes

The eye is redrawn at a reduced height that keeps its vertical centre. A
rounded rectangle losing height already reads as a closing lid, so no second
shape is needed, and nothing has to be known about what is behind the lid.

`faceEyeHeightForStep()` interpolates linearly from `EYE_H` to `EYE_CLOSED_H`:

| Step | Eye height | Drawn at y | Corner radius |
|---|---|---|---|
| 0 | 72 | 96 | 18 |
| 1 | 55 | 104 | 18 |
| 2 | 38 | 113 | 18 |
| 3 | 21 | 121 | 10 |
| 4 | 4 | 130 | 2 |

Two details that are easy to get wrong:

- **The corner radius must be clamped to half the current height.** Otherwise
  `fillRoundRect`'s corner arcs overlap and the shape degenerates at closure.
  This is why the radius column falls off at steps 3 and 4.
- **`EYE_CLOSED_H` is 4, not 0.** A few remaining rows leave a thin
  closed-lid line, which reads as a closed eye. At zero the eye simply
  vanishes, which reads as the display failing.

Interpolation is integer-only. The ESP32-C3 has no floating-point unit, and
single-pixel precision is all this needs.

### Timing

| Constant | Value | Meaning |
|---|---|---|
| `BLINK_STEPS` | 4 | Frames in each direction |
| `BLINK_FRAME_MS` | 18 ms | Minimum interval between frames |
| `BLINK_CLOSED_HOLD_MS` | 70 ms | Time held shut |
| `BLINK_INTERVAL_MIN_MS` | 2600 ms | Shortest gap between blinks |
| `BLINK_INTERVAL_MAX_MS` | 6400 ms | Longest gap between blinks |

A blink therefore takes `4 × 18 + 70 + 4 × 18 = 214 ms`, and blinks occur
every 2.6–6.4 s, averaging about 4.8 s including the blink itself.

`BLINK_FRAME_MS` is a floor, not a guarantee: the SPI transfer for one band
repaint costs several milliseconds by itself, so the real frame interval is
whatever the transfer plus the scheduler produces.

**Why the interval is randomised.** A fixed interval reads as mechanical — a
metronome, not a creature. Randomising it is the single cheapest thing in this
firmware that makes the robot feel alive. Four steps was chosen the same way:
fewer looks like a glitch, more looks sleepy.

**Rollover safety.** Elapsed-time comparisons use unsigned subtraction
(`now - lastFrameMs >= interval`) rather than comparing absolute timestamps, so
they remain correct across the `millis()` rollover at approximately 49 days of
uptime.

---

## 4. Validation

The animation has no automated test — it targets hardware that cannot be
exercised in CI. Two things were done instead:

1. **The state machine was ported to a standalone simulation** and run over
   400 s of virtual time: 83 blinks, no step underflow or overflow, every
   drawn eye rectangle inside the declared repaint band, and no degenerate
   corner radii.
2. **Visual confirmation on hardware**: the face renders, the eyes blink, and
   no flicker is visible.

---

## 5. Not implemented

Deliberately listed so this document is not mistaken for a description of a
finished animation system:

- **Expressions other than the resting face.** No happy, sleepy, surprised, or
  alert states exist. The state machine has room for them — a mood enum
  selecting different eye and mouth geometry — but none are written.
- **Gaze or eye movement.** The eyes do not move, and there are no pupils to
  move.
- **Mouth animation.** The smile is drawn once at boot and never redrawn.
- **Animation driven by events.** Nothing external influences the face; the
  blink timer is the only input. Audio-reactive and presence-reactive
  behaviour both depend on subsystems that are not built yet.
- **Double buffering or dirty-rectangle tracking.** The repaint region is
  hard-reasoned rather than computed at runtime, which is adequate for one
  animated element and would not be for several.
