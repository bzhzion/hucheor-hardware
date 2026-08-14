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

// ISO 8601 week number (1-53) for the device's current local time.
//
// Deliberately gmtime_r(), not localtime_r(): this module never asks the C
// library to do any timezone/DST math itself. setCurrentTime() is always
// given the *French local wall-clock time*, just encoded as if it were UTC
// (both DCF77 and the phone-clock sync produce that format - see their own
// comments for why). Reading it back with gmtime_r() hands the same fields
// straight back out, with zero DST logic anywhere in this firmware to keep
// in sync with EU legislation - each time source is independently
// responsible for knowing whether CEST or CET currently applies (DCF77
// broadcasts it live, the phone's OS has its own kept-up-to-date timezone
// database). If this ever used localtime_r() with a hardcoded TZ string
// instead, a future change to the EU's DST rules (there have been
// discussions of abolishing the switch entirely) would silently produce
// wrong opening hours until the firmware itself was updated and reflashed.
int currentIsoWeek() {
  time_t now = time(nullptr);
  struct tm localNow;
  gmtime_r(&now, &localNow);
  char buf[4];
  strftime(buf, sizeof(buf), "%V", &localNow);
  return atoi(buf);
}

// Right after a cold boot or a power-loss restart, and before any time
// source (DCF77, phone-clock sync, NTP) has synced, the system clock reads
// close to epoch 0 (1970-01-01, a Thursday) - not zero, not obviously wrong
// to a naive check. If that weekday happens to be enabled in the active
// model, isOpenNow() would confidently announce an arbitrary open/closed
// status instead of failing safe. Same threshold as network.cpp's
// SANE_EPOCH_THRESHOLD (kept separate on purpose: this module must not
// depend on Network to know whether its own clock looks plausible).
const time_t SANE_EPOCH_THRESHOLD = 8L * 365 * 24 * 3600;

bool clockLooksSane() { return time(nullptr) > SANE_EPOCH_THRESHOLD; }

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
  if (name.length() == 0) name = String("Mod&egrave;le ") + (model + 1);
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
  // Fail safe rather than guess on a garbage post-boot clock (see
  // clockLooksSane() above) - "closed" is the safer wrong answer for a
  // blind/low-vision visitor than confidently announcing "open".
  if (!clockLooksSane()) return false;

  time_t now = time(nullptr);
  struct tm localNow;
  gmtime_r(&now, &localNow);

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
  // epochSeconds must already be French local wall-clock time, encoded as
  // if it were UTC (i.e. what you'd get from timegm() on the local
  // year/month/day/hour/minute fields) - see the comment on currentIsoWeek()
  // for why this module works this way instead of applying a timezone.
  struct timeval tv = {.tv_sec = epochSeconds, .tv_usec = 0};
  settimeofday(&tv, nullptr);
}

} // namespace Schedule
