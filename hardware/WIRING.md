# Câblage de référence (avant schéma KiCad)

Ce document liste précisément quoi brancher où, pour que le premier prototype sur plaque d'essai
et le futur schéma KiCad partent de la même base, sans rien deviner à l'un ou l'autre bout. Les
numéros de broche ESP32 correspondent exactement à ceux déjà utilisés dans le firmware
(`firmware/src/main.cpp`), pas de valeur inventée ici.

## Nomenclature (BOM)

| Réf. | Composant | Rôle | Alimentation | Remarque |
|---|---|---|---|---|
| U1 | ESP32-WROOM-32 DevKit (30 ou 38 broches) | MCU | 5V via USB/VIN, régule en 3,3V en interne | N'importe quel clone générique "ESP32 DevKitC" convient |
| U2 | Module CC1101 868 MHz (breakout SPI) | Réception radio NF S32-002 | 3,3V (broche 3V3 de U1) | Antenne à ressort souvent déjà soudée sur le module |
| U3 | Module ampli I2S MAX98357A (breakout) | Amplification audio | 5V (rail USB, pas via U1) | Sortie jusqu'à 3W sous 4Ω |
| U4 | Module récepteur DCF77 77,5 kHz | Synchro horaire autonome | 3,3V (broche 3V3 de U1) | Optionnel mais recommandé |
| SP1 | Haut-parleur 4Ω, 3-5W | Diffusion du message | - | Connecteur JST-PH 2 broches conseillé |
| J1 | Connecteur USB-C ou Micro-USB (souvent déjà sur U1) | Alimentation 5V | - | Alimentation secteur 5V/1A minimum une fois en devanture |

## Architecture d'alimentation

Tout part du rail **5V** (USB) :
- U1 (ESP32) régule ce 5V en 3,3V en interne via son propre régulateur embarqué, et expose cette
  3,3V régulée sur sa broche `3V3`.
- U2 (CC1101) et U4 (DCF77) sont alimentés depuis cette broche `3V3` de U1 : leur consommation est
  faible (quelques dizaines de mA), largement dans la marge du régulateur embarqué d'un DevKit
  standard.
- U3 (MAX98357A) est alimenté **directement depuis le rail 5V**, pas depuis la 3,3V de U1 : la
  puissance de sortie audio (jusqu'à 3W) dépend directement de la tension d'alimentation de
  l'ampli, le faire passer par le régulateur 3,3V de l'ESP32 le priverait de la moitié de sa
  puissance et solliciterait inutilement ce régulateur.

Ne jamais alimenter U3 depuis la broche `3V3` de U1 pour cette raison.

## Brochage détaillé

### U2 : module CC1101 (868 MHz)

| Broche module CC1101 | Connecté à (U1 ESP32) | Signal |
|---|---|---|
| VCC | 3V3 | Alimentation 3,3V |
| GND | GND | Masse |
| SCK | GPIO18 | Horloge SPI (VSPI) |
| MISO | GPIO19 | Données SPI entrantes |
| MOSI | GPIO23 | Données SPI sortantes |
| CSN | GPIO5 | Chip select SPI |
| GDO0 | GPIO4 | Sortie données radio, lue par interruption |
| GDO2 | non connecté | Non utilisé par ce firmware |

GPIO18/19/23/5 correspondent au bus SPI matériel par défaut de l'ESP32 (VSPI) : aucun autre
périphérique SPI n'est utilisé, pas de partage de bus à gérer.

### U3 : module ampli I2S MAX98357A

| Broche module MAX98357A | Connecté à | Signal |
|---|---|---|
| VIN | Rail 5V (pas U1/3V3, voir ci-dessus) | Alimentation |
| GND | GND | Masse |
| BCLK | GPIO26 | Horloge de bit I2S |
| LRC | GPIO25 | Word select (gauche/droite) I2S |
| DIN | GPIO27 | Données audio I2S |
| SD | Non connecté | Laissé flottant = ampli actif, gain par défaut (tiré en interne par le module) |
| GAIN | Non connecté | Gain par défaut (9 dB), ajustable plus tard si besoin (voir datasheet MAX98357A) |
| + / - (sortie HP) | SP1 (haut-parleur) | Sortie audio différentielle vers le haut-parleur |

### U4 : module récepteur DCF77

| Broche module DCF77 | Connecté à (U1 ESP32) | Signal |
|---|---|---|
| VCC | 3V3 | Alimentation 3,3V |
| GND | GND | Masse |
| OUT (parfois nommée "T" ou "DATA") | GPIO32 | Signal démodulé, lu par interruption sur front |
| PON (si présente) | Non connectée | Laissée flottante = module toujours actif sur la plupart des breakouts courants |

Le brochage exact varie légèrement selon le fabricant du module DCF77 (souvent 3 broches
VCC/GND/OUT, parfois une 4e broche d'activation) : vérifier la sérigraphie du module reçu avant de
câbler, certains inversent VCC/GND par rapport à d'autres.

## Antennes

- **CC1101** : antenne à ressort (souvent déjà soudée sur le module) suffit pour la portée visée
  (devanture de commerce, quelques mètres). Pas besoin d'antenne externe/déportée pour le
  prototype.
- **DCF77** : antenne ferrite généralement déjà intégrée au module, orientation sensible (à tester
  sur place, la réception grandes ondes est directionnelle).

## Notes pour le passage au schéma KiCad

- Utiliser les symboles génériques "ESP32-WROOM-32" et "MAX98357A" déjà présents dans les
  bibliothèques KiCad standard (`RF_Module`, `Amplifier_Audio`) plutôt qu'un symbole de DevKit
  complet, si le PCB final intègre le module ESP32 directement plutôt qu'un DevKit entier.
- Prévoir un connecteur JST-PH 2 broches pour SP1 et pour l'alimentation 5V (J1), plus robuste
  qu'un simple bornier à vis pour un appareil qui sera manipulé/installé.
- Découpler chaque alimentation (100nF + 10µF en parallèle, au plus près de chaque module) : pas
  fait sur les breakouts du commerce, à ajouter explicitement sur le PCB final.
