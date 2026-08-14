#include "dcf77_clock.h"

#include <time.h>

#include "schedule.h"

namespace {

// This toolchain's newlib doesn't provide timegm() (UTC-based mktime).
// Standard Gregorian calendar day-count formula, independent of the
// system's TZ setting (unlike mktime(), which would apply it).
time_t timegmCompat(const struct tm *t) {
  int year = t->tm_year + 1900;
  int month = t->tm_mon + 1;
  static const int CUMULATIVE_DAYS[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

  long days = (year - 1970) * 365L + (year - 1969) / 4 - (year - 1901) / 100 + (year - 1601) / 400;
  days += CUMULATIVE_DAYS[month - 1];
  bool isLeapYear = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  if (month > 2 && isLeapYear) days += 1;
  days += t->tm_mday - 1;

  return days * 86400L + t->tm_hour * 3600L + t->tm_min * 60L + t->tm_sec;
}

int dcfPin = -1;

volatile unsigned long lastRiseMs = 0;
volatile uint8_t bitIndex = 0;
volatile uint8_t bits[59];
volatile bool frameComplete = false;

void IRAM_ATTR onDcfEdge() {
  unsigned long now = millis();
  bool high = digitalRead(dcfPin);

  if (high) {
    // Rising edge: start of this second's pulse (or, if the previous gap
    // was much longer than ~1s, this rising edge is second 0 of a new
    // minute - DCF77 transmits no pulse at all during second 59 to mark
    // that boundary).
    unsigned long gap = now - lastRiseMs;
    if (gap > 1500 && gap < 2500) {
      if (bitIndex >= 58) frameComplete = true; // a near-complete prior minute
      bitIndex = 0;
    }
    lastRiseMs = now;
  } else {
    // Falling edge: pulse width since the rising edge encodes the bit.
    unsigned long width = now - lastRiseMs;
    int bit = -1;
    if (width >= 50 && width <= 150) {
      bit = 0;
    } else if (width >= 150 && width <= 250) {
      bit = 1;
    }

    if (bit >= 0 && bitIndex < 59) {
      bits[bitIndex] = bit;
      bitIndex++;
    } else {
      // Noise or a corrupted pulse: resync on the next minute mark rather
      // than trust a partially-decoded frame.
      bitIndex = 0;
    }
  }
}

int bcdValue(const uint8_t *frame, int start, int count) {
  static const int WEIGHTS[8] = {1, 2, 4, 8, 10, 20, 40, 80};
  int value = 0;
  for (int i = 0; i < count; i++) {
    if (frame[start + i]) value += WEIGHTS[i];
  }
  return value;
}

bool evenParity(const uint8_t *frame, int start, int count) {
  int ones = 0;
  for (int i = 0; i < count; i++) ones += frame[start + i];
  return (ones % 2) == 0;
}

// Decodes a full 59-bit DCF77 minute frame. Returns true and fills
// *utcEpoch on success (parity + sanity checks passed), false otherwise -
// a single garbled minute is simply skipped, the next one is tried a
// minute later.
bool decodeFrame(const uint8_t *frame, time_t *utcEpoch) {
  if (frame[0] != 0) return false;  // bit 0: always 0 (minute start marker)
  if (frame[20] != 1) return false; // bit 20: always 1 (start-of-time marker)

  bool cest = frame[17] == 1; // Z1: CEST (UTC+2) in effect
  bool cet = frame[18] == 1;  // Z2: CET (UTC+1) in effect
  if (cest == cet) return false; // exactly one of the two must be set

  if (!evenParity(frame, 21, 8)) return false;  // minute + its parity bit
  if (!evenParity(frame, 29, 7)) return false;  // hour + its parity bit
  if (!evenParity(frame, 36, 23)) return false; // date fields + their parity bit

  int minute = bcdValue(frame, 21, 7);
  int hour = bcdValue(frame, 29, 6);
  int day = bcdValue(frame, 36, 6);
  int month = bcdValue(frame, 45, 5);
  int year = bcdValue(frame, 50, 8);

  if (minute > 59 || hour > 23 || day < 1 || day > 31 || month < 1 || month > 12) return false;

  struct tm t = {};
  t.tm_year = year + 2000 - 1900;
  t.tm_mon = month - 1;
  t.tm_mday = day;
  t.tm_hour = hour;
  t.tm_min = minute;
  t.tm_sec = 0;

  // DCF77 transmits French/German local time (CET or CEST), not UTC.
  // timegm() treats the fields as if they were already UTC, so subtract
  // the announced offset to recover the true UTC instant.
  time_t asIfUtc = timegmCompat(&t);
  *utcEpoch = asIfUtc - (cest ? 2 : 1) * 3600;
  return true;
}

} // namespace

namespace Dcf77Clock {

void begin(int pin) {
  dcfPin = pin;
  pinMode(dcfPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(dcfPin), onDcfEdge, CHANGE);
}

bool poll() {
  if (!frameComplete) return false;

  uint8_t snapshot[59];
  noInterrupts();
  memcpy(snapshot, const_cast<uint8_t *>(bits), sizeof(snapshot));
  frameComplete = false;
  interrupts();

  time_t utcEpoch;
  if (decodeFrame(snapshot, &utcEpoch)) {
    // The frame describes the minute that just finished, and the correct
    // instant right now is the start of the *next* minute (the frame for
    // minute N is fully received right as minute N+1 begins).
    Schedule::setCurrentTime(utcEpoch + 60);
    return true;
  }
  return false;
}

} // namespace Dcf77Clock
