#include <Arduino.h>

#include "config.h"
#include "audio.h"

/* ---------------------------------------------------------------------------
 * UART selection
 *
 * Serial1 is the ESP32-C3's second hardware UART. Serial is left alone: on
 * this board it is the native USB CDC port carrying the console.
 *
 * The pins are passed to begin() explicitly, which routes UART1 to them
 * through the GPIO matrix. This is what makes D6/D7 usable here even though
 * they are UART0's default pins -- see the caution in config.h.
 * -------------------------------------------------------------------------*/
#define AUDIO_SERIAL Serial1

/* ---------------------------------------------------------------------------
 * DFPlayer command frame -- 10 bytes, fixed length
 *
 *   byte  0    1     2     3    4     5      6      7      8     9
 *        7E   FF    06   CMD   FB  PARA_H PARA_L CHK_H  CHK_L   EF
 *        |     |     |     |    |     |      |      |      |     |
 *      start  ver  len  command |   parameter    checksum      end
 *                               no-feedback flag
 *
 * The checksum is the 16-bit two's complement of the sum of bytes 1 through
 * 6. The feedback flag is left at 0: an acknowledgement would have to come
 * back over the module's TX line, which may not be connected on this build.
 * -------------------------------------------------------------------------*/
#define DF_START        0x7E
#define DF_VERSION      0xFF
#define DF_LEN          0x06
#define DF_NO_FEEDBACK  0x00
#define DF_END          0xEF

#define DF_CMD_SET_VOLUME  0x06  /* parameter: 0-30                        */
#define DF_CMD_PLAY_MP3    0x12  /* parameter: track number in /mp3/       */

#define DF_FRAME_LEN  10

static unsigned long audioReadyAtMs = 0;
static unsigned long audioLastCommandMs = 0;
static bool audioVolumeSent = false;

/* Assembles and transmits one command frame.
 *
 * Serial1.write() hands the bytes to the UART's transmit buffer and returns;
 * it does not wait for them to clock out. A frame is 10 bytes, about 10 ms of
 * wire time at 9600 baud, and none of that is spent blocking the run loop. */
static void audioSendCommand(uint8_t command, uint16_t parameter) {
  const uint8_t parameterHigh = (uint8_t)(parameter >> 8);
  const uint8_t parameterLow  = (uint8_t)(parameter & 0xFF);

  const uint16_t sum = (uint16_t)DF_VERSION + DF_LEN + command +
                       DF_NO_FEEDBACK + parameterHigh + parameterLow;
  const uint16_t checksum = (uint16_t)(0u - sum);

  const uint8_t frame[DF_FRAME_LEN] = {
    DF_START,
    DF_VERSION,
    DF_LEN,
    command,
    DF_NO_FEEDBACK,
    parameterHigh,
    parameterLow,
    (uint8_t)(checksum >> 8),
    (uint8_t)(checksum & 0xFF),
    DF_END
  };

  AUDIO_SERIAL.write(frame, DF_FRAME_LEN);
}

bool audioIsReady(unsigned long now) {
  /* Signed difference, so the check survives the millis() rollover. */
  return (long)(now - audioReadyAtMs) >= 0;
}

/* Shared gate for every outbound command: the module must be past its
 * power-up window, and frames must not be sent back to back. */
static bool audioCanSend(unsigned long now) {
  if (!audioIsReady(now)) {
    return false;
  }
  return (now - audioLastCommandMs) >= DFPLAYER_COMMAND_GAP_MS;
}

void audioBegin(unsigned long now) {
  AUDIO_SERIAL.begin(DFPLAYER_BAUD, SERIAL_8N1,
                     DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);

  audioVolumeSent = false;
  audioReadyAtMs = now + DFPLAYER_BOOT_MS;

  /* Backdated so the inter-command gap has already elapsed by the time the
   * module becomes ready, rather than adding a second wait on top of it. */
  audioLastCommandMs = now - DFPLAYER_COMMAND_GAP_MS;
}

void audioUpdate(unsigned long now) {
  /* The volume command is the one piece of deferred setup: it cannot be sent
   * from audioBegin() because the module is not listening yet. */
  if (!audioVolumeSent && audioCanSend(now)) {
    if (audioSetVolume(now, AUDIO_VOLUME)) {
      audioVolumeSent = true;
    }
  }
}

bool audioSetVolume(unsigned long now, uint8_t volume) {
  if (!audioCanSend(now)) {
    return false;
  }
  if (volume > 30) {
    volume = 30;
  }

  audioSendCommand(DF_CMD_SET_VOLUME, volume);
  audioLastCommandMs = now;
  return true;
}

bool audioPlayTrack(unsigned long now, uint16_t track) {
  if (track < 1 || track > AUDIO_TRACK_COUNT) {
    return false;
  }
  if (!audioCanSend(now)) {
    return false;
  }

  audioSendCommand(DF_CMD_PLAY_MP3, track);
  audioLastCommandMs = now;
  return true;
}
