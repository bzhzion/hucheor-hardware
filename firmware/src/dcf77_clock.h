#pragma once

// DCF77 longwave time receiver (77.5 kHz, Mainflingen, Germany), reachable
// across all of France and used by every "horloge radio-pilotee" sold here.
// Written from scratch against the public DCF77 telegram specification
// (published openly by PTB, the German metrology institute) - not derived
// from any third-party DCF77 library's code, only the standard's own bit
// layout, which is public technical documentation, same category as the
// NF S32-002 standard this project already implements against.
//
// Why this exists alongside the phone-clock sync in ConfigServer: a DCF77
// receiver module (~5-10 EUR, one GPIO pin) keeps the beacon's clock
// accurate continuously and autonomously, without depending on the
// shopkeeper revisiting the config page. Wire it once and the open/closed
// schedule (see schedule.h) stays correct indefinitely - no internet, no
// WiFi station mode, no NTP needed.
//
// Signal shape expected on the pin: HIGH during each second's amplitude-
// reduced pulse (~100ms = bit 0, ~200ms = bit 1), LOW the rest of the
// second, and no pulse at all during second 59 (marks the start of a new
// minute). This matches the output polarity of common cheap DCF77 receiver
// modules; if a specific module inverts this, invert the pin read in
// dcf77_clock.cpp's interrupt handler rather than the decode logic.

#include <Arduino.h>

namespace Dcf77Clock {

// Attaches an interrupt on `pin`. Call once from setup().
void begin(int pin);

// Call from loop() as often as convenient (cheap: just checks a flag).
// Returns true exactly once, the moment a full minute's telegram has been
// received and passed its parity checks - at which point the device's
// system clock has already been updated via Schedule::setCurrentTime().
bool poll();

} // namespace Dcf77Clock
