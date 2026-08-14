#pragma once

// Minimal WiFi access point + web server so a shopkeeper can upload a new
// audio message (WAV file) and see the beacon's status, without any app or
// account. Deliberately simple HTML: no JS framework, real <label>s, visible
// focus outlines, large touch targets - same WCAG 2.2 AA baseline as the
// public website (see docs/hucheor.md "Regle accessibilite").

namespace ConfigServer {

// Starts a WiFi AP named "Hucheor-XXXX" (XXXX from the chip's MAC) and an
// HTTP server on port 80 serving the upload form.
void begin();

} // namespace ConfigServer
