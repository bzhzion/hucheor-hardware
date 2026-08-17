#pragma once

// Minimal WAV (PCM, mono or stereo, 16-bit) playback over I2S, reading from
// LittleFS. Deliberately not using a third-party audio library: the obvious
// candidate (ESP8266Audio) is GPLv3, which is incompatible with this
// repository's BZ-1.1 license (GPL requires permitting redistribution that
// BZ-1.1 forbids). Native I2S output only needs the driver already bundled
// with the Arduino ESP32 core.

#include <Arduino.h>

namespace AudioPlayer {

// pinBck/pinWs/pinData: I2S pins wired to the MAX98357A (BCLK, LRC, DIN).
bool begin(int pinBck, int pinWs, int pinData);

// Plays a mono or stereo 16-bit PCM WAV file from LittleFS. Blocking: fine
// for v1 (a beacon plays one short message at a time), revisit if the
// firmware later needs to stay responsive to the radio while playing.
bool playWavFile(const char *path);

} // namespace AudioPlayer
