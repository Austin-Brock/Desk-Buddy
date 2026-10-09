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
 * Status of this file at this commit: the display is initialised and
 * cleared. No face is drawn yet.
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
  Serial.println(F("  stage : display cleared (no face drawn yet)"));
}

void loop() {
  /* Nothing to schedule yet. The main loop is deliberately kept free of
   * blocking delay() calls: everything the robot will do -- animation, and
   * later audio and presence detection -- has to be scheduled cooperatively
   * off millis(), because a blocking wait in one subsystem would stall the
   * others. */
}
