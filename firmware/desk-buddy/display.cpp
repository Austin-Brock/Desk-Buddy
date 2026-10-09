#include <Arduino.h>

#include "config.h"
#include "display.h"

/* ---------------------------------------------------------------------------
 * The four-argument-plus-reset constructor form selects the software SPI
 * signature, but because TFT_SCLK and TFT_MOSI are the ESP32-C3's hardware
 * SPI pins the underlying bus is still driven by the SPI peripheral. Passing
 * the pins explicitly, rather than relying on the board's default SPI object,
 * keeps the wiring self-documenting in the source.
 * -------------------------------------------------------------------------*/
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

void displayBegin() {
  tft.init(TFT_WIDTH, TFT_HEIGHT);
  tft.setRotation(TFT_ROTATION);

  /* Clear immediately after init so the panel never shows the random contents
   * of its RAM at power-on. */
  tft.fillScreen(DISPLAY_CLEAR_COLOR);
}
