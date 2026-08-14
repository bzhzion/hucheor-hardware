# Firmware

ESP32 firmware for the Hucheor beacon, built with [PlatformIO](https://platformio.org/). See the
[repository root README](../README.md) for the full walkthrough (setup, security posture, CI),
this file stays a short per-module summary.

## Status

- Radio: initialization and raw edge capture only. NF S32-002 frame decoding is **not
  implemented** yet: see `src/main.cpp` for details on why (it needs real timing data from our
  own RTL-SDR capture, not guessed or borrowed values).
- Audio (`audio_player.h`/`.cpp`): plays a mono/stereo 16-bit PCM WAV file from LittleFS over I2S.
  Native ESP32 I2S driver only, no third-party audio library (the obvious choice, ESP8266Audio, is
  GPLv3: incompatible with this repo's CC BY-NC 4.0 license).
- Network (`network.h`/`.cpp`): two connection modes. Standalone (default) creates its own WiFi
  AP (`Hucheor-XXXX`); station joins the shop's own WiFi instead, falling back to standalone
  automatically if that fails. Station mode advertises itself via mDNS (`hucheor-xxxx.local`) for
  a future companion app to discover it, and syncs time via NTP since it has internet access.
- Config server (`config_server.h`/`.cpp`): accessible HTML pages (real `<label>`s, visible focus
  outlines, only JS on the page is a tiny clock-sync snippet - WCAG 2.2 AA baseline required
  project-wide) to upload separate open/closed messages, edit opening hours, and switch WiFi mode.
- Schedule (`schedule.h`/`.cpp`): up to 4 seasonal weekly models (e.g. "Standard", "Summer"), and
  up to 12 ISO week ranges (1-53) assigning which model applies when.
- Clock: three independent sources, none require any DST rule hardcoded in this firmware (see
  `schedule.h`'s top comment) - the phone's clock (auto-synced whenever the shopkeeper opens the
  config page), a **DCF77 longwave receiver** (`dcf77_clock.h`/`.cpp`, one GPIO pin, written from
  scratch against the public DCF77 telegram spec, same footing as this project's own NF S32-002
  work), and NTP (only available in station mode).
- Not yet wired up: the frame-decode -> audio-playback trigger exists in `main.cpp` but calls a
  decoder that always returns `false` for now.

## Build

```
cd firmware
pio run
```

## Hardware (planned)

- MCU: ESP32 (S3 or C3 preferred)
- Radio: CC1101 (SPI), 868.3 MHz OOK
- Audio: MAX98357A (I2S) + speaker
- Clock: DCF77 receiver module (~5-10 EUR, one GPIO pin), optional but recommended
- Wiring: not finalized, see `src/main.cpp` pin comments; will be updated once the first
  prototype is actually wired and tested.

## License

Code in this folder is licensed under CC BY-NC 4.0 (see repository root `LICENSE.md` /
`NOTICE.md`). Third-party dependencies pulled via PlatformIO keep their own licenses:
- `CC1101-ESP-Arduino`: MIT
- `ESPAsyncWebServer` / `AsyncTCP`: LGPL-3.0 (weak copyleft, safe to depend on: unlike GPL/AGPL,
  LGPL explicitly allows linking from differently-licensed code)
- LittleFS: bundled with the Arduino ESP32 core itself, not a separate dependency

Rejected: `ESP8266Audio` (GPLv3: would force this whole firmware under GPL, incompatible with
CC BY-NC 4.0).
