# Firmware

ESP32 firmware for the Hucheor beacon, built with [PlatformIO](https://platformio.org/).

## Status

- Radio: initialization and raw edge capture only. NF S32-002 frame decoding is **not
  implemented** yet — see `src/main.cpp` for details on why (it needs real timing data from our
  own RTL-SDR capture, not guessed or borrowed values).
- Audio (`audio_player.h`/`.cpp`): plays a mono/stereo 16-bit PCM WAV file from LittleFS over I2S.
  Native ESP32 I2S driver only, no third-party audio library (the obvious choice, ESP8266Audio, is
  GPLv3 — incompatible with this repo's CC BY-NC 4.0 license).
- Config server (`config_server.h`/`.cpp`): WiFi AP (`Hucheor-XXXX`) + accessible HTML pages to
  upload separate open/closed messages and edit opening hours. Real `<label>`s, visible focus
  outlines, only JS on the page is a tiny clock-sync snippet - matches the WCAG 2.2 AA baseline
  required project-wide.
- Schedule (`schedule.h`/`.cpp`): up to 4 seasonal weekly models (e.g. "Standard", "Summer"), and
  up to 12 ISO week ranges (1-53) assigning which model applies when.
- Clock: two independent sources, neither needs internet. The phone's clock, auto-synced whenever
  the shopkeeper opens the config page (drifts between visits); and a **DCF77 longwave receiver**
  (`dcf77_clock.h`/`.cpp`, one GPIO pin) for continuous accurate time with no phone involvement -
  written from scratch against the public DCF77 telegram spec, same footing as this project's own
  NF S32-002 work.
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
- `CC1101-ESP-Arduino` — MIT
- `ESPAsyncWebServer` / `AsyncTCP` — LGPL-3.0 (weak copyleft, safe to depend on: unlike GPL/AGPL,
  LGPL explicitly allows linking from differently-licensed code)
- LittleFS — bundled with the Arduino ESP32 core itself, not a separate dependency

Rejected: `ESP8266Audio` (GPLv3 — would force this whole firmware under GPL, incompatible with
CC BY-NC 4.0).
