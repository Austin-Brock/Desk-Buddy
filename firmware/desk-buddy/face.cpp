#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "face.h"

/* ---------------------------------------------------------------------------
 * Derived geometry -- implementation detail, not configuration.
 *
 * EYE_BAND_* is the bounding box of both eyes together: the only region the
 * blink animation is allowed to touch. It is derived from the eye geometry in
 * config.h so it stays correct if the face is re-proportioned.
 *
 * At the default proportions the band is 160x72 -- 22.5 KB at 16 bpp, against
 * 150 KB for the full frame, so one animation frame moves 15% of the data a
 * full repaint would. That difference is why the blink shows no flicker.
 * -------------------------------------------------------------------------*/
#define EYE_BAND_X  EYE_LEFT_X
#define EYE_BAND_Y  EYE_TOP_Y
#define EYE_BAND_W  ((EYE_RIGHT_X + EYE_W) - EYE_LEFT_X)
#define EYE_BAND_H  EYE_H

/* Adafruit_GFX drawCircleHelper() corner mask: bit 1 = top-right,
 * 2 = top-left, 4 = bottom-right, 8 = bottom-left. The two bottom quadrants
 * together form the smile. */
#define MOUTH_CORNER_MASK (4 | 8)

/* ---------------------------------------------------------------------------
 * Animation state
 *
 * All of it is file-local. Nothing outside this module can reach it, and
 * nothing modifies it from an interrupt context, so no synchronisation is
 * needed anywhere.
 * -------------------------------------------------------------------------*/
enum FaceAnimState {
  FACE_RESTING,        /* eyes open, waiting for the next blink   */
  FACE_BLINK_CLOSING,  /* eye height shrinking                    */
  FACE_BLINK_CLOSED,   /* holding shut                            */
  FACE_BLINK_OPENING   /* eye height growing back                 */
};

static FaceAnimState faceState = FACE_RESTING;
static uint8_t blinkStep = 0;          /* 0 = fully open, BLINK_STEPS = shut */
static unsigned long lastFrameMs = 0;  /* when the last frame was drawn      */
static unsigned long nextBlinkMs = 0;  /* when the next blink should start    */

/* ---------------------------------------------------------------------------
 * Drawing
 * -------------------------------------------------------------------------*/

/* Draws both eyes in their fully open resting state. */
static void faceDrawEyes() {
  tft.fillRoundRect(EYE_LEFT_X,  EYE_TOP_Y, EYE_W, EYE_H, EYE_RADIUS, FACE_FG);
  tft.fillRoundRect(EYE_RIGHT_X, EYE_TOP_Y, EYE_W, EYE_H, EYE_RADIUS, FACE_FG);
}

/* Draws the smile. Adafruit_GFX has no stroke width, so thickness comes from
 * concentric arcs at decreasing radii. */
static void faceDrawMouth() {
  for (int i = 0; i < MOUTH_THICKNESS; i++) {
    tft.drawCircleHelper(MOUTH_CENTER_X, MOUTH_CENTER_Y,
                         MOUTH_RADIUS - i, MOUTH_CORNER_MASK, FACE_FG);
  }
}

void faceDraw() {
  tft.fillScreen(FACE_BG);
  faceDrawEyes();
  faceDrawMouth();
}

/* Redraws both eyes at the given height, vertically centred on the resting
 * eye position, repainting only the eye band.
 *
 * The band is cleared and redrawn in one pass rather than erasing just the
 * difference between the old and new eye shapes. The difference region is
 * fiddly to compute for a rounded rectangle and the saving would be a few
 * hundred microseconds, so the simpler version wins. If the face ever gains a
 * background that is not flat black, this is the decision to revisit first. */
static void faceDrawEyesAtHeight(int16_t h) {
  if (h < EYE_CLOSED_H) {
    h = EYE_CLOSED_H;
  } else if (h > EYE_H) {
    h = EYE_H;
  }

  const int16_t y = EYE_TOP_Y + (EYE_H - h) / 2;

  /* Corner radius cannot exceed half the height, or fillRoundRect draws
   * corners that overlap and the shape degenerates. */
  int16_t r = EYE_RADIUS;
  if (r > h / 2) {
    r = h / 2;
  }

  tft.fillRect(EYE_BAND_X, EYE_BAND_Y, EYE_BAND_W, EYE_BAND_H, FACE_BG);

  if (r > 1) {
    tft.fillRoundRect(EYE_LEFT_X,  y, EYE_W, h, r, FACE_FG);
    tft.fillRoundRect(EYE_RIGHT_X, y, EYE_W, h, r, FACE_FG);
  } else {
    /* Guard for very small closed heights, where there is no room left to
     * round the corners. Not reached with the current EYE_CLOSED_H of 4,
     * which still yields a radius of 2. */
    tft.fillRect(EYE_LEFT_X,  y, EYE_W, h, FACE_FG);
    tft.fillRect(EYE_RIGHT_X, y, EYE_W, h, FACE_FG);
  }
}

/* ---------------------------------------------------------------------------
 * Blink animation
 *
 * Approach: the eyes are redrawn at a reduced height for a few frames, which
 * squints them shut and then reopens them. A rounded rectangle that loses
 * height while keeping its vertical centre already looks like a closing lid,
 * so no second shape is needed and nothing has to be known about what is
 * behind the lid. Only the eye band is touched.
 *
 * See docs/display.md for the full rationale, the per-step geometry table,
 * and the data budget behind the partial-redraw decision.
 * -------------------------------------------------------------------------*/

/* Picks the next blink time, at a random offset from now. Humans blink every
 * few seconds and irregularly; a fixed interval reads as mechanical, which is
 * exactly the impression this robot is trying to avoid. */
static void faceScheduleNextBlink(unsigned long now) {
  nextBlinkMs = now + (unsigned long)random(BLINK_INTERVAL_MIN_MS,
                                            BLINK_INTERVAL_MAX_MS);
}

/* Maps a blink step to an eye height: step 0 is fully open, BLINK_STEPS is
 * fully shut, with linear interpolation in between. Integer arithmetic is
 * used throughout -- the ESP32-C3 has no FPU, and single-pixel precision is
 * all this needs. */
static int16_t faceEyeHeightForStep(uint8_t step) {
  if (step == 0)           return EYE_H;
  if (step >= BLINK_STEPS) return EYE_CLOSED_H;

  const int16_t travel = EYE_H - EYE_CLOSED_H;
  return (int16_t)(EYE_H - (travel * (int32_t)step) / BLINK_STEPS);
}

void faceAnimationUpdate(unsigned long now) {
  switch (faceState) {
    case FACE_RESTING:
      /* Subtraction rather than a direct comparison so the check stays
       * correct across the millis() rollover at ~49 days of uptime. */
      if ((long)(now - nextBlinkMs) >= 0) {
        faceState = FACE_BLINK_CLOSING;
        blinkStep = 0;
        lastFrameMs = now;
      }
      break;

    case FACE_BLINK_CLOSING:
      if (now - lastFrameMs >= BLINK_FRAME_MS) {
        lastFrameMs = now;
        blinkStep++;
        faceDrawEyesAtHeight(faceEyeHeightForStep(blinkStep));
        if (blinkStep >= BLINK_STEPS) {
          faceState = FACE_BLINK_CLOSED;
        }
      }
      break;

    case FACE_BLINK_CLOSED:
      if (now - lastFrameMs >= BLINK_CLOSED_HOLD_MS) {
        lastFrameMs = now;
        faceState = FACE_BLINK_OPENING;
      }
      break;

    case FACE_BLINK_OPENING:
      if (now - lastFrameMs >= BLINK_FRAME_MS) {
        lastFrameMs = now;
        blinkStep--;
        faceDrawEyesAtHeight(faceEyeHeightForStep(blinkStep));
        if (blinkStep == 0) {
          faceState = FACE_RESTING;
          faceScheduleNextBlink(now);
        }
      }
      break;
  }
}

void faceBegin(unsigned long now) {
  faceDraw();
  faceState = FACE_RESTING;
  blinkStep = 0;
  lastFrameMs = now;
  faceScheduleNextBlink(now);
}
