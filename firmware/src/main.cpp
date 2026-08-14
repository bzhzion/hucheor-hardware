// Hucheor beacon firmware skeleton (ESP32 + CC1101 @ 868.3 MHz OOK).
//
// This is written from scratch (clean room): it does not reuse any code from
// balises-ouistici/esp-arduino-nfs32002 (AGPLv3), only the same publicly
// documented radio parameters of the NF S32-002 standard (868.3 MHz, OOK).
//
// Status: radio init + raw edge capture only. The actual NF S32-002 frame
// matching is NOT implemented yet - it needs real timing data from our own
// RTL-SDR capture (see project roadmap) before it can be written correctly.
// Do not guess or borrow timing constants from other projects here.

#include <Arduino.h>
#include <CC1101_ESP_Arduino.h>

#include "audio_player.h"
#include "config_server.h"
#include "dcf77_clock.h"
#include "network.h"
#include "schedule.h"

// Wiring: to be confirmed once the first prototype is actually built.
static const int PIN_SPI_SCK = 18;
static const int PIN_SPI_MISO = 19;
static const int PIN_SPI_MOSI = 23;
static const int PIN_SPI_CS = 5;
static const int PIN_RADIO_GDO0 = 4; // CC1101 data output, read on edge interrupt

static const int PIN_I2S_BCK = 26;
static const int PIN_I2S_WS = 25;
static const int PIN_I2S_DATA = 27;

static const int PIN_DCF77 = 32; // optional: DCF77 receiver module output

CC1101 radio(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SPI_CS, PIN_RADIO_GDO0, PIN_RADIO_GDO0);

static const size_t EDGE_BUFFER_SIZE = 800;
volatile uint32_t edgeIntervalsUs[EDGE_BUFFER_SIZE];
volatile size_t edgeCount = 0;
volatile bool bufferReady = false;

// ISR: only record timings, never do Serial/logging or anything blocking here.
void IRAM_ATTR onRadioEdge() {
  static uint32_t lastEdgeUs = 0;
  uint32_t now = micros();
  uint32_t delta = now - lastEdgeUs;
  lastEdgeUs = now;

  if (edgeCount < EDGE_BUFFER_SIZE) {
    edgeIntervalsUs[edgeCount++] = delta;
  }
  if (edgeCount >= EDGE_BUFFER_SIZE) {
    bufferReady = true;
  }
}

// TODO(hucheor): implement once we have our own NF S32-002 capture.
// Must not assume a single hardcoded timing table like Ouistici's prototype -
// derive the real preamble/bit encoding from our own RTL-SDR recording first.
bool matchesNfS32002Frame(const volatile uint32_t *intervalsUs, size_t count) {
  (void)intervalsUs;
  (void)count;
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // Deliberately no setenv("TZ", ...) here: every time source (DCF77,
  // phone-clock sync, NTP once Network is in station mode) hands
  // Schedule::setCurrentTime() already-correct French local wall-clock
  // time, encoded as if it were UTC. Schedule reads it back with gmtime_r(),
  // never localtime_r() - no CEST/CET rule is hardcoded in this firmware.
  // See schedule.h for the full reasoning (a hardcoded EU DST rule would go
  // silently wrong if that legislation ever changes without a matching
  // firmware update).

  radio.init();
  radio.setMHZ(868.3);
  radio.setModulation(ASK_OOK);
  radio.setRx();

  attachInterrupt(digitalPinToInterrupt(PIN_RADIO_GDO0), onRadioEdge, CHANGE);

  if (!AudioPlayer::begin(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DATA)) {
    Serial.println("Hucheor: audio player init failed");
  }

  Schedule::begin();
  Network::begin(); // decides standalone AP vs joining the shop's WiFi, see network.h
  ConfigServer::begin();
  Dcf77Clock::begin(PIN_DCF77);

  Serial.println("Hucheor: listening on 868.3 MHz (no frame decoding yet)");
}

void loop() {
  Network::poll(); // non-blocking: station reconnect + periodic NTP resync, see network.h

  if (Dcf77Clock::poll()) {
    Serial.println("Hucheor: clock synced from DCF77");
  }

  if (bufferReady) {
    noInterrupts();
    bool matched = matchesNfS32002Frame(edgeIntervalsUs, edgeCount);
    edgeCount = 0;
    bufferReady = false;
    interrupts();

    if (matched) {
      Serial.println("Remote detected");
      const char *path = Schedule::isOpenNow() ? "/message_open.wav" : "/message_closed.wav";
      if (!AudioPlayer::playWavFile(path)) {
        // Shopkeeper only uploaded one generic message (or the schedule was
        // never configured): fall back to it rather than staying silent.
        AudioPlayer::playWavFile("/message.wav");
      }
    }
  }
}
