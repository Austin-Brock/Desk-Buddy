/* ===========================================================================
 * audio.h -- DFPlayer MP3 playback over UART
 *
 * Hardware layer. Owns the UART, the DFPlayer command frame format, and the
 * module's readiness timing. It plays a numbered clip when asked; it does not
 * decide when a clip should play, which belongs to the behaviour layer.
 *
 * Commands are assembled directly rather than through a DFPlayer library.
 * The protocol is ten bytes with a checksum and this firmware uses two
 * commands, so a dependency would cost more than it saves. The frame format
 * is documented in docs/audio.md.
 *
 * STATUS: implemented, NOT verified on hardware. The module itself is
 * verified -- three clips played from a standalone bench sketch -- but this
 * module has never been run.
 * ===========================================================================
 */

#ifndef DESK_BUDDY_AUDIO_H
#define DESK_BUDDY_AUDIO_H

#include <Arduino.h>

/* Opens the UART and starts the module's readiness timer. Call once, from
 * setup().
 *
 * Does not block: the DFPlayer ignores commands for a period after power-up,
 * and rather than waiting out that period here, audioBegin() records when it
 * expires and audioUpdate() sends the volume command once it has. */
void audioBegin(unsigned long now);

/* Services deferred work -- currently the post-power-up volume command. Call
 * from loop(); returns immediately when there is nothing to do, and never
 * blocks. */
void audioUpdate(unsigned long now);

/* Requests playback of /mp3/NNNN.mp3 by track number, 1-based, using the
 * DFPlayer's folder-playback command.
 *
 * Returns false and does nothing if the module is not ready yet, if the
 * previous command was too recent, or if the track number is out of range.
 * The caller should treat false as "not played" -- notably, it must not
 * advance its track rotation or start a cooldown.
 *
 * Returning true means the command frame was handed to the UART, not that
 * audio was heard. Nothing in this firmware can confirm playback: the
 * module's BUSY line is not wired and no status frame is read. */
bool audioPlayTrack(unsigned long now, uint16_t track);

/* Sets playback volume, 0-30. Returns false under the same conditions as
 * audioPlayTrack(). */
bool audioSetVolume(unsigned long now, uint8_t volume);

/* True once the module's post-power-up window has elapsed and commands are
 * being sent. This reflects elapsed time only -- it is not an acknowledgement
 * from the module, which cannot be obtained without its TX line wired. */
bool audioIsReady(unsigned long now);

#endif /* DESK_BUDDY_AUDIO_H */
