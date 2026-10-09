/* ===========================================================================
 * Desk-Buddy -- firmware entry point
 *
 * A small expressive desk productivity robot.
 * Target board: Seeed Studio XIAO ESP32C3 (RISC-V, 4 MB flash, 400 KB SRAM)
 *
 * This file is deliberately short. It owns boot order and the run loop, and
 * nothing else:
 *
 *   config.h     pin map, panel geometry, face proportions, animation timing
 *   display.h    hardware layer -- ST7789 bring-up, driver instance
 *   face.h       expression layer -- face geometry, blink state machine
 *   this file    behaviour layer -- boot sequence and cooperative scheduling
 *
 * The sketch folder name must match the .ino file name, which is why the
 * firmware lives at firmware/desk-buddy/desk-buddy.ino. Every .cpp beside it
 * is compiled into the sketch automatically.
 *
 * Build notes (see README.md for the full procedure):
 *   Board     : "XIAO_ESP32C3" from the esp32 board package by Espressif
 *   Libraries : Adafruit GFX Library, Adafruit ST7735 and ST7789 Library
 *   Serial    : 115200 baud
 *
 * Implemented: display, animated face, blinking.
 * Not implemented: audio, reminders, presence detection. See docs/.
 * ===========================================================================
 */

#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "face.h"

void setup() {
  Serial.begin(SERIAL_BAUD);

  /* Bounded wait for the USB CDC host to attach. This is not a while(!Serial)
   * spin: the robot must boot and run with no computer attached, so blocking
   * forever on a serial connection is not acceptable. */
  const unsigned long serialWaitDeadline = millis() + SERIAL_ATTACH_TIMEOUT_MS;
  while (!Serial && millis() < serialWaitDeadline) {
    delay(10);
  }

  Serial.println();
  Serial.println(F("Desk-Buddy booting"));
  Serial.println(F("  board : Seeed Studio XIAO ESP32C3"));
  Serial.printf("  pins  : CS=%d DC=%d SCLK=%d MOSI=%d RST=%d\n",
                TFT_CS, TFT_DC, TFT_SCLK, TFT_MOSI, TFT_RST);

  displayBegin();

  /* The panel has no read-back path in this wiring, so there is no way to
   * confirm initialisation in software. A successful init is confirmed
   * visually: the backlight comes on and the screen goes black. */
  Serial.printf("  tft   : initialised %dx%d, rotation %d\n",
                tft.width(), tft.height(), TFT_ROTATION);

  /* Seeded from micros() rather than from a hardware entropy source. The seed
   * only affects how the blink rhythm differs between power-ups, which is
   * cosmetic, and micros() avoids a dependency on an ESP-IDF header whose
   * location has moved between core versions. */
  randomSeed(micros());

  faceBegin(millis());
  Serial.println(F("  stage : face live, blink animation running"));
  Serial.println(F("  audio : not implemented"));
}

void loop() {
  /* Cooperative scheduling. Every subsystem is called once per iteration with
   * the current millis(), does at most one small unit of work, and returns.
   *
   * There is no delay() here and there must never be one: a blink spans about
   * 214 ms, and a blocking wait in any one subsystem would stall all the
   * others. The ESP32-C3 is single-core, so there is no second core to hide
   * blocking work on. See docs/architecture.md.
   */
  const unsigned long now = millis();

  faceAnimationUpdate(now);

  /* Planned, not implemented -- each will follow the same contract:
   *   audioUpdate(now);      poll the DFPlayer, dispatch due reminders
   *   presenceUpdate(now);   sample the PIR, update presence state
   */
}
