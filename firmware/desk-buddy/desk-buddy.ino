/* ===========================================================================
 * Desk-Buddy -- firmware entry point
 *
 * A small expressive desk productivity robot.
 * Target board: Seeed Studio XIAO ESP32C3 (RISC-V, 4 MB flash, 400 KB SRAM)
 *
 * This file is the Arduino sketch entry point. The sketch folder name must
 * match the .ino file name, which is why the firmware lives at
 * firmware/desk-buddy/desk-buddy.ino rather than directly under firmware/.
 *
 * Build notes (see README.md for the full procedure):
 *   Board     : "XIAO_ESP32C3" from the esp32 board package by Espressif
 *   Libraries : Adafruit GFX Library, Adafruit ST7735 and ST7789 Library
 *   Serial    : 115200 baud
 *
 * Status of this file at this commit: the display is initialised and the
 * robot's resting face is drawn. The face is static -- it does not animate
 * yet.
 * ===========================================================================
 */

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

/* ---------------------------------------------------------------------------
 * Pin map
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
 * and would make full-screen redraws visibly sluggish.
 *
 * Note on the display's pin labels: this ST7789 module labels its SPI pins
 * SCL and SDA, which are I2C names. The module is SPI, not I2C -- SCL is the
 * SPI clock and SDA is SPI MOSI. The panel is write-only in this design, so
 * there is no MISO connection.
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

/* Panel resolution in the display's native (unrotated) orientation.
 * This is a 2.0" 240x320 IPS panel, so the driver is initialised as 240 wide
 * by 320 tall and rotation 0 keeps it portrait, which is the orientation the
 * robot's face is designed for. */
#define TFT_WIDTH   240
#define TFT_HEIGHT  320
#define TFT_ROTATION 0

/* Serial baud rate. The XIAO ESP32C3 presents a native USB CDC port, so this
 * value is nominal on the USB link, but it is also what the Arduino IDE
 * Serial Monitor must be set to. */
#define SERIAL_BAUD 115200

/* ---------------------------------------------------------------------------
 * Display instance
 *
 * The four-argument-plus-reset constructor form selects the software SPI
 * signature, but because TFT_SCLK and TFT_MOSI are the ESP32-C3's hardware
 * SPI pins the underlying bus is still driven by the SPI peripheral. Passing
 * the pins explicitly, rather than relying on the board's default SPI object,
 * keeps the wiring self-documenting in the source.
 * -------------------------------------------------------------------------*/
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

/* Brings up the panel. Separated from setup() so the display bring-up
 * sequence stays readable as one unit. */
static void displayBegin() {
  tft.init(TFT_WIDTH, TFT_HEIGHT);
  tft.setRotation(TFT_ROTATION);

  /* The robot's face is white on black. Clearing to black here means the
   * panel never shows the random contents of its RAM at power-on. */
  tft.fillScreen(ST77XX_BLACK);
}

/* ---------------------------------------------------------------------------
 * Face geometry
 *
 * The face is intentionally minimalist: two solid rounded-rectangle eyes and
 * a simple arc smile, white on black. Design reasoning:
 *
 *  - Solid eyes with no pupils read as friendly and alert at a glance from
 *    across a desk, and they are cheap to redraw, which matters because the
 *    blink animation will repaint the eye region repeatedly.
 *  - Pure white on pure black suits the IPS panel's contrast and means the
 *    "erase" colour is the same as the background, so clearing a region is a
 *    single fillRect rather than a redraw of anything underneath.
 *  - Every coordinate is a named constant rather than a literal so the face
 *    can be re-proportioned in one place, and so the animation code can
 *    compute the exact bounding box it needs to repaint.
 *
 * Layout in the 240x320 portrait frame:
 *
 *        x=40        x=144
 *          +----+      +----+          y=96   eye top
 *          |    |      |    |
 *          +----+      +----+          y=168  eye bottom
 *               \____/                 y=242  mouth bottom
 * -------------------------------------------------------------------------*/

/* Foreground and background. FACE_BG doubles as the erase colour. */
#define FACE_FG  ST77XX_WHITE
#define FACE_BG  ST77XX_BLACK

/* Eyes. Two identical rounded rectangles, symmetric about the screen centre.
 * EYE_LEFT_X is derived from the panel width so the pair stays centred if the
 * eye size or the gap between them is changed. */
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

/* Adafruit_GFX drawCircleHelper() corner mask: bit 1 = top-right,
 * 2 = top-left, 4 = bottom-right, 8 = bottom-left. The two bottom quadrants
 * together form the smile. */
#define MOUTH_CORNER_MASK (4 | 8)

/* Draws both eyes in their fully open resting state. */
static void faceDrawEyes() {
  tft.fillRoundRect(EYE_LEFT_X,  EYE_TOP_Y, EYE_W, EYE_H, EYE_RADIUS, FACE_FG);
  tft.fillRoundRect(EYE_RIGHT_X, EYE_TOP_Y, EYE_W, EYE_H, EYE_RADIUS, FACE_FG);
}

/* Draws the smile. */
static void faceDrawMouth() {
  for (int i = 0; i < MOUTH_THICKNESS; i++) {
    tft.drawCircleHelper(MOUTH_CENTER_X, MOUTH_CENTER_Y,
                         MOUTH_RADIUS - i, MOUTH_CORNER_MASK, FACE_FG);
  }
}

/* Draws the complete resting face, clearing the frame first so it can be
 * called at any time to return the robot to its resting expression.
 *
 * This is the only routine that touches the whole frame. Animation must not
 * call it, because repainting the full 240x320 frame over SPI is visible as a
 * flicker; animation repaints only the region it changes. */
static void faceDraw() {
  tft.fillScreen(FACE_BG);
  faceDrawEyes();
  faceDrawMouth();
}

void setup() {
  Serial.begin(SERIAL_BAUD);

  /* Brief, bounded wait for the USB CDC host to attach. This is not a
   * while(!Serial) spin: the robot must boot and run with no computer
   * attached, so blocking forever on a serial connection is not acceptable. */
  const unsigned long serialWaitDeadline = millis() + 1500UL;
  while (!Serial && millis() < serialWaitDeadline) {
    delay(10);
  }

  Serial.println();
  Serial.println(F("Desk-Buddy booting"));
  Serial.println(F("  board : Seeed Studio XIAO ESP32C3"));
  Serial.printf("  pins  : CS=%d DC=%d SCLK=%d MOSI=%d RST=%d\n",
                TFT_CS, TFT_DC, TFT_SCLK, TFT_MOSI, TFT_RST);

  displayBegin();

  /* The ST7789 has no read-back path in this wiring, so there is no way to
   * confirm initialisation in software. A successful init is confirmed
   * visually: the panel backlight comes on and the screen goes black. */
  Serial.printf("  tft   : initialised %dx%d, rotation %d\n",
                tft.width(), tft.height(), TFT_ROTATION);

  faceDraw();
  Serial.println(F("  stage : resting face drawn (static, no animation)"));
}

void loop() {
  /* The face is static at this commit, so there is nothing to schedule yet.
   * The main loop is deliberately kept free of blocking delay() calls:
   * everything the robot will do -- animation, and later audio and presence
   * detection -- has to be scheduled cooperatively off millis(), because a
   * blocking wait in one subsystem would stall the others. */
}
