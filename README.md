# Hucheor Hardware

Hardware and firmware for **Hucheor**, a sound beacon for local shops that responds to the
standard French remote control used by blind and visually impaired people (NF S32-002, 868.3 MHz).

"Hucheor" is Old French for "one who calls out, who guides".

This repository holds everything meant to be freely shared and self-buildable:

- `firmware/` — ESP32 firmware (radio decoding, audio playback, WiFi configuration)
- `hardware/` — KiCad schematics and PCB Gerber files
- `enclosure/` — 3D-printable enclosure files (STL)

The overall project (business context, deployment, website) lives in a separate private
repository. This one is the public, source-available part: anyone can fetch these files, build
their own Hucheor beacon for personal or internal use, and modify it. See
[NOTICE.md](NOTICE.md) for exactly what that does and doesn't allow — this is **not** an OSI-style
open source license, commercial resale is deliberately excluded.

## Roadmap

- **v1 (MVP, in progress)**: NF S32-002 (868.3 MHz) reception + loudspeaker audio playback.
- **v2 (Bluetooth LE Audio / Auracast)**: broadcast the message directly to compatible hearing
  aids and earbuds, alongside the loudspeaker. No source-available beacon combines 868.3 MHz
  reception with Auracast today. Not an ESP32-only feature: full Auracast (LC3 codec, Broadcast
  Isochronous Streams, certified interoperability) needs a dedicated co-processor, likely a Nordic
  nRF5340 (candidate module: Aurawave AW100), talking to the ESP32 over UART/SPI.
- **v3 (companion mobile app)**: Flutter (iOS + Android), BLE beacon triggering, Auracast
  reception, directory of equipped shops, GPS navigation mode. Stays compatible with the existing
  physical NF S32-002 remote (the app complements it, never replaces it).
- **v4 (ecosystem)**: shop-owner management interface, OpenStreetMap-based beacon map, directory
  integration.

Priority stays on finishing v1 before starting v2.

## Accessibility

This project exists for blind and visually impaired people. Any interface built here (firmware
web config UI, future companion app) must target **WCAG 2.2 AA at minimum** from the first draft:
strong contrast, generous font sizes, no text conveyed only through images, full keyboard
navigation, correct ARIA attributes, respect for reduced-motion preferences. This is not optional
on this project.

## Status

Early stage, nothing built yet.

- Radio/firmware: `firmware/` has an init-and-listen skeleton (ESP32 + CC1101 @ 868.3 MHz OOK),
  written from scratch. Frame decoding is deliberately left unimplemented pending a real RTL-SDR
  capture of the NF S32-002 signal — no hardcoded timing table borrowed from anywhere else.
- Hardware/enclosure: not started, waiting on a validated firmware prototype first.

## Approach

This project does not reuse code from [Ouistici](https://balises-ouistici.org/)'s
`esp-arduino-nfs32002` (AGPLv3): that repo's reusable library doesn't actually compile (several
unrelated bugs — mismatched member variables, a radio object that goes out of scope, an
interrupt attached to a non-static class method, duplicate declarations), and reusing AGPL code
would force this whole repository under AGPLv3, which conflicts with the license below. Hucheor's
firmware is written independently, using only the same publicly documented radio parameters of
the NF S32-002 standard (868.3 MHz, OOK) and Ouistici's general approach as inspiration, not their
code.

## License

**CC BY-NC 4.0** (Attribution-NonCommercial). See [NOTICE.md](NOTICE.md) for what this means in
practice, and [LICENSE.md](LICENSE.md) for the full legal text. In short: build one for yourself,
for a friend for free, or for your own organization's internal use, and modify/share freely — but
no one besides BREIZHZION may sell, charge for, or otherwise commercially provide a Hucheor
beacon or a device built from these files, including nonprofit associations.

## Credits

Inspired by prior work by [Ouistici](https://balises-ouistici.org/) (Les Petites Débrouillardes
Auvergne-Rhône-Alpes), dormant since August 2024 — see "Approach" above for why their code isn't
reused directly.
