/* ===========================================================================
 * Desk-Buddy -- firmware entry point
 *
 * A small expressive desk productivity robot.
 * Target board: Seeed Studio XIAO ESP32C3 (RISC-V, 4 MB flash, 400 KB SRAM)
 *
 * This file is deliberately short. It owns boot order and the run loop, and
 * nothing else:
 *
 *   config.h     pin map, panel geometry, face proportions, timing, volume
 *   display.h    hardware layer -- ST7789 bring-up, driver instance
 *   motion.h     hardware layer -- HC-SR501 PIR input, edge detection
 *   audio.h      hardware layer -- DFPlayer UART, command frames
 *   face.h       expression layer -- face geometry, blink state machine
 *   this file    behaviour layer -- boot sequence, cooperative scheduling,
 *                and the motion-triggered audio policy
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
 * VERIFICATION STATUS -- read this before trusting any of it:
 *
 *   Display, face, blink animation ... verified running on hardware.
 *   Motion-triggered audio .......... IMPLEMENTED BUT UNVERIFIED. The PIR and
 *                                     the DFPlayer have each been proven with
 *                                     separate bench sketches, but this
 *                                     firmware has never been run, and the
 *                                     three subsystems have never operated
 *                                     together on one build.
 *   Interval reminders, moods ....... not implemented.
 *
 * See docs/project-roadmap.md.
 * ===========================================================================
 */

#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "face.h"
#include "motion.h"
#include "audio.h"

/* ---------------------------------------------------------------------------
 * Behaviour layer -- motion-triggered audio
 *
 * Policy: a new motion event plays the next clip in rotation, subject to a
 * cooldown. This is the behaviour layer's business, not the sensor module's
 * and not the audio module's -- motion.cpp reports that movement started,
 * audio.cpp plays a numbered clip, and the decision connecting them lives
 * here.
 *
 * Why a cooldown at all: a person working at a desk generates near-continuous
 * movement, so without one the robot would retrigger constantly and talk over
 * itself. Twenty seconds matches the spacing used in the bench test.
 *
 * What the cooldown is measured from: the moment the command is sent, not the
 * moment playback finishes. The DFPlayer's BUSY line is not wired and clip
 * durations have not been measured, so completion is not observable. If a
 * clip ever runs longer than the cooldown, the next trigger will arrive while
 * it is still playing. See docs/audio.md.
 * -------------------------------------------------------------------------*/
static unsigned long lastAudioMs = 0;
static uint16_t nextTrack = 1;

static void behaviourUpdate(unsigned long now) {
  /* Consume the event unconditionally. Reading it here even when the cooldown
   * blocks playback is deliberate: the alternative is leaving it latched, so
   * that a trigger during the cooldown fires the instant the cooldown ends,
   * which would make the robot respond to movement that happened twenty
   * seconds earlier. */
  if (!motionTriggered()) {
    return;
  }

  if (now - lastAudioMs < MOTION_COOLDOWN_MS) {
    Serial.printf("[%lu ms] motion -- ignored, cooldown
", now);
    return;
  }

  if (!audioPlayTrack(now, nextTrack)) {
    /* The audio module refused -- not ready, or a command was too recent.
     * Neither the cooldown nor the track rotation advances, so the next
     * motion event tries the same clip again. */
    Serial.printf("[%lu ms] motion -- audio not ready, track %u not sent
",
                  now, (unsigned)nextTrack);
    return;
  }

  Serial.printf("[%lu ms] motion -- playing /mp3/%04u.mp3
",
                now, (unsigned)nextTrack);

  lastAudioMs = now;
  nextTrack = (uint16_t)((nextTrack % AUDIO_TRACK_COUNT) + 1);
}

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

  const unsigned long bootMs = millis();
  faceBegin(bootMs);

  motionBegin(bootMs);
  audioBegin(bootMs);

  /* Backdated so the first motion event is not swallowed by a cooldown that
   * has not had a chance to elapse yet. Unsigned wraparound makes this safe
   * even at boot, when millis() is near zero. */
  lastAudioMs = bootMs - MOTION_COOLDOWN_MS;

  Serial.printf("  pir   : OUT=D1 (GPIO3), warm-up %lu s
",
                PIR_WARMUP_MS / 1000UL);
  Serial.printf("  audio : UART1 tx=D7 rx=D6 @ %d baud, volume %d/30, "
                "%d tracks
",
                DFPLAYER_BAUD, AUDIO_VOLUME, AUDIO_TRACK_COUNT);
  Serial.println(F("  stage : face live, blink animation running"));
  Serial.println(F("  stage : motion-triggered audio active (UNVERIFIED)"));
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

  /* Order matters only in that the animation goes first: it is the subsystem
   * a human would notice stuttering. Each call does at most one small unit of
   * work, so none of them starves the others. */
  faceAnimationUpdate(now);  /* repaint at most one animation frame */
  motionUpdate(now);         /* sample the PIR, detect edges        */
  audioUpdate(now);          /* deferred DFPlayer setup             */
  behaviourUpdate(now);      /* motion event -> play the next clip  */
}
