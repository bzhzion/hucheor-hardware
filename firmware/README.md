# Firmware

ESP32 firmware for the Hucheor beacon, built with [PlatformIO](https://platformio.org/).

## Status

Radio initialization and raw edge capture only. NF S32-002 frame decoding is **not implemented**
yet — see `src/main.cpp` for details on why (it needs real timing data from our own RTL-SDR
capture, not guessed or borrowed values).

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
`NOTICE.md`), except the `CC1101-ESP-Arduino` dependency pulled via PlatformIO, which is MIT
licensed by its own author.
