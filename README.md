# Hucheor Hardware

Open hardware and firmware for **Hucheor**, a sound beacon for local shops that responds to the
standard French remote control used by blind and visually impaired people (NF S32-002, 868.3 MHz).

"Hucheor" is Old French for "one who calls out, who guides".

This repository holds everything meant to be freely shared and self-buildable:

- KiCad schematics and PCB Gerber files
- ESP32 firmware (radio decoding, audio playback, WiFi configuration)
- 3D-printable enclosure files (STL)

The overall project (business context, deployment, website) lives in a separate private
repository. This one is the public, open part: anyone should be able to fetch these files, build
their own Hucheor beacon, and improve on it.

## Roadmap

- **v1 (MVP, in progress)**: NF S32-002 (868.3 MHz) reception + loudspeaker audio playback.
- **v2 (Bluetooth LE Audio / Auracast)**: broadcast the message directly to compatible hearing
  aids and earbuds, alongside the loudspeaker. No open source beacon combines 868.3 MHz reception
  with Auracast today. Not an ESP32-only feature: full Auracast (LC3 codec, Broadcast Isochronous
  Streams, certified interoperability) needs a dedicated co-processor, likely a Nordic nRF5340
  (candidate module: Aurawave AW100), talking to the ESP32 over UART/SPI.
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

Early stage, nothing published yet. First milestones (see the admin project brief):

- Fork and finish `balises-ouistici/esp-arduino-nfs32002` (radio frame decoding)
- Validate the NF S32-002 signal with a real RTL-SDR capture
- Wire a first ESP32 + CC1101 + MAX98357A prototype

## License

Not chosen yet. Leaning towards a share-alike hardware license that allows self-build but
restricts commercial resale by third parties (e.g. CERN OHL-S).

## Credits

Builds on prior work by [Ouistici](https://balises-ouistici.org/) (Les Petites Débrouillardes
Auvergne-Rhône-Alpes), dormant since August 2024.
