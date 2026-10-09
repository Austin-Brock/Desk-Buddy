/* ===========================================================================
 * config.h -- Desk-Buddy hardware configuration and tunable constants
 *
 * Every value a person might reasonably want to change lives here: the pin
 * map, the panel geometry, the proportions of the face, and the animation
 * timing. Implementation details that are derived from these values stay in
 * the module that owns them.
 *
 * This header deliberately has no #include of its own. It defines colours as
 * raw RGB565 literals rather than using the driver library's ST77XX_*
 * constants, so configuration does not depend on the display library and can
 * be included from anywhere.
 * ===========================================================================
 */

#ifndef DESK_BUDDY_CONFIG_H
#define DESK_BUDDY_CONFIG_H

/* ---------------------------------------------------------------------------
 * Pin map -- ST7789 display
 *
 * The XIAO silkscreen labels (D0, D2, D8, D10) are provided as macros by the
 * XIAO_ESP32C3 variant in the esp32 board package. They are used here instead
 * of raw GPIO numbers so the code reads the same way the board is labelled.
 *
 *   Silkscreen   GPIO   Display pin   Role
 *   ----------   ----   -----------   ---------------------------------------
 *   D0           2      CS            SPI chip select
 *   D2           4      DC            data / command select
 *   D8           8      SCL           SPI clock  (SCK)  -- hardware SPI pin
 *   D10          10     SDA           SPI data   (MOSI) -- hardware SPI pin
 *                       V             3V3
 *                       G             GND
 *
 * Hardware decision: D8 and D10 are the XIAO's native hardware SPI SCK and
 * MOSI pins. Wiring the display clock and data to these specific pins is what
 * allows the display to be driven by the ESP32-C3 SPI peripheral rather than
 * by bit-banged software SPI, which is roughly an order of magnitude slower
 * and would make the eye repaints visibly sluggish.
 *
 * Note on the display's pin labels: this ST7789 module labels its SPI pins
 * SCL and SDA, which are I2C names. The module is SPI, not I2C -- SCL is the
 * SPI clock and SDA is SPI MOSI. The panel is write-only in this design, so
 * there is no MISO connection and the firmware cannot read anything back.
 *
 * Hardware caution: GPIO2, GPIO8 and GPIO9 are ESP32-C3 boot strapping pins.
 * GPIO2 (D0) is used here for chip select, which is safe because the display
 * only presents a high-impedance input on that line and does not drive it at
 * boot. No pin may be wired to something that pulls it low while the board is
 * resetting. See docs/hardware.md.
 * -------------------------------------------------------------------------*/
#define TFT_CS    D0   /* GPIO2  */
#define TFT_DC    D2   /* GPIO4  */
#define TFT_SCLK  D8   /* GPIO8  -- hardware SPI SCK  */
#define TFT_MOSI  D10  /* GPIO10 -- hardware SPI MOSI */

/* The 2.0" ST7789 module used here exposes no RST pin, so the reset line is
 * declared as absent. Adafruit_ST7789 accepts -1 for the reset argument and
 * then resets the panel over SPI with its software reset command (SWRESET)
 * during init() instead of toggling a hardware line. This works reliably on
 * this module; the trade-off is that the panel cannot be recovered from a
 * wedged state without a full power cycle. See docs/display.md. */
#define TFT_RST   -1

/* ---------------------------------------------------------------------------
 * Panel geometry
 *
 * Resolution in the display's native (unrotated) orientation. This is a 2.0"
 * 240x320 IPS panel, so the driver is initialised as 240 wide by 320 tall and
 * rotation 0 keeps it portrait, which is the orientation the robot's face is
 * designed for: eyes above a mouth.
 * -------------------------------------------------------------------------*/
#define TFT_WIDTH     240
#define TFT_HEIGHT    320
#define TFT_ROTATION  0

/* ---------------------------------------------------------------------------
 * Face palette
 *
 * RGB565. White on black is not only the right look for a small robot face,
 * it also means the erase colour is the background colour: clearing a region
 * is a single fillRect with nothing underneath to restore. A patterned
 * background would require either a framebuffer -- 150 KB for this panel,
 * against 400 KB of SRAM -- or re-deriving the background every frame.
 * -------------------------------------------------------------------------*/
#define FACE_FG  0xFFFF  /* white */
#define FACE_BG  0x0000  /* black; doubles as the erase colour */

/* Colour the panel is cleared to at bring-up, before anything is drawn. Kept
 * separate from the face palette so the hardware layer does not depend on a
 * face constant -- they happen to be the same value today. */
#define DISPLAY_CLEAR_COLOR  0x0000

/* ---------------------------------------------------------------------------
 * Face geometry
 *
 * The face is intentionally minimalist: two solid rounded-rectangle eyes and
 * a simple arc smile. Solid eyes with no pupils read as friendly and alert at
 * a glance from across a desk, and they are cheap to redraw, which matters
 * because the blink animation repaints them several times per blink.
 *
 * Layout in the 240x320 portrait frame:
 *
 *        x=40        x=144
 *          +----+      +----+          y=96   eye top
 *          |    |      |    |
 *          +----+      +----+          y=168  eye bottom
 *               \____/                 y=242  mouth bottom
 *
 * EYE_LEFT_X is derived from the panel width so the pair stays centred if the
 * eye size or the gap between them is changed.
 * -------------------------------------------------------------------------*/
#define EYE_W        56
#define EYE_H        72
#define EYE_RADIUS   18
#define EYE_GAP      48
#define EYE_TOP_Y    96
#define EYE_LEFT_X   ((TFT_WIDTH - (2 * EYE_W + EYE_GAP)) / 2)
#define EYE_RIGHT_X  (EYE_LEFT_X + EYE_W + EYE_GAP)

/* Mouth. Drawn as the bottom half of a circle, which gives a cleaner curve
 * than stitching line segments together. Thickness is produced by drawing
 * several concentric arcs, because Adafruit_GFX has no stroke width. */
#define MOUTH_CENTER_X   (TFT_WIDTH / 2)
#define MOUTH_CENTER_Y   196
#define MOUTH_RADIUS     46
#define MOUTH_THICKNESS  3

/* ---------------------------------------------------------------------------
 * Blink animation timing
 *
 * See docs/display.md for the full rationale. In short: four steps at an 18 ms
 * frame interval closes the eye in about 70 ms, which matches the feel of a
 * natural blink -- fewer steps looks like a glitch, more looks sleepy -- and
 * the interval between blinks is randomised because a fixed interval reads as
 * a metronome rather than as a creature.
 * -------------------------------------------------------------------------*/

/* Height of the eye at full closure, in pixels. Deliberately not zero: a
 * couple of remaining rows leave a thin closed-lid line, which looks more
 * like a closed eye than the eye vanishing entirely. */
#define EYE_CLOSED_H  4

/* Number of intermediate frames in each direction. */
#define BLINK_STEPS  4

/* Minimum time between animation frames. The SPI transfer for one eye-band
 * repaint costs a few milliseconds on its own, so this is a floor, not a
 * guarantee. */
#define BLINK_FRAME_MS  18UL

/* How long the eyes stay shut at full closure. */
#define BLINK_CLOSED_HOLD_MS  70UL

/* Bounds of the randomised gap between blinks. */
#define BLINK_INTERVAL_MIN_MS  2600UL
#define BLINK_INTERVAL_MAX_MS  6400UL

/* ---------------------------------------------------------------------------
 * Serial console
 *
 * The XIAO ESP32C3 presents a native USB CDC port, so this value is nominal
 * on the USB link, but it is also what the Arduino IDE Serial Monitor must be
 * set to.
 * -------------------------------------------------------------------------*/
#define SERIAL_BAUD  115200

/* Bounded wait for the USB CDC host to attach at boot. Bounded, because the
 * robot must boot and run with no computer attached. */
#define SERIAL_ATTACH_TIMEOUT_MS  1500UL

#endif /* DESK_BUDDY_CONFIG_H */
