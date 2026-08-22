# Matériel

Les schémas KiCad et fichiers Gerber du circuit imprimé de la balise Hucheor viendront ici.

Rien de publié en KiCad pour l'instant : le premier prototype est encore en cours de câblage sur
plaque d'essai (ESP32 + CC1101 + MAX98357A + DCF77). La conception du PCB viendra une fois ce
prototype validé.

En attendant, [`WIRING.md`](WIRING.md) documente le câblage précis à suivre (nomenclature,
architecture d'alimentation, brochage broche par broche) - identique à ce qu'utilise le firmware
(`firmware/src/main.cpp`), pour que la plaque d'essai et le futur schéma KiCad partent de la même
base sans rien deviner.

Sous licence BREIZHZION Personal Use License (BZ-1.1), voir `LICENSE.md`/`NOTICE.md` à la racine
du dépôt.
