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
 *   Board:  "XIAO_ESP32C3" from the esp32 board package by Espressif
 *   Serial: 115200 baud
 *
 * Status of this file at this commit: hardware skeleton only. It brings up
 * the serial link and prints a boot banner so the board, the USB CDC serial
 * path, and the pin definitions can be verified before any peripheral code
 * is introduced.
 * ===========================================================================
 */

#include <Arduino.h>

/* ---------------------------------------------------------------------------
 * Pin map
 *
 * The XIAO silkscreen labels (D0, D2, D8, D10) are provided as macros by the
 * XIAO_ESP32C3 variant in the esp32 board package. They are used here instead
 * of raw GPIO numbers so the code reads the same way the board is labelled.
 *
 *   Silkscreen   GPIO   Role on this board
 *   ----------   ----   ----------------------------------------------------
 *   D0           2      SPI chip select for the display
 *   D2           4      SPI data/command select for the display
 *   D8           8      SPI clock  (SCK)  -- hardware SPI peripheral pin
 *   D10          10     SPI data   (MOSI) -- hardware SPI peripheral pin
 *
 * Hardware decision: D8 and D10 are the XIAO's native hardware SPI SCK and
 * MOSI pins. Wiring the display clock and data to these specific pins is what
 * allows the display to be driven by the ESP32-C3 SPI peripheral rather than
 * by bit-banged software SPI, which is roughly an order of magnitude slower
 * and would make full-screen redraws visibly sluggish.
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
 * declared as "absent" and the panel is reset over SPI by its software reset
 * command during initialisation instead. Kept as a named constant so the
 * reason is recorded at the point of use. See docs/display.md. */
#define TFT_RST   -1

/* Serial baud rate. The XIAO ESP32C3 presents a native USB CDC port, so this
 * value is nominal on the USB link, but it is also what the Arduino IDE
 * Serial Monitor must be set to. */
#define SERIAL_BAUD 115200

/* Interval of the boot-stage heartbeat message, in milliseconds. */
#define HEARTBEAT_INTERVAL_MS 2000UL

static unsigned long lastHeartbeatMs = 0;

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
  Serial.println(F("  stage : firmware skeleton (no peripherals initialised)"));
}

void loop() {
  /* The main loop is deliberately non-blocking and free of delay() calls.
   * Everything the robot does -- animation, and later audio and presence
   * detection -- has to be scheduled cooperatively off millis(), because a
   * blocking delay in one subsystem would stall the others. The heartbeat
   * below establishes that pattern from the first commit. */
  const unsigned long now = millis();

  if (now - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatMs = now;
    Serial.printf("[%lu ms] alive\n", now);
  }
}
