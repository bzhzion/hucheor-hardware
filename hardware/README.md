# Matériel

Les schémas KiCad et fichiers Gerber du circuit imprimé de la balise Hucheor viendront ici.

Le premier prototype physique est encore en cours de câblage sur plaque d'essai (ESP32 + CC1101 +
MAX98357A + DCF77). Le PCB final viendra une fois ce prototype validé.

[`WIRING.md`](WIRING.md) documente le câblage précis à suivre (nomenclature, architecture
d'alimentation, brochage broche par broche) - identique à ce qu'utilise le firmware
(`firmware/src/main.cpp`), pour que la plaque d'essai et le schéma KiCad partent de la même base
sans rien deviner.

## `hucheor.kicad_sch` : premier jet, à vérifier avant usage

**Ce schéma a été généré par script (`generate_schematic.py`, bibliothèque Python `kiutils`) sans
KiCad installé sur la machine qui l'a produit.** Le format `.kicad_sch` est fait pour être édité
et vérifié visuellement dans l'éditeur de schémas de KiCad (Eeschema) : ce fichier n'a **jamais été
ouvert dans KiCad, ni passé à l'ERC**. Il a seulement été relu avec succès par la même bibliothèque
qui l'a écrit (`kiutils`), ce qui garantit que la syntaxe est valide, pas que le schéma est
électriquement correct ou lisible visuellement.

Avant de s'en servir pour quoi que ce soit de réel :
1. Ouvrir `hucheor.kicad_sch` dans KiCad (Eeschema peut ouvrir un fichier `.kicad_sch` seul, sans
   projet `.kicad_pro`).
2. Vérifier visuellement le placement des symboles et les libellés de net (`+5V`, `GND`, `+3V3`,
   `GPIO18_SCK`, etc. - la correspondance exacte est dans `WIRING.md`).
3. Lancer l'ERC (Vérification électrique) et corriger ce qu'il signale.
4. Les 4 modules (ESP32, CC1101, MAX98357A, DCF77) et le haut-parleur/connecteur d'alimentation
   utilisent des symboles génériques créés pour l'occasion (pas de symbole officiel KiCad pour ces
   breakouts hobbyistes) - pas d'empreinte (footprint) assignée, normal à ce stade, à faire une
   fois les breakouts précis choisis.

Pour régénérer le fichier après une modification de `WIRING.md` :
```
cd hardware
python -m venv .kicad-venv        # une fois
.kicad-venv/Scripts/pip install kiutils
.kicad-venv/Scripts/python generate_schematic.py
```

Sous licence BREIZHZION Personal Use License (BZ-1.1), voir `LICENSE.md`/`NOTICE.md` à la racine
du dépôt.
