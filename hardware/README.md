# Matériel

Les schémas KiCad et fichiers Gerber du circuit imprimé de la balise Hucheor viendront ici.

Le premier prototype physique est encore en cours de câblage sur plaque d'essai (ESP32 + CC1101 +
MAX98357A + DCF77). Le PCB final viendra une fois ce prototype validé.

[`WIRING.md`](WIRING.md) documente le câblage précis à suivre (nomenclature, architecture
d'alimentation, brochage broche par broche) - identique à ce qu'utilise le firmware
(`firmware/src/main.cpp`), pour que la plaque d'essai et le schéma KiCad partent de la même base
sans rien deviner.

## `hucheor.kicad_sch` : généré par script, ERC vérifiée

Ce schéma est généré par `generate_schematic.py` (bibliothèque Python `kiutils`) à partir du
câblage documenté dans `WIRING.md`. KiCad 10 est maintenant installé sur la machine qui l'a produit
: le fichier a été **rechargé avec succès et passé à l'ERC via `kicad-cli sch erc`**, pas seulement
relu par la bibliothèque qui l'a écrit.

**22 avertissements ERC restants, tous attendus** (voir la sortie de `kicad-cli sch erc` pour le
détail) :
- `power_pin_not_driven` / `ground_pin_not_ground` : les connexions passent par des labels de net
  (`+5V`, `GND`, `+3V3`) plutôt que par de vrais symboles de flag d'alimentation KiCad (`power:GND`,
  `power:+3V3`...) - à ajouter dans l'éditeur si on veut faire taire ces avertissements, purement
  cosmétique.
- `lib_symbol_issues` (bibliothèque vide `''`) : les 6 symboles (ESP32, CC1101, MAX98357A, DCF77,
  haut-parleur, connecteur d'alimentation) sont générés sur mesure (aucun symbole officiel KiCad
  pour ces breakouts hobbyistes) et ne sont pas encore enregistrés dans une table de bibliothèque
  de projet (`sym-lib-table`) - sans conséquence pour la lecture/l'ERC, à faire si on veut les
  réutiliser proprement dans d'autres projets KiCad.
- `endpoint_off_grid` : imprécision flottante mineure sur les coordonnées de broches
  (ex. `15.239999999999998` au lieu de `15.24`), cosmétique.

Aucune empreinte (footprint) assignée pour l'instant, normal à ce stade - à faire une fois les
breakouts précis choisis.

**Pour vérifier ou régénérer** :
```
# Verifier (KiCad installe)
kicad-cli sch erc --severity-all hardware/hucheor.kicad_sch

# Regenerer apres une modification de WIRING.md
cd hardware
python -m venv .kicad-venv        # une fois
.kicad-venv/Scripts/pip install kiutils
.kicad-venv/Scripts/python generate_schematic.py
```

Historique de mise au point (utile si `generate_schematic.py` est modifié un jour) : la première
version chargeait dans `kiutils` mais **échouait au chargement dans KiCad 10** avec un message
générique `Failed to load schematic`, sans plus de détail. Cause trouvée par bissection contre un
vrai fichier KiCad round-trippé par `kiutils` (qui, lui, se rechargeait sans erreur) : les broches
doivent être portées par des **sous-unités imbriquées** du symbole (unité 0 = dessin, unité 1 =
broches - convention utilisée par tous les symboles KiCad officiels, y compris les plus simples
comme `Device:R`), pas directement sur le symbole parent ; et la position de chaque propriété
(`Reference`, `Value`...) doit avoir un angle explicite (`0` si non tourné), sinon KiCad 10 refuse
de charger l'instance placée alors même que `kiutils` l'écrit et la relit sans broncher.

Sous licence BREIZHZION Personal Use License (BZ-1.1), voir `LICENSE.md`/`NOTICE.md` à la racine
du dépôt.
