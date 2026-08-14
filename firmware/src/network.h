#pragma once

#include <Arduino.h>

// Two connection modes:
// - Standalone (default, no configuration needed): the beacon creates its
//   own WiFi access point ("Hucheor-XXXX"), as before. No internet access,
//   so no NTP - relies on DCF77 and/or the phone-clock sync (see
//   schedule.h) for the time.
// - Station: the beacon joins the shop's own WiFi network instead. Easier
//   to reach for configuration (same network as the shopkeeper's other
//   devices, discoverable via mDNS instead of having to find/join a
//   dedicated hotspot each time), and - since it now has internet access -
//   time syncs automatically via NTP, no phone/DCF77 needed (though both
//   still work as extra sources).
//
// Falls back to standalone automatically if station credentials are set
// but the connection fails (wrong password, shop WiFi down, out of range),
// so a configuration mistake can never brick access to the device entirely.

namespace Network {

void begin();

bool isStation();

// "hucheor-XXXX" (from the chip's MAC) - the AP's SSID in standalone mode,
// and the mDNS name (reachable as "<hostname>.local") in station mode.
String hostname();

// Standalone mode's own WiFi password (WPA2), generated once per device and
// stored in NVS - see config_server.h for the matching HTTP Basic Auth
// password, generated the same way but kept separate.
String apPassword();

String stationSsid();

// Stores new station credentials and restarts the device to apply them
// (simplest reliable way to cleanly retry Network::begin() with the new
// settings, rather than tearing down and reconfiguring WiFi live).
void configureStation(const String &ssid, const String &password);

} // namespace Network
