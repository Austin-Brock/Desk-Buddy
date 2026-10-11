#include <Arduino.h>

#include "config.h"
#include "motion.h"

/* ---------------------------------------------------------------------------
 * All state is file-local, as with the other modules: nothing outside this
 * translation unit can reach it, and nothing is touched from an interrupt
 * context, so no synchronisation is required.
 *
 * The sensor is polled rather than interrupt-driven. A PIR changes state on a
 * human timescale, an interrupt would buy nothing, and polling keeps every
 * subsystem in this firmware on the same cooperative model.
 * -------------------------------------------------------------------------*/
static bool motionLastLevel = false;      /* last accepted sensor level     */
static bool motionEventPending = false;   /* set on a rising edge           */
static unsigned long motionWarmUpUntilMs = 0;
static unsigned long motionLastSampleMs = 0;
static unsigned long motionLastEdgeMs = 0;

void motionBegin(unsigned long now) {
  /* Plain INPUT, no pull resistor: the HC-SR501 drives its output actively
   * both high and low, so a pull would only fight it. */
  pinMode(PIR_PIN, INPUT);

  motionLastLevel = (digitalRead(PIR_PIN) == HIGH);
  motionEventPending = false;
  motionLastSampleMs = now;
  motionLastEdgeMs = now;
  motionWarmUpUntilMs = now + PIR_WARMUP_MS;
}

bool motionIsWarmingUp(unsigned long now) {
  /* Signed difference so the comparison survives the millis() rollover at
   * ~49 days, matching the pattern used by the blink animation. */
  return (long)(now - motionWarmUpUntilMs) < 0;
}

void motionUpdate(unsigned long now) {
  if (now - motionLastSampleMs < PIR_SAMPLE_INTERVAL_MS) {
    return;
  }
  motionLastSampleMs = now;

  const bool level = (digitalRead(PIR_PIN) == HIGH);
  if (level == motionLastLevel) {
    return;
  }

  /* Reject edges that arrive too soon after the previous one. The sensor's
   * real transitions are hundreds of milliseconds apart at the very least, so
   * anything faster is noise rather than a person.
   *
   * Note that motionLastLevel is deliberately left unchanged here, which
   * gives the guard a useful asymmetry: a glitch that reverts within the
   * window is discarded entirely, because by the next sample the level
   * matches again and there is no edge to report -- while a genuine
   * transition is merely accepted late, on the first sample after the window
   * closes, rather than being lost. */
  if (now - motionLastEdgeMs < PIR_EDGE_GUARD_MS) {
    return;
  }

  motionLastEdgeMs = now;
  motionLastLevel = level;

  /* Only a rising edge is an event, and only once the sensor is trustworthy.
   * During warm-up the level is still tracked -- so the module knows where it
   * stands when the window closes -- but no event is raised. */
  if (level && !motionIsWarmingUp(now)) {
    motionEventPending = true;
  }
}

bool motionTriggered() {
  if (!motionEventPending) {
    return false;
  }
  motionEventPending = false;
  return true;
}

bool motionLevel() {
  return motionLastLevel;
}
