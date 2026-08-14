#include "config_server.h"

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Preferences.h>

#include "network.h"
#include "schedule.h"
#include "version.h" // generated at build time, see scripts/gen_version.py

namespace {

// Shown at the bottom of every config page - lets a shopkeeper (or, more
// realistically, whoever is helping them remotely) tell at a glance whether
// the device is running an official tagged release or a local dev build.
#define HUCHEOR_FOOTER \
  "<p class=\"hint\" style=\"margin-top:2rem;text-align:center\">Hucheor firmware " \
  FIRMWARE_VERSION "</p>"

AsyncWebServer server(80);
Preferences prefs;
const char *MESSAGE_PATH_OPEN = "/message_open.wav";
const char *MESSAGE_PATH_CLOSED = "/message_closed.wav";
const char *HTTP_USER = "admin";

// A shop announcement has no business being long: this comfortably covers a
// couple of minutes of 16-bit mono audio at typical voice sample rates,
// while capping how much of the LittleFS partition a single upload (or a
// mistaken/hostile one) can consume (secu-audit, 2026-08-14).
const size_t MAX_UPLOAD_BYTES = 2 * 1024 * 1024;
size_t uploadedBytes = 0;
bool uploadTooLarge = false;

char httpPassword[13]; // 12 digits + null terminator

const char *WEEKDAY_NAMES[7] = {"Dimanche", "Lundi", "Mardi", "Mercredi",
                                 "Jeudi",    "Vendredi", "Samedi"};

// Generates a random 12-digit code (printable on a small label, no
// ambiguous characters since it's digits only). Called once ever per
// device: stored in NVS (Preferences) so it survives reboots and firmware
// updates, and stays unique per device instead of being shared across every
// Hucheor beacon. The WiFi AP's own password is generated the same way, but
// owned by the Network module (see network.cpp) since it's a WiFi concern,
// not an HTTP one.
void generateCode(char *out) {
  for (int i = 0; i < 12; i++) {
    out[i] = '0' + (esp_random() % 10);
  }
  out[12] = '\0';
}

void loadOrCreateHttpPassword() {
  prefs.begin("hucheor", false);
  String httpPass = prefs.getString("http_pass", "");
  if (httpPass.length() == 0) {
    generateCode(httpPassword);
    prefs.putString("http_pass", httpPassword);
  } else {
    httpPass.toCharArray(httpPassword, sizeof(httpPassword));
  }
  prefs.end();
}

// Plain HTML, no external assets (works fully offline on the AP, no logo -
// not worth the effort on a page this small), large touch targets and real
// labels. Same brand palette/feel as the public website ("Le Heraut" theme:
// parchment/ink/terracotta), condensed into a small card since that is the
// only content on the page. System font stack instead of the site's custom
// webfonts (self-hosted on cdn.breizhzion.com, unreachable from an offline
// WiFi AP with no internet) - keeps it looking native on each phone at zero
// extra bytes. A tiny inline script auto-syncs the device clock from the
// visiting phone's own clock (see Schedule) - the only JS on this page.
#define HUCHEOR_STYLE_BLOCK \
  "<style>" \
  "*{box-sizing:border-box}" \
  "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;" \
  "max-width:440px;margin:2.5rem auto;padding:0 1.25rem;" \
  "background:#f3e8d2;color:#241708;line-height:1.5}" \
  ".eyebrow{display:inline-block;background:#241708;color:#e7c77a;" \
  "font-weight:700;font-size:.78rem;letter-spacing:.08em;text-transform:uppercase;" \
  "padding:.35rem .8rem;border-radius:999px;margin-bottom:1rem}" \
  "h1{color:#b1451f;margin:0 0 .3rem;font-size:1.9rem}" \
  ".card{background:#e9d9b6;border-radius:16px;padding:1.75rem;" \
  "box-shadow:0 12px 24px -16px rgba(36,23,8,.35);margin-top:1.25rem}" \
  "label{display:block;font-weight:700;margin-bottom:.5rem}" \
  ".hint{font-size:.85rem;color:#4a3420;margin:.4rem 0 1.4rem}" \
  "input[type=file],input[type=time]{display:block;width:100%;padding:.6rem;" \
  "border:1px solid #cbb98f;border-radius:8px;background:#fff;color:#241708}" \
  "button{font-size:1.05rem;font-weight:700;padding:.8rem 1.75rem;" \
  "border-radius:999px;border:none;background:#b1451f;color:#f3e8d2;" \
  "cursor:pointer;margin-top:1.4rem;transition:background .2s ease}" \
  "button:hover{background:#8a3417}" \
  "button:focus-visible,input:focus-visible{outline:3px solid #241708;outline-offset:2px}" \
  "a{color:#8a3417}" \
  "nav{margin:1rem 0}" \
  ".day-row{display:flex;align-items:center;gap:.75rem;padding:.6rem 0;" \
  "border-bottom:1px solid #cbb98f}" \
  ".day-row:last-child{border-bottom:none}" \
  ".day-row label.day-name{flex:1 0 90px;margin:0;font-weight:700}" \
  ".day-row input[type=time]{width:auto;flex:1}" \
  "</style>" \
  "<script>" \
  /* Sends the phone's LOCAL wall-clock time (already DST-adjusted by the */ \
  /* phone's own, regularly-updated OS timezone database), encoded as if */ \
  /* it were UTC - matching what Schedule::setCurrentTime() expects. Using */ \
  /* Date.now()/1000 directly would send true UTC instead, which the */ \
  /* firmware never converts back to local time (see schedule.h). */ \
  "var d=new Date();" \
  "var localAsUtc=Date.UTC(d.getFullYear(),d.getMonth(),d.getDate()," \
  "d.getHours(),d.getMinutes(),d.getSeconds())/1000;" \
  "fetch('/time',{method:'POST',headers:{'Content-Type':'text/plain'}," \
  "body:String(Math.floor(localAsUtc))}).catch(function(){});" \
  "</script>"

const char INDEX_HTML[] PROGMEM =
    "<!doctype html><html lang=\"fr\"><head>"
    "<meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<title>Hucheor - Configuration</title>"
    HUCHEOR_STYLE_BLOCK
    "</head><body>"
    "<span class=\"eyebrow\">Configuration du bo&icirc;tier</span>"
    "<h1>Hucheor</h1>"
    "<nav><a href=\"/schedule\">Horaires d'ouverture</a> &middot; <a href=\"/wifi\">R&eacute;seau</a></nav>"
    "<div class=\"card\">"
    "<form method=\"POST\" action=\"/upload/open\" enctype=\"multipart/form-data\">"
    "<label for=\"wav-open\">Message quand le commerce est ouvert (.wav)</label>"
    "<input type=\"file\" id=\"wav-open\" name=\"wav\" accept=\"audio/wav\" required>"
    "<p class=\"hint\">Format WAV, 2 Mo maximum.</p>"
    "<button type=\"submit\">Envoyer</button>"
    "</form>"
    "</div>"
    "<div class=\"card\">"
    "<form method=\"POST\" action=\"/upload/closed\" enctype=\"multipart/form-data\">"
    "<label for=\"wav-closed\">Message quand le commerce est ferm&eacute; (.wav)</label>"
    "<input type=\"file\" id=\"wav-closed\" name=\"wav\" accept=\"audio/wav\" required>"
    "<p class=\"hint\">Format WAV, 2 Mo maximum.</p>"
    "<button type=\"submit\">Envoyer</button>"
    "</form>"
    "</div>"
    HUCHEOR_FOOTER
    "</body></html>";

const char SUCCESS_HTML[] PROGMEM =
    "<!doctype html><html lang=\"fr\"><head>"
    "<meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<title>Hucheor - Message mis &agrave; jour</title>"
    HUCHEOR_STYLE_BLOCK
    "</head><body>"
    "<span class=\"eyebrow\">Configuration du bo&icirc;tier</span>"
    "<h1>Hucheor</h1>"
    "<div class=\"card\">"
    "<p>Le message audio a bien &eacute;t&eacute; mis &agrave; jour.</p>"
    "<p class=\"hint\"><a href=\"/\">Retour</a></p>"
    "</div>"
    HUCHEOR_FOOTER
    "</body></html>";

bool requireAuth(AsyncWebServerRequest *request) {
  if (!request->authenticate(HTTP_USER, httpPassword)) {
    request->requestAuthentication();
    return false;
  }
  return true;
}

String formatMinutes(uint16_t minutes) {
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", minutes / 60, minutes % 60);
  return String(buf);
}

uint16_t parseMinutes(const String &hhmm, uint16_t fallback) {
  int colon = hhmm.indexOf(':');
  if (colon < 1) return fallback;
  int hour = hhmm.substring(0, colon).toInt();
  int minute = hhmm.substring(colon + 1).toInt();
  if (hour < 0 || hour > 23 || minute < 0 || minute > 59) return fallback;
  return hour * 60 + minute;
}

String buildScheduleHtml(int currentModel) {
  if (currentModel < 0 || currentModel >= Schedule::MAX_MODELS) currentModel = 0;

  String html;
  html.reserve(4000);
  html += "<!doctype html><html lang=\"fr\"><head>"
          "<meta charset=\"UTF-8\">"
          "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
          "<title>Hucheor - Horaires</title>" HUCHEOR_STYLE_BLOCK
          "</head><body>"
          "<span class=\"eyebrow\">Configuration du bo&icirc;tier</span>"
          "<h1>Horaires d'ouverture</h1>"
          "<nav><a href=\"/\">Retour</a></nav>";

  // Model switcher: each model is a full week schedule (e.g. "Standard",
  // "Ete", "Vacances de Noel"). Which one applies to which week of the year
  // is decided further down, in the week-ranges table.
  html += "<div class=\"card\"><label>Mod&egrave;le en cours d'&eacute;dition</label><nav>";
  for (int m = 0; m < Schedule::MAX_MODELS; m++) {
    html += "<a href=\"/schedule?model=" + String(m) + "\"" +
            (m == currentModel ? " style=\"font-weight:700\"" : "") + ">" + Schedule::modelName(m) +
            "</a> ";
  }
  html += "</nav>";

  html += "<form method=\"POST\" action=\"/schedule\">"
          "<input type=\"hidden\" name=\"model\" value=\"" + String(currentModel) + "\">"
          "<label for=\"mname\">Nom de ce mod&egrave;le</label>"
          "<input type=\"text\" id=\"mname\" name=\"mname\" maxlength=\"24\" value=\"" +
          Schedule::modelName(currentModel) + "\" style=\"margin-bottom:1.2rem\">";

  for (int day = 0; day <= 6; day++) {
    Schedule::DaySchedule s = Schedule::get(currentModel, day);
    html += "<div class=\"day-row\">";
    html += "<label class=\"day-name\" for=\"d" + String(day) + "_en\">";
    html += WEEKDAY_NAMES[day];
    html += "</label>";
    html += "<input type=\"checkbox\" id=\"d" + String(day) + "_en\" name=\"d" + String(day) +
            "_en\"" + (s.enabled ? " checked" : "") + ">";
    html += "<input type=\"time\" name=\"d" + String(day) + "_open\" value=\"" +
            formatMinutes(s.openMinute) + "\">";
    html += "<input type=\"time\" name=\"d" + String(day) + "_close\" value=\"" +
            formatMinutes(s.closeMinute) + "\">";
    html += "</div>";
  }

  html += "<p class=\"hint\">Cochez les jours ouverts et r&eacute;glez les horaires de ce mod&egrave;le. "
          "Un jour non coch&eacute; est consid&eacute;r&eacute; ferm&eacute; toute la journ&eacute;e.</p>"
          "<button type=\"submit\">Enregistrer ce mod&egrave;le</button>"
          "</form></div>";

  // Week ranges: which model applies to which weeks of the year (ISO week
  // numbers, 1-53). A week with no matching range falls back to the first
  // model ("Standard").
  html += "<div class=\"card\"><form method=\"POST\" action=\"/schedule/ranges\">"
          "<label>Semaines de l'ann&eacute;e (1 &agrave; 53)</label>"
          "<p class=\"hint\">Exemple : de la semaine 1 &agrave; 26, mod&egrave;le Standard ; "
          "de la semaine 27 &agrave; 35, mod&egrave;le &Eacute;t&eacute;.</p>";
  for (int i = 0; i < Schedule::MAX_RANGES; i++) {
    Schedule::WeekRange r = Schedule::getRange(i);
    html += "<div class=\"day-row\">";
    html += "<input type=\"number\" min=\"0\" max=\"53\" name=\"r" + String(i) + "_start\" "
            "value=\"" + String(r.startWeek) + "\" placeholder=\"d&eacute;but\" style=\"width:4.5rem\">";
    html += "<input type=\"number\" min=\"0\" max=\"53\" name=\"r" + String(i) + "_end\" "
            "value=\"" + String(r.endWeek) + "\" placeholder=\"fin\" style=\"width:4.5rem\">";
    html += "<select name=\"r" + String(i) + "_model\">";
    for (int m = 0; m < Schedule::MAX_MODELS; m++) {
      html += "<option value=\"" + String(m) + "\"" + (r.model == m ? " selected" : "") + ">" +
              Schedule::modelName(m) + "</option>";
    }
    html += "</select></div>";
  }
  html += "<p class=\"hint\">Laissez d&eacute;but &agrave; 0 pour d&eacute;sactiver une ligne.</p>"
          "<button type=\"submit\">Enregistrer les semaines</button>"
          "</form></div>"
          HUCHEOR_FOOTER
          "</body></html>";
  return html;
}

} // namespace

namespace ConfigServer {

void begin() {
  // Network::begin() (called before this, from main.cpp) has already
  // brought up either the standalone AP or the shop's own WiFi network by
  // the time this runs - this module only needs its own HTTP Basic Auth
  // password, a separate concern from whichever WiFi credentials apply.
  loadOrCreateHttpPassword();

  // TODO(hucheor): once the enclosure exists, print the WiFi AP password
  // (Network::apPassword(), standalone mode only) and this HTTP password on
  // a label inside the device instead of Serial - physical possession of
  // the box is the actual trust boundary here.
  Serial.printf("Hucheor config login: %s / password: %s\n", HTTP_USER, httpPassword);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!requireAuth(request)) return;
    request->send_P(200, "text/html", INDEX_HTML);
  });

  // Auto-synced from the configuring phone's own clock (inline script in
  // HUCHEOR_STYLE_BLOCK) since this device has no internet access to reach
  // an NTP server - see schedule.h for why, and the drift this implies.
  // A plain-text POST body (not multipart/urlencoded) isn't parsed into
  // request params by ESPAsyncWebServer - read it explicitly via the onBody
  // handler instead of relying on a "magic" param name that varies by
  // library version.
  server.on(
      "/time", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!requireAuth(request)) return;
        request->send(204);
      },
      nullptr,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        if (!request->authenticate(HTTP_USER, httpPassword)) return;
        char buf[16] = {0};
        size_t n = min(len, sizeof(buf) - 1);
        memcpy(buf, data, n);
        time_t epoch = atol(buf);
        if (epoch > 0) Schedule::setCurrentTime(epoch);
      });

  server.on("/schedule", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!requireAuth(request)) return;
    int model = request->hasParam("model") ? request->getParam("model")->value().toInt() : 0;
    request->send(200, "text/html", buildScheduleHtml(model));
  });

  server.on("/schedule", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!requireAuth(request)) return;
    int model = request->hasParam("model", true) ? request->getParam("model", true)->value().toInt() : 0;
    if (model < 0 || model >= Schedule::MAX_MODELS) model = 0;

    if (request->hasParam("mname", true)) {
      Schedule::setModelName(model, request->getParam("mname", true)->value());
    }

    for (int day = 0; day <= 6; day++) {
      Schedule::DaySchedule s = Schedule::get(model, day);
      s.enabled = request->hasParam("d" + String(day) + "_en", true);
      if (request->hasParam("d" + String(day) + "_open", true)) {
        s.openMinute = parseMinutes(
            request->getParam("d" + String(day) + "_open", true)->value(), s.openMinute);
      }
      if (request->hasParam("d" + String(day) + "_close", true)) {
        s.closeMinute = parseMinutes(
            request->getParam("d" + String(day) + "_close", true)->value(), s.closeMinute);
      }
      Schedule::set(model, day, s);
    }
    request->redirect(("/schedule?model=" + String(model)).c_str());
  });

  server.on("/schedule/ranges", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!requireAuth(request)) return;
    for (int i = 0; i < Schedule::MAX_RANGES; i++) {
      Schedule::WeekRange r;
      if (request->hasParam("r" + String(i) + "_start", true)) {
        r.startWeek = request->getParam("r" + String(i) + "_start", true)->value().toInt();
      }
      if (request->hasParam("r" + String(i) + "_end", true)) {
        r.endWeek = request->getParam("r" + String(i) + "_end", true)->value().toInt();
      }
      if (request->hasParam("r" + String(i) + "_model", true)) {
        r.model = request->getParam("r" + String(i) + "_model", true)->value().toInt();
      }
      Schedule::setRange(i, r);
    }
    request->redirect("/schedule");
  });

  server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!requireAuth(request)) return;
    String html;
    html.reserve(1400);
    html += "<!doctype html><html lang=\"fr\"><head>"
            "<meta charset=\"UTF-8\">"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
            "<title>Hucheor - R&eacute;seau</title>" HUCHEOR_STYLE_BLOCK
            "</head><body>"
            "<span class=\"eyebrow\">Configuration du bo&icirc;tier</span>"
            "<h1>R&eacute;seau</h1>"
            "<nav><a href=\"/\">Retour</a></nav>"
            "<div class=\"card\"><p>";
    if (Network::isStation()) {
      html += "Le bo&icirc;tier est connect&eacute; au r&eacute;seau WiFi <strong>" + Network::stationSsid() +
              "</strong>. Il est aussi joignable sur ce r&eacute;seau via <strong>" +
              Network::hostname() + ".local</strong>, et l'heure se synchronise "
              "automatiquement par Internet (NTP).";
    } else {
      html += "Le bo&icirc;tier fonctionne en point d'acc&egrave;s autonome (<strong>" + Network::hostname() +
              "</strong>), sans connexion Internet.";
    }
    html += "</p>"
            "<form method=\"POST\" action=\"/wifi\">"
            "<label for=\"ssid\">Rejoindre le WiFi du commerce (optionnel)</label>"
            "<input type=\"text\" id=\"ssid\" name=\"ssid\" maxlength=\"32\" "
            "placeholder=\"Nom du r&eacute;seau WiFi\" value=\"" + Network::stationSsid() + "\">"
            "<label for=\"pass\" style=\"margin-top:1rem\">Mot de passe</label>"
            "<input type=\"password\" id=\"pass\" name=\"pass\" maxlength=\"63\">"
            "<label for=\"ntp\" style=\"margin-top:1rem\">Serveur NTP (heure par Internet)</label>"
            "<input type=\"text\" id=\"ntp\" name=\"ntp\" maxlength=\"63\" "
            "value=\"" + Network::ntpServer() + "\">"
            "<p class=\"hint\">Laissez le nom de r&eacute;seau vide et enregistrez pour revenir au "
            "point d'acc&egrave;s autonome. Le serveur NTP n'est utilis&eacute; que si le bo&icirc;tier rejoint un "
            "r&eacute;seau WiFi. Le bo&icirc;tier red&eacute;marre pour appliquer le changement.</p>"
            "<button type=\"submit\">Enregistrer et red&eacute;marrer</button>"
            "</form></div>"
            HUCHEOR_FOOTER
            "</body></html>";
    request->send(200, "text/html", html);
  });

  server.on("/wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!requireAuth(request)) return;
    String ssid = request->hasParam("ssid", true) ? request->getParam("ssid", true)->value() : "";
    String pass = request->hasParam("pass", true) ? request->getParam("pass", true)->value() : "";
    String ntp = request->hasParam("ntp", true) ? request->getParam("ntp", true)->value() : "";
    request->send(200, "text/plain", "Red\xC3\xA9marrage en cours...");
    Network::configureStation(ssid, pass, ntp); // restarts the device, does not return
  });

  auto handleUploadRequest = [](AsyncWebServerRequest *request, const char *path) {
    if (!requireAuth(request)) return;
    if (uploadTooLarge) {
      LittleFS.remove(path); // don't leave a truncated/corrupt WAV as the active message
      request->send(413, "text/plain",
                     "Fichier trop volumineux (2 Mo maximum). Rien n'a \xC3\xA9t\xC3\xA9 chang\xC3\xA9.");
      return;
    }
    request->send_P(200, "text/html", SUCCESS_HTML);
  };

  auto handleUploadBody = [](AsyncWebServerRequest *request, const char *path, size_t index,
                              uint8_t *data, size_t len) {
    if (!request->authenticate(HTTP_USER, httpPassword)) return;

    if (index == 0) {
      uploadedBytes = 0;
      uploadTooLarge = false;
    }
    uploadedBytes += len;
    if (uploadedBytes > MAX_UPLOAD_BYTES) {
      uploadTooLarge = true;
      return;
    }

    File file = index == 0 ? LittleFS.open(path, "w") : LittleFS.open(path, "a");
    if (file) {
      file.write(data, len);
      file.close();
    }
  };

  server.on(
      "/upload/open", HTTP_POST,
      [handleUploadRequest](AsyncWebServerRequest *request) {
        handleUploadRequest(request, MESSAGE_PATH_OPEN);
      },
      [handleUploadBody](AsyncWebServerRequest *request, String filename, size_t index,
                          uint8_t *data, size_t len, bool final) {
        handleUploadBody(request, MESSAGE_PATH_OPEN, index, data, len);
      });

  server.on(
      "/upload/closed", HTTP_POST,
      [handleUploadRequest](AsyncWebServerRequest *request) {
        handleUploadRequest(request, MESSAGE_PATH_CLOSED);
      },
      [handleUploadBody](AsyncWebServerRequest *request, String filename, size_t index,
                          uint8_t *data, size_t len, bool final) {
        handleUploadBody(request, MESSAGE_PATH_CLOSED, index, data, len);
      });

  server.begin();
}

} // namespace ConfigServer
