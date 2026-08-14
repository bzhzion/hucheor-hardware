# Hucheor Hardware

Open hardware and firmware for **Hucheor**, a sound beacon for local shops that responds to the
standard French remote control used by blind and visually impaired people (NF S32-002, 868.3 MHz).

This repository holds everything meant to be freely shared and self-buildable:

- KiCad schematics and PCB Gerber files
- ESP32 firmware (radio decoding, audio playback, WiFi configuration)
- 3D-printable enclosure files (STL)

The overall project (business context, deployment, website) lives in a separate private
repository. This one is the public, open part: anyone should be able to fetch these files, build
their own Hucheor beacon, and improve on it.

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
