/* ===========================================================================
 * motion.h -- HC-SR501 PIR motion input
 *
 * Hardware layer. Owns the sensor pin, the warm-up period, and edge
 * detection. It reports that motion *started*; it does not decide what the
 * robot should do about it, which belongs to the behaviour layer.
 *
 * STATUS: implemented, NOT verified on hardware. The sensor itself is
 * verified -- motion detection was proven with a standalone bench sketch --
 * but this module has never been run. See docs/wiring.md.
 * ===========================================================================
 */

#ifndef DESK_BUDDY_MOTION_H
#define DESK_BUDDY_MOTION_H

#include <Arduino.h>

/* Configures the sensor pin and starts the warm-up timer. Call once, from
 * setup(). */
void motionBegin(unsigned long now);

/* Samples the sensor and detects edges. Call from loop(); returns
 * immediately when no sample is due, and never blocks. */
void motionUpdate(unsigned long now);

/* Returns true once for each new motion event, then clears the flag -- so
 * exactly one caller consumes each event and a single trigger cannot be
 * acted on twice.
 *
 * "New motion event" means a LOW-to-HIGH transition on the sensor output.
 * Note what this does NOT mean: the HC-SR501 holds its output high for a
 * configurable period after it fires, so continuous movement produces far
 * fewer events than you might expect, and the output returning low means the
 * hold period expired rather than that motion stopped. */
bool motionTriggered();

/* True while the sensor is still within its post-power-up warm-up window, in
 * which its output is not trustworthy. Motion events are discarded during
 * this period. */
bool motionIsWarmingUp(unsigned long now);

/* The last accepted level of the sensor output. Exposed for diagnostics; the
 * behaviour layer should use motionTriggered() instead, because this goes low
 * when the sensor's hold period expires rather than when a person leaves. */
bool motionLevel();

#endif /* DESK_BUDDY_MOTION_H */
