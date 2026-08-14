#pragma once

#include <Arduino.h>

// Per-weekday opening hours, so the beacon can announce "open" or "closed"
// depending on when the remote is pressed. Stored in NVS (Preferences), one
// open/close range per day (a shop that closes for lunch is a v2 problem,
// not v1).
//
// Time source: this firmware does not assume internet access (the device's
// only WiFi role so far is its own config access point, see
// config_server.h), so it relies on the ESP32's internal system clock,
// which the shopkeeper sets once from their phone's browser clock via the
// config page (see ConfigServer). It will drift over days without a real
// RTC/NTP source - acceptable for a v1 MVP, revisit once the hardware
// roadmap (v2 Auracast co-processor, see docs) settles whether WiFi
// station+NTP is worth adding.

namespace Schedule {

struct DaySchedule {
  bool enabled = false;
  uint16_t openMinute = 9 * 60;   // minutes since midnight
  uint16_t closeMinute = 18 * 60;
};

void begin();

// index: 0 = Sunday ... 6 = Saturday (matches struct tm::tm_wday).
DaySchedule get(int weekday);
void set(int weekday, const DaySchedule &schedule);

// Uses the ESP32's system clock (see setCurrentTime()).
bool isOpenNow();

// Sets the device's system clock. epochSeconds: seconds since 1970-01-01
// UTC, as sent by the shopkeeper's browser (Date.now() / 1000) when they
// configure the beacon - the device has no other way to know the time.
void setCurrentTime(time_t epochSeconds);

} // namespace Schedule
