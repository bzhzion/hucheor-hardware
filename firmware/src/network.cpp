#include "network.h"

#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <time.h>

#include "schedule.h"

namespace {

const char *NAMESPACE = "hucheor-net";
const unsigned long STATION_CONNECT_TIMEOUT_MS = 15000;

Preferences prefs;
bool stationMode = false;
char hostnameBuf[24];
char apPasswordBuf[13];

// Same random-12-digit-code approach as ConfigServer's HTTP password (see
// config_server.cpp) - kept separate on purpose: rotating one should never
// invalidate the other.
void generateCode(char *out) {
  for (int i = 0; i < 12; i++) out[i] = '0' + (esp_random() % 10);
  out[12] = '\0';
}

void computeHostname() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(hostnameBuf, sizeof(hostnameBuf), "hucheor-%02x%02x", mac[4], mac[5]);
}

void loadOrCreateApPassword() {
  prefs.begin(NAMESPACE, false);
  String pass = prefs.getString("ap_pass", "");
  if (pass.length() == 0) {
    generateCode(apPasswordBuf);
    prefs.putString("ap_pass", apPasswordBuf);
  } else {
    pass.toCharArray(apPasswordBuf, sizeof(apPasswordBuf));
  }
  prefs.end();
}

// This toolchain's newlib doesn't provide timegm(). Same formula as
// dcf77_clock.cpp's timegmCompat() - duplicated rather than shared across
// modules for ~10 lines, not worth the coupling.
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

// NTP gives true UTC. Schedule wants French local wall-clock time encoded
// as if it were UTC (see schedule.h) - apply the EU DST rule *just* for
// this one conversion, right after a fresh NTP sync, rather than leaving a
// timezone permanently configured (which would let it silently leak into
// some future gmtime_r()/localtime_r() call elsewhere and reintroduce the
// exact hardcoded-DST-rule problem this design otherwise avoids).
void syncTimeFromNtp() {
  prefs.begin(NAMESPACE, true);
  String server = prefs.getString("ntp_server", "fr.pool.ntp.org");
  prefs.end();

  configTime(0, 0, server.c_str(), "pool.ntp.org"); // 2nd arg: fallback if the first is unreachable

  time_t now = 0;
  for (int attempt = 0; attempt < 20; attempt++) {
    now = time(nullptr);
    if (now > 8 * 365 * 24 * 3600) break; // sane post-2000ish epoch: NTP has replied
    delay(500);
  }
  if (now < 8 * 365 * 24 * 3600) {
    Serial.println("Network: NTP sync timed out");
    return;
  }

  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  struct tm localNow;
  localtime_r(&now, &localNow);
  unsetenv("TZ");
  tzset();

  Schedule::setCurrentTime(timegmCompat(&localNow));
  Serial.println("Network: clock synced from NTP");
}

bool connectToStation(const String &ssid, const String &password) {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostnameBuf);
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < STATION_CONNECT_TIMEOUT_MS) {
    delay(250);
  }
  return WiFi.status() == WL_CONNECTED;
}

} // namespace

namespace Network {

void begin() {
  computeHostname();

  prefs.begin(NAMESPACE, true);
  String ssid = prefs.getString("sta_ssid", "");
  String password = prefs.getString("sta_pass", "");
  prefs.end();

  if (ssid.length() > 0 && connectToStation(ssid, password)) {
    stationMode = true;
    Serial.printf("Network: joined \"%s\", IP %s, reachable as %s.local\n", ssid.c_str(),
                   WiFi.localIP().toString().c_str(), hostnameBuf);

    if (MDNS.begin(hostnameBuf)) {
      MDNS.addService("hucheor", "tcp", 80);
    }
    syncTimeFromNtp();
    return;
  }

  if (ssid.length() > 0) {
    Serial.println("Network: could not join the configured WiFi, falling back to standalone AP");
  }

  stationMode = false;
  loadOrCreateApPassword();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(hostnameBuf, apPasswordBuf);
  Serial.printf("Network: standalone AP \"%s\" / password: %s\n", hostnameBuf, apPasswordBuf);
}

bool isStation() { return stationMode; }

String hostname() { return String(hostnameBuf); }

String apPassword() { return String(apPasswordBuf); }

String stationSsid() {
  prefs.begin(NAMESPACE, true);
  String ssid = prefs.getString("sta_ssid", "");
  prefs.end();
  return ssid;
}

String ntpServer() {
  prefs.begin(NAMESPACE, true);
  String server = prefs.getString("ntp_server", "fr.pool.ntp.org");
  prefs.end();
  return server;
}

void configureStation(const String &ssid, const String &password, const String &ntpServer) {
  prefs.begin(NAMESPACE, false);
  prefs.putString("sta_ssid", ssid);
  prefs.putString("sta_pass", password);
  if (ntpServer.length() > 0) prefs.putString("ntp_server", ntpServer);
  prefs.end();
  delay(500); // let ESPAsyncWebServer flush its HTTP response before the reset cuts everything off
  ESP.restart();
}

} // namespace Network
