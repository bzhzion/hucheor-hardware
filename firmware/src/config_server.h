#pragma once

// Minimal WiFi access point + web server so a shopkeeper can upload a new
// audio message (WAV file), without any app or account. Deliberately simple
// HTML: no JS framework, real <label>s, visible focus outlines, large touch
// targets - same WCAG 2.2 AA baseline as the public website (see
// docs/hucheor.md "Regle accessibilite").
//
// Security: the WiFi AP itself is WPA2-protected with a per-device password
// (not shared across every Hucheor beacon), plus HTTP Basic Auth on top in
// case the WiFi password leaks. Both are generated once and stored in NVS.
// Meant to be printed on a label inside the enclosure (physical possession
// of the box is the trust boundary), not displayed publicly - anyone who can
// reach the AP without those credentials must not be able to change what
// the beacon announces.

namespace ConfigServer {

// Starts a WiFi AP named "Hucheor-XXXX" (XXXX from the chip's MAC) and an
// HTTP server on port 80 serving the upload form. Both protected, see above.
void begin();

} // namespace ConfigServer
