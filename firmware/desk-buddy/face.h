/* ===========================================================================
 * face.h -- the robot's face and its animation
 *
 * Expression layer. Owns the face's geometry, the blink state machine, and
 * the repaint regions. Depends on the display module for output and on
 * config.h for its proportions and timing; depends on nothing else.
 *
 * The behaviour layer talks to the face through this header. It must not need
 * to know how a blink is drawn, and this module must not need to know why a
 * blink was requested.
 * ===========================================================================
 */

#ifndef DESK_BUDDY_FACE_H
#define DESK_BUDDY_FACE_H

#include <Arduino.h>

/* Draws the resting face and schedules the first blink. Call once, from
 * setup(), after displayBegin(). */
void faceBegin(unsigned long now);

/* Draws the complete resting face, clearing the frame first, so it can be
 * called at any time to return the robot to its resting expression.
 *
 * This is the only routine that touches the whole frame. Animation must not
 * call it: repainting all 240x320 pixels pushes 150 KB over SPI and is
 * visible as a black flash. */
void faceDraw();

/* Advances the face animation by at most one frame. Call from loop() as often
 * as possible. Returns immediately when there is nothing to do, and never
 * blocks -- a blink spans about 214 ms, and spending that in delay() would
 * stall every other subsystem. */
void faceAnimationUpdate(unsigned long now);

#endif /* DESK_BUDDY_FACE_H */
