# Firmware

ESP32 firmware for the Hucheor beacon, built with [PlatformIO](https://platformio.org/).

## Status

- Radio: initialization and raw edge capture only. NF S32-002 frame decoding is **not
  implemented** yet — see `src/main.cpp` for details on why (it needs real timing data from our
  own RTL-SDR capture, not guessed or borrowed values).
- Audio (`audio_player.h`/`.cpp`): plays a mono/stereo 16-bit PCM WAV file from LittleFS over I2S.
  Native ESP32 I2S driver only, no third-party audio library (the obvious choice, ESP8266Audio, is
  GPLv3 — incompatible with this repo's CC BY-NC 4.0 license).
- Config server (`config_server.h`/`.cpp`): WiFi AP (`Hucheor-XXXX`) + a plain HTML page to upload
  a new `message.wav`. Simple accessible markup (real `<label>`, visible focus outline, no JS),
  matches the WCAG 2.2 AA baseline required project-wide.
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
