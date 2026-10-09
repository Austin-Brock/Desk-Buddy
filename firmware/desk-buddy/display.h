/* ===========================================================================
 * display.h -- ST7789 panel bring-up
 *
 * Hardware layer. Owns the driver instance and the panel's initialisation
 * sequence, and nothing else. This module knows about pixels and pins; it
 * knows nothing about faces.
 * ===========================================================================
 */

#ifndef DESK_BUDDY_DISPLAY_H
#define DESK_BUDDY_DISPLAY_H

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

/* The driver instance. Exposed because the drawing code in face.cpp issues
 * GFX primitives against it directly; wrapping every primitive would add a
 * layer of indirection without adding anything. */
extern Adafruit_ST7789 tft;

/* Initialises the panel at the configured resolution and rotation, and clears
 * it to black. Call once, from setup(), before anything is drawn.
 *
 * There is no return value because there is nothing to report: only MOSI is
 * wired, so the firmware cannot read the controller's registers and cannot
 * confirm that initialisation succeeded. Display faults are diagnosed
 * visually. */
void displayBegin();

#endif /* DESK_BUDDY_DISPLAY_H */
