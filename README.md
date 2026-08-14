# Hucheor Hardware

Hardware and firmware for **Hucheor**, a sound beacon for local shops that responds to the
standard French remote control used by blind and visually impaired people (NF S32-002, 868.3 MHz).

"Hucheor" is Old French for "one who calls out, who guides".

This repository holds everything meant to be freely shared and self-buildable:

- `firmware/` - ESP32 firmware (radio decoding, audio playback, WiFi configuration)
- `hardware/` - KiCad schematics and PCB Gerber files
- `enclosure/` - 3D-printable enclosure files (STL)
- `docs/` - screenshots and standalone HTML mockups of the config UI (for previewing/testing the
  web pages without flashing real hardware, see [Configuration UI](#configuration-ui) below)

The overall project (business context, deployment, website) lives in a separate private
repository. This one is the public, source-available part: anyone can fetch these files, build
their own Hucheor beacon for personal or internal use, and modify it. See
[NOTICE.md](NOTICE.md) for exactly what that does and doesn't allow - this is **not** an OSI-style
open source license, commercial resale is deliberately excluded.

## Status

**v1 firmware in active development.** Most of the software stack that doesn't depend on having
physical hardware in hand is implemented and tested (audio playback, scheduling, clock sync,
network, config UI, CI). What's still missing is the actual NF S32-002 radio decoding (blocked on
capturing a real signal, see [Approach](#approach)) and the physical prototype itself.

| Area | State |
|---|---|
| Radio reception (CC1101 @ 868.3 MHz) | Init + raw edge capture only. Frame matching (`matchesNfS32002Frame()`) always returns `false` for now - deliberately left unimplemented until a real RTL-SDR capture is available, see below. |
| Audio playback | Done. WAV (16-bit mono/stereo) from LittleFS over I2S, see [Audio playback](#audio-playback). |
| Opening-hours scheduling | Done. Seasonal models + week ranges, see [Scheduling](#scheduling-seasonal-opening-hours). |
| Clock sync | Done. Three independent sources (DCF77, phone, NTP), see [Keeping time](#keeping-time). |
| Network (AP / station modes) | Done. Automatic fallback, mDNS, configurable NTP, see [Network modes](#network-modes). |
| Config web UI | Done. See [Configuration UI](#configuration-ui). |
| Security | Audited, fixes applied, see [Security](#security). |
| CI | Firmware build on every push + weekly dependency check, see [Continuous integration](#continuous-integration). |
| Hardware / enclosure | Not started, waiting on a validated firmware prototype first. |

## How it works

1. A blind or visually impaired person carries the existing, nationally-distributed NF S32-002
   remote control (the same one already used for sound-enabled pedestrian crossings and public
   buildings). Nothing new to carry.
2. They point it at a shop equipped with a Hucheor beacon and press the button, exactly as they
   would in front of a public building's sound beacon.
3. The beacon (ESP32 + CC1101 radio module) picks up the 868.3 MHz signal, matches it against the
   NF S32-002 frame format, and reads the shop's configured opening hours for the current moment.
4. It plays back a short recorded message over a loudspeaker (MAX98357A I2S amp): the shop's name,
   and whether it's currently open or closed.

No app, no smartphone required for the core use case - the existing remote control is the whole
interface. See [Roadmap](#roadmap) for what a future companion app would add on top.

## Audio playback

`firmware/src/audio_player.{h,cpp}`. Plays 16-bit WAV files (mono or stereo, any sample rate)
stored on the ESP32's internal LittleFS filesystem, over I2S. The shopkeeper uploads two WAV
files through the config UI: one played when the shop is open, one when it's closed (see
[Configuration UI](#configuration-ui)). If neither has ever been uploaded (or the schedule was
never configured), the firmware falls back to a single generic `/message.wav` rather than staying
silent.

The WAV parser scans chunks instead of assuming a fixed 44-byte header (handles files with extra
`LIST`/`fact` chunks before `data`), and is hardened against malformed files (see
[Security](#security)).

No third-party audio library is used: the obvious choice, ESP8266Audio, is GPLv3-licensed, which
would force this whole repository under a copyleft license incompatible with
[the license below](#license). The WAV parsing and I2S playback here are implemented directly
against the ESP32 Arduino core's own `driver/i2s.h`.

## Scheduling (seasonal opening hours)

`firmware/src/schedule.{h,cpp}`. A shop's opening hours are rarely the same all year (summer
hours, Christmas closure, etc.), so scheduling is built around **models**, not a single fixed
weekly schedule:

- Up to **4 named models** (e.g. "Standard", "Été", "Vacances de Noël"), each a full 7-day weekly
  schedule (per-day enabled/disabled, open time, close time). A day can also be an overnight range
  (closes after midnight, e.g. a bar open 20:00-02:00) - the firmware detects this automatically
  when the close time is earlier than the open time.
- Up to **12 ISO week-number ranges** (1-53), each assigned to one of the 4 models. A week with no
  matching range falls back to the first model. Example: weeks 1-26 use "Standard", weeks 27-35
  use "Été", weeks 36-53 use "Standard" again.

All of this is edited from the phone's browser through the [config UI](#configuration-ui), no
reflashing needed to change hours.

**Fail-safe by design**: right after a cold boot or a power outage, before any clock source has
synced, the system clock reads close to zero (1970-01-01). `isOpenNow()` refuses to answer in that
case and reports "closed" rather than confidently guessing an arbitrary status from a bogus date -
"closed" is the safer wrong answer for a blind visitor than "open".

## Keeping time

The firmware never computes daylight-saving time (CET/CEST) itself. Instead, every clock source is
responsible for resolving DST on its own side and handing over **French local wall-clock time**,
encoded as if it were UTC - `Schedule` always reads it back with `gmtime_r()`, never
`localtime_r()`, so no EU DST rule is hardcoded anywhere in this firmware (a rule that could
change: there have been actual EU discussions about abolishing the seasonal switch entirely).

Three independent sources, any combination of which can be present:

- **DCF77** (`firmware/src/dcf77_clock.{h,cpp}`): an optional 77.5 kHz longwave receiver module.
  Decodes the German DCF77 time signal (received across France too), which already broadcasts
  French-equivalent local time plus live CEST/CET flags - no conversion needed, works with zero
  internet connection.
- **Phone sync**: visiting the config UI (any page) sends the configuring phone's own local
  wall-clock fields (already DST-adjusted by the phone's regularly-updated OS timezone database)
  to the beacon, via a tiny inline script.
- **NTP**: only available in [station mode](#network-modes) (internet access). A configurable NTP
  server (default `fr.pool.ntp.org`) provides true UTC, which the firmware converts to French local
  time exactly once per sync (a transient `TZ` rule applied and immediately unset - never left
  configured permanently) and re-syncs automatically every 6 hours to correct clock drift.

## Network modes

`firmware/src/network.{h,cpp}`. Two connection modes, switchable from the config UI's `/wifi`
page:

- **Standalone (default)**: the beacon creates its own WiFi access point (`Hucheor-XXXX`, WPA2,
  a password generated once per device). No internet access - relies on DCF77 and/or phone sync for
  the time.
- **Station**: the beacon joins the shop's own WiFi network instead. Easier to reach for
  configuration (phone stays on its usual network), discoverable via mDNS
  (`hucheor-xxxx.local`, no extra dependency - `ESPmDNS` ships with the Arduino ESP32 core), and
  gets automatic NTP time sync since it now has internet access.

Falls back to standalone automatically if station credentials are set but the connection fails
(wrong password, shop WiFi down, out of range) - a configuration mistake can never brick access to
the device.

Non-blocking and resilient by design: a dropped station connection is retried automatically (30s
cooldown, never floods the radio), and NTP resyncs happen in the background without ever stalling
radio reception or audio playback. The only intentionally blocking wait is the initial WiFi
connection attempt at boot (up to 15s, before anything else has started).

## Configuration UI

`firmware/src/config_server.{h,cpp}`. A small embedded HTTP server (no external assets, works
fully offline on the standalone AP) for the shopkeeper to configure the beacon from any phone
browser - no app required for setup. Protected by HTTP Digest Auth (a 12-digit code, generated
once per device, never sent in clear text over the network - see [Security](#security)). Every
page's footer shows the running firmware version (see [Versioning](#versioning)).

| Page | What it's for |
|---|---|
| `/` | Upload the "open" and "closed" WAV messages (2 MB cap each). |
| `/schedule` | Edit opening-hours models and assign week ranges to them, see [Scheduling](#scheduling-seasonal-opening-hours). |
| `/wifi` | Switch between standalone/station mode, set the shop's WiFi credentials, configure the NTP server. |

Screenshots (rendered from the standalone mockups in [`docs/mockups/`](docs/mockups/), which
mirror the firmware's actual generated HTML byte-for-byte - useful for previewing UI changes
without flashing a real device):

<table>
<tr>
<td><img src="docs/screenshots/config-index.png" width="260" alt="Config UI: home page with the two WAV upload forms"></td>
<td><img src="docs/screenshots/config-schedule.png" width="260" alt="Config UI: opening-hours schedule editor"></td>
<td><img src="docs/screenshots/config-wifi.png" width="260" alt="Config UI: network settings page"></td>
</tr>
</table>

Same visual theme as the public website ("Le Heraut": parchment/ink/terracotta), a system font
stack instead of the site's custom webfonts (unreachable from an offline AP with no internet), and
built to be usable: real `<label>`s, large touch targets, visible focus outlines, no JavaScript
beyond the one inline clock-sync script.

### Setup walkthrough

1. Flash the firmware (see [Building the firmware](#building-the-firmware)).
2. Power on the beacon. It starts in standalone mode, creating a WiFi network named
   `Hucheor-XXXX` (the exact suffix comes from the device's MAC address, printed to the serial
   console at boot alongside the WPA2 and config passwords).
3. On a phone, join that WiFi network and open `http://hucheor-xxxx.local` (or the AP's IP,
   `192.168.4.1`) in a browser.
4. Log in with the HTTP Digest credentials printed to serial at boot.
5. Upload the "open" and "closed" WAV messages on the home page.
6. Go to **Horaires d'ouverture** and set up at least the "Standard" model's weekly hours. Add
   seasonal models and week ranges if needed.
7. Optionally, go to **Réseau** and enter the shop's WiFi credentials to switch to station mode
   (recommended once set up: gets automatic internet time sync and easier day-to-day reachability
   via `hucheor-xxxx.local`).
8. Done - the beacon now responds to any NF S32-002 remote control in range.

## Security

Audited against a checklist adapted to this embedded-ESP32 context (not a typical web backend),
focused on the two pieces of new attack surface: the config server (handles untrusted uploads and
credentials) and the WAV parser (handles untrusted file content).

**Fixed:**
- Integer underflow in the WAV `fmt ` chunk parser (a malformed chunk size below 16 bytes wrapped
  around in unsigned arithmetic into a huge `seek()` offset) - now rejected explicitly.
- Unchecked `seek()` return value could let a malformed WAV file spin the parser loop forever
  (denial of service) - now aborts parsing on any failed seek.
- No upload size limit - capped at 2 MB per file, more than enough for a short voice message,
  truncated/corrupt uploads are deleted rather than left as the active message.
- `esp_random()` (used to generate the WiFi and HTTP passwords) was called before the WiFi radio
  was enabled, which weakens its entropy per Espressif's own guidance - reordered so radio
  initialization happens first.
- A single `portMAX_DELAY` I2S write with no timeout could hang the entire firmware forever if the
  audio amp ever stopped draining its DMA buffer (disconnected/faulty hardware) - replaced with a
  generously bounded 2-second timeout, playback aborts cleanly instead of freezing the device.

**By design, not a gap**: the config server uses HTTP Digest Auth (via `ESPAsyncWebServer`'s
default challenge), not Basic - the password is never sent in clear text, even on an unencrypted
WiFi network. Full TLS was considered and deliberately not added: no mature HTTPS support in the
web server library used here, a self-signed certificate would show a scary "not secure" warning to
non-technical shopkeepers on every visit, and the realistic threat model (an attacker would already
need to be on the same WiFi network as the shop) doesn't justify the added complexity.

**Accepted risk**: the two device passwords (WiFi AP + HTTP Digest) are a 12-digit numeric code
with no lockout after repeated failed attempts (10^12 combinations, and an attacker is already
constrained to physical WiFi proximity - not a service exposed to the internet).

## Continuous integration

Three GitHub Actions workflows:
- **Build** (`.github/workflows/build-firmware.yml`): compiles the firmware on every push/PR that
  touches `firmware/`.
- **Dependency check** (`.github/workflows/check-deps.yml`): runs weekly (and on demand), checks
  for outdated PlatformIO packages, and opens/closes a GitHub issue automatically depending on the
  result.
- **Release** (`.github/workflows/release.yml`): triggered by pushing a `vX.Y.Z` tag. Builds the
  firmware with that version stamped in (see [Versioning](#versioning) below) and publishes a
  GitHub Release with the compiled `firmware.bin`/`firmware.elf` attached.

## Versioning

The version shown at the bottom of every [config UI](#configuration-ui) page comes from the Git
tag used to build that firmware, never a hardcoded value in source. `firmware/scripts/gen_version.py`
(a PlatformIO pre-build script) generates `firmware/include/version.h` from the `FIRMWARE_VERSION`
environment variable, set to the pushed tag by the release workflow above - falls back to `"dev"`
for any build that isn't triggered by a tag (local builds, the plain CI build/PR checks), so it's
always obvious whether a given firmware came from an official release. Same pattern as this
ecosystem's mobile apps (see the admin repo's `docs/mobile-app-releases.md`): the version is
stamped in at build time, never committed.

To cut a release: `git tag vX.Y.Z && git push origin vX.Y.Z`.

## Building the firmware

Requires [PlatformIO](https://platformio.org/). From `firmware/`:

```sh
pio run          # compile
pio run -t upload  # flash to a connected ESP32
```

## Roadmap

- **v1 (MVP, in progress)**: NF S32-002 (868.3 MHz) reception + loudspeaker audio playback. See
  the [Status](#status) table above for exactly what's done.
- **v2 (Bluetooth LE Audio / Auracast)**: broadcast the message directly to compatible hearing aids
  and earbuds, alongside the loudspeaker. No source-available beacon combines 868.3 MHz reception
  with Auracast today. Not an ESP32-only feature: full Auracast (LC3 codec, Broadcast Isochronous
  Streams, certified interoperability) needs a dedicated co-processor, likely a Nordic nRF5340
  (candidate module: Aurawave AW100), talking to the ESP32 over UART/SPI.
- **v3 (companion mobile app)**: React Native / Expo (iOS + Android), consistent with the rest of
  the ecosystem's mobile apps. BLE beacon triggering, Auracast reception, directory of equipped
  shops, GPS navigation mode. Stays compatible with the existing physical NF S32-002 remote (the
  app complements it, never replaces it).
- **v4 (ecosystem)**: shop-owner management interface, OpenStreetMap-based beacon map, directory
  integration.

Priority stays on finishing v1 (real radio capture + a wired prototype) before starting v2.

## Accessibility

This project exists for blind and visually impaired people. Any interface built here (firmware web
config UI, future companion app) must target **WCAG 2.2 AA at minimum** from the first draft:
strong contrast, generous font sizes, no text conveyed only through images, full keyboard
navigation, correct ARIA attributes, respect for reduced-motion preferences. This is not optional
on this project.

## Approach

Hucheor's firmware is written independently, from scratch, using only the publicly documented
radio parameters of the NF S32-002 standard (868.3 MHz, OOK) - no borrowed code.

Radio frame decoding is deliberately left unimplemented (`matchesNfS32002Frame()` always returns
`false`) until a real signal has been captured with an RTL-SDR from an actual NF S32-002 remote -
no timing constants guessed or borrowed from any other project.

## License

**CC BY-NC 4.0** (Attribution-NonCommercial). See [NOTICE.md](NOTICE.md) for what this means in
practice, and [LICENSE.md](LICENSE.md) for the full legal text. In short: build one for yourself,
for a friend for free, or for your own organization's internal use, and modify/share freely - but
no one besides BREIZHZION may sell, charge for, or otherwise commercially provide a Hucheor
beacon or a device built from these files, including nonprofit associations.

This is deliberately **not an OSI-style open source license**: commercial resale is excluded, so
this project should be described as source-available, not open source.
