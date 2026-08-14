#include "schedule.h"

#include <Preferences.h>
#include <sys/time.h>

namespace {

Preferences prefs;
const char *NAMESPACE = "hucheor-sched";

// One Preferences key per field per day, e.g. "d3_en", "d3_open", "d3_close".
String keyFor(int weekday, const char *field) {
  return String("d") + weekday + "_" + field;
}

} // namespace

namespace Schedule {

void begin() {
  // Nothing to precompute: Preferences is opened per call (rare - only when
  // the shopkeeper edits hours, or once at boot per day check), not worth
  // keeping it open permanently and risking a stale handle across reboots.
}

DaySchedule get(int weekday) {
  DaySchedule day;
  if (weekday < 0 || weekday > 6) return day;

  prefs.begin(NAMESPACE, true);
  day.enabled = prefs.getBool(keyFor(weekday, "en").c_str(), false);
  day.openMinute = prefs.getUShort(keyFor(weekday, "open").c_str(), day.openMinute);
  day.closeMinute = prefs.getUShort(keyFor(weekday, "close").c_str(), day.closeMinute);
  prefs.end();
  return day;
}

void set(int weekday, const DaySchedule &schedule) {
  if (weekday < 0 || weekday > 6) return;

  prefs.begin(NAMESPACE, false);
  prefs.putBool(keyFor(weekday, "en").c_str(), schedule.enabled);
  prefs.putUShort(keyFor(weekday, "open").c_str(), schedule.openMinute);
  prefs.putUShort(keyFor(weekday, "close").c_str(), schedule.closeMinute);
  prefs.end();
}

bool isOpenNow() {
  time_t now = time(nullptr);
  struct tm localNow;
  localtime_r(&now, &localNow);

  DaySchedule today = get(localNow.tm_wday);
  if (!today.enabled) return false;

  uint16_t nowMinutes = localNow.tm_hour * 60 + localNow.tm_min;
  if (today.closeMinute > today.openMinute) {
    return nowMinutes >= today.openMinute && nowMinutes < today.closeMinute;
  }
  // Overnight range (closes after midnight, e.g. a bar open 20:00-02:00).
  return nowMinutes >= today.openMinute || nowMinutes < today.closeMinute;
}

void setCurrentTime(time_t epochSeconds) {
  struct timeval tv = {.tv_sec = epochSeconds, .tv_usec = 0};
  settimeofday(&tv, nullptr);
}

} // namespace Schedule
