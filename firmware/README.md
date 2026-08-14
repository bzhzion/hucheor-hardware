# Firmware

Firmware ESP32 pour la balise Hucheor, construit avec [PlatformIO](https://platformio.org/). Voir
le [README à la racine du dépôt](../README.md) pour le parcours complet (installation, posture
sécurité, CI), ce fichier reste un résumé court par module.

## État

- Radio : initialisation et capture brute des fronts uniquement. Le décodage de trame NF S32-002
  n'est **pas implémenté** pour l'instant : voir `src/main.cpp` pour le détail du pourquoi (ça
  nécessite des données de timing réelles issues de notre propre capture RTL-SDR, pas des valeurs
  devinées ou empruntées).
- Audio (`audio_player.h`/`.cpp`) : joue un fichier WAV PCM 16 bits mono/stéréo depuis LittleFS via
  I2S. Uniquement le driver I2S natif de l'ESP32, aucune bibliothèque audio tierce (le choix
  évident, ESP8266Audio, est en GPLv3 : incompatible avec la licence CC BY-NC 4.0 de ce dépôt).
- Réseau (`network.h`/`.cpp`) : deux modes de connexion. Autonome (par défaut) crée son propre
  point d'accès WiFi (`Hucheor-XXXX`) ; station rejoint plutôt le WiFi propre du commerce, avec
  repli automatique sur le mode autonome en cas d'échec. Le mode station s'annonce via mDNS
  (`hucheor-xxxx.local`) pour qu'une future application compagnon puisse le découvrir, et
  synchronise l'heure via NTP puisqu'il a accès à internet.
- Serveur de configuration (`config_server.h`/`.cpp`) : pages HTML accessibles (de vrais
  `<label>`, contours de focus visibles, seul JS de la page : un petit script de synchronisation
  d'horloge - le seuil WCAG 2.2 AA est requis sur tout le projet) pour envoyer les messages
  ouvert/fermé séparément, modifier les horaires d'ouverture, et changer de mode WiFi.
- Planification (`schedule.h`/`.cpp`) : jusqu'à 4 modèles hebdomadaires saisonniers (ex.
  "Standard", "Été"), et jusqu'à 12 plages de semaines ISO (1-53) assignant quel modèle s'applique
  quand.
- Horloge : trois sources indépendantes, aucune ne nécessite de règle de changement d'heure codée
  en dur dans ce firmware (voir le commentaire en tête de `schedule.h`) - l'horloge du téléphone
  (synchronisée automatiquement à chaque fois que le commerçant ouvre la page de configuration), un
  **récepteur grandes ondes DCF77** (`dcf77_clock.h`/`.cpp`, une seule broche GPIO, écrit depuis
  zéro à partir de la spécification publique du télégramme DCF77, sur le même principe que le
  travail NF S32-002 propre à ce projet), et NTP (disponible uniquement en mode station).
- Pas encore câblé : le déclenchement décodage de trame -> lecture audio existe dans `main.cpp`
  mais appelle un décodeur qui renvoie toujours `false` pour l'instant.

## Compilation

```
cd firmware
pio run
```

## Matériel (prévu)

- MCU : ESP32 (S3 ou C3 de préférence)
- Radio : CC1101 (SPI), 868,3 MHz OOK
- Audio : MAX98357A (I2S) + haut-parleur
- Horloge : module récepteur DCF77 (~5-10 EUR, une broche GPIO), optionnel mais recommandé
- Câblage : pas encore finalisé, voir les commentaires de broches dans `src/main.cpp` ; sera mis à
  jour une fois le premier prototype réellement câblé et testé.

## Licence

Le code de ce dossier est sous licence CC BY-NC 4.0 (voir `LICENSE.md`/`NOTICE.md` à la racine du
dépôt). Les dépendances tierces récupérées via PlatformIO gardent leur propre licence :
- `CC1101-ESP-Arduino` : MIT
- `ESPAsyncWebServer` / `AsyncTCP` : LGPL-3.0 (copyleft faible, sûr en dépendance : contrairement à
  GPL/AGPL, LGPL autorise explicitement la liaison depuis du code sous une licence différente)
- LittleFS : fourni avec le core Arduino ESP32 lui-même, pas une dépendance séparée

Écarté : `ESP8266Audio` (GPLv3 : forcerait tout ce firmware sous GPL, incompatible avec CC BY-NC
4.0).
