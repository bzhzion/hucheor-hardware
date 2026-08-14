#include "schedule.h"

#include <Preferences.h>
#include <sys/time.h>
#include <time.h>

namespace {

Preferences prefs;
const char *NAMESPACE = "hucheor-sched";

String dayKey(int model, int weekday, const char *field) {
  return String("m") + model + "_d" + weekday + "_" + field;
}

String rangeKey(int index, const char *field) {
  return String("r") + index + "_" + field;
}

// ISO 8601 week number (1-53) for the device's current local time. newlib's
// strftime supports %V on ESP32 (part of the standard C library, not
// something we implement ourselves).
int currentIsoWeek() {
  time_t now = time(nullptr);
  struct tm localNow;
  localtime_r(&now, &localNow);
  char buf[4];
  strftime(buf, sizeof(buf), "%V", &localNow);
  return atoi(buf);
}

} // namespace

namespace Schedule {

void begin() {
  // Nothing to precompute: Preferences is opened per call (rare - only when
  // the shopkeeper edits hours, or once per remote press), not worth
  // keeping it open permanently and risking a stale handle across reboots.
}

String modelName(int model) {
  if (model < 0 || model >= MAX_MODELS) return "";
  prefs.begin(NAMESPACE, true);
  String name = prefs.getString((String("mname") + model).c_str(), "");
  prefs.end();
  if (name.length() == 0) name = String("Modele ") + (model + 1);
  return name;
}

void setModelName(int model, const String &name) {
  if (model < 0 || model >= MAX_MODELS) return;
  prefs.begin(NAMESPACE, false);
  prefs.putString((String("mname") + model).c_str(), name);
  prefs.end();
}

DaySchedule get(int model, int weekday) {
  DaySchedule day;
  if (model < 0 || model >= MAX_MODELS || weekday < 0 || weekday > 6) return day;

  prefs.begin(NAMESPACE, true);
  day.enabled = prefs.getBool(dayKey(model, weekday, "en").c_str(), false);
  day.openMinute = prefs.getUShort(dayKey(model, weekday, "open").c_str(), day.openMinute);
  day.closeMinute = prefs.getUShort(dayKey(model, weekday, "close").c_str(), day.closeMinute);
  prefs.end();
  return day;
}

void set(int model, int weekday, const DaySchedule &schedule) {
  if (model < 0 || model >= MAX_MODELS || weekday < 0 || weekday > 6) return;

  prefs.begin(NAMESPACE, false);
  prefs.putBool(dayKey(model, weekday, "en").c_str(), schedule.enabled);
  prefs.putUShort(dayKey(model, weekday, "open").c_str(), schedule.openMinute);
  prefs.putUShort(dayKey(model, weekday, "close").c_str(), schedule.closeMinute);
  prefs.end();
}

WeekRange getRange(int index) {
  WeekRange range;
  if (index < 0 || index >= MAX_RANGES) return range;

  prefs.begin(NAMESPACE, true);
  range.startWeek = prefs.getUChar(rangeKey(index, "start").c_str(), 0);
  range.endWeek = prefs.getUChar(rangeKey(index, "end").c_str(), 0);
  range.model = prefs.getUChar(rangeKey(index, "model").c_str(), 0);
  prefs.end();
  return range;
}

void setRange(int index, const WeekRange &range) {
  if (index < 0 || index >= MAX_RANGES) return;

  prefs.begin(NAMESPACE, false);
  prefs.putUChar(rangeKey(index, "start").c_str(), range.startWeek);
  prefs.putUChar(rangeKey(index, "end").c_str(), range.endWeek);
  prefs.putUChar(rangeKey(index, "model").c_str(), range.model);
  prefs.end();
}

int modelForWeek(int isoWeek) {
  for (int i = 0; i < MAX_RANGES; i++) {
    WeekRange range = getRange(i);
    if (range.startWeek == 0) continue; // unused slot
    if (isoWeek >= range.startWeek && isoWeek <= range.endWeek) {
      return range.model < MAX_MODELS ? range.model : 0;
    }
  }
  return 0; // no matching range: default model
}

bool isOpenNow() {
  time_t now = time(nullptr);
  struct tm localNow;
  localtime_r(&now, &localNow);

  int model = modelForWeek(currentIsoWeek());
  DaySchedule today = get(model, localNow.tm_wday);
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
