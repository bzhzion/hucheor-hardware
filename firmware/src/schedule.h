#pragma once

#include <Arduino.h>

// Seasonal opening hours: a shop can have several distinct weekly patterns
// ("models" - e.g. "Standard", "Ete", "Vacances de Noel") and assign each
// ISO week of the year (1-53) to one of them. Falls back to model 0 if a
// week has no explicit assignment.
//
// Time source: this firmware does not assume internet access (the device's
// only WiFi role so far is its own config access point, see
// config_server.h). Two ways to get real time, both optional and
// independent of each other:
// - The shopkeeper's phone clock, auto-synced whenever they open the config
//   page (see ConfigServer's inline script + setCurrentTime()).
// - A DCF77 longwave radio receiver module (see dcf77_clock.h), which syncs
//   automatically and continuously without any phone/WiFi involvement -
//   preferred once wired, since it can't go stale between shopkeeper visits.
// Without either, the ESP32's internal clock drifts freely from whatever it
// was last set to (or 1970-01-01 if never set) - acceptable degraded mode
// for a v1 MVP, not a silent wrong-answer: DaySchedule::enabled defaults to
// false, so an un-synced clock reads as "closed" rather than guessing.

namespace Schedule {

const int MAX_MODELS = 4;
const int MAX_RANGES = 12;

struct DaySchedule {
  bool enabled = false;
  uint16_t openMinute = 9 * 60;   // minutes since midnight
  uint16_t closeMinute = 18 * 60;
};

struct WeekRange {
  uint8_t startWeek = 0; // 1-53, 0 = unused slot
  uint8_t endWeek = 0;
  uint8_t model = 0;
};

void begin();

String modelName(int model);
void setModelName(int model, const String &name);

// weekday: 0 = Sunday ... 6 = Saturday (matches struct tm::tm_wday).
DaySchedule get(int model, int weekday);
void set(int model, int weekday, const DaySchedule &schedule);

WeekRange getRange(int index);
void setRange(int index, const WeekRange &range);

// Which model applies to a given ISO week number (1-53). Returns 0 (first
// model) if no configured range covers that week.
int modelForWeek(int isoWeek);

// Uses the ESP32's system clock (see setCurrentTime()).
bool isOpenNow();

// Sets the device's system clock. epochSeconds: seconds since 1970-01-01
// UTC. Called either from the config page (phone's clock) or from the DCF77
// receiver once it has decoded a valid telegram.
void setCurrentTime(time_t epochSeconds);

} // namespace Schedule
