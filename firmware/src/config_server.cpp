#include "config_server.h"

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>

namespace {

AsyncWebServer server(80);
Preferences prefs;
const char *MESSAGE_PATH = "/message.wav";
const char *HTTP_USER = "admin";

// A shop announcement has no business being long: this comfortably covers a
// couple of minutes of 16-bit mono audio at typical voice sample rates,
// while capping how much of the LittleFS partition a single upload (or a
// mistaken/hostile one) can consume (secu-audit, 2026-08-14).
const size_t MAX_UPLOAD_BYTES = 2 * 1024 * 1024;
size_t uploadedBytes = 0;

char apPassword[13]; // 12 digits + null terminator
char httpPassword[13];

// Generates a random 12-digit code (printable on a small label, no
// ambiguous characters since it's digits only). Called once ever per
// device: stored in NVS (Preferences) so it survives reboots and firmware
// updates, and stays unique per device instead of being shared across every
// Hucheor beacon.
void generateCode(char *out) {
  for (int i = 0; i < 12; i++) {
    out[i] = '0' + (esp_random() % 10);
  }
  out[12] = '\0';
}

void loadOrCreateCredentials() {
  prefs.begin("hucheor", false);

  String wifiPass = prefs.getString("wifi_pass", "");
  if (wifiPass.length() == 0) {
    generateCode(apPassword);
    prefs.putString("wifi_pass", apPassword);
  } else {
    wifiPass.toCharArray(apPassword, sizeof(apPassword));
  }

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
// labels. Same brand colors as the public website ("Le Heraut" theme).
const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="fr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Hucheor - Configuration</title>
<style>
  body {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    max-width: 480px; margin: 2rem auto; padding: 0 1.25rem;
    background: #f3e8d2; color: #241708;
  }
  h1 { color: #b1451f; }
  label { display: block; font-weight: bold; margin-bottom: 0.5rem; }
  input[type=file] {
    display: block; width: 100%; margin-bottom: 1.5rem; padding: 0.6rem;
    border: 1px solid #cbb98f; border-radius: 8px; background: #fff;
  }
  button {
    font-size: 1.1rem; padding: 0.75rem 1.75rem; border-radius: 999px;
    border: none; background: #b1451f; color: #f3e8d2; cursor: pointer;
    transition: background 0.2s ease;
  }
  button:hover { background: #8a3417; }
  button:focus-visible { outline: 3px solid #241708; outline-offset: 3px; }
</style>
</head>
<body>
  <h1>Hucheor</h1>
  <p>Choisissez le fichier audio (.wav) que le boitier annoncera.</p>
  <form method="POST" action="/upload" enctype="multipart/form-data">
    <label for="wav">Fichier audio (.wav)</label>
    <input type="file" id="wav" name="wav" accept="audio/wav" required>
    <button type="submit">Envoyer</button>
  </form>
</body>
</html>
)HTML";

bool requireAuth(AsyncWebServerRequest *request) {
  if (!request->authenticate(HTTP_USER, httpPassword)) {
    request->requestAuthentication();
    return false;
  }
  return true;
}

} // namespace

namespace ConfigServer {

void begin() {
  // Bring the radio subsystem up *before* generating the random security
  // codes below: esp_random()'s entropy quality depends on RF activity
  // (Espressif's own guidance), so generating credentials first thing at
  // boot - before any radio has ever been enabled - could be weaker than
  // intended on a device's very first run (secu-audit, 2026-08-14).
  WiFi.mode(WIFI_AP);

  loadOrCreateCredentials();

  char apName[32];
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(apName, sizeof(apName), "Hucheor-%02X%02X", mac[4], mac[5]);
  WiFi.softAP(apName, apPassword);

  // TODO(hucheor): once the enclosure exists, print apName/apPassword/
  // HTTP_USER+httpPassword on a label inside the device instead of Serial -
  // physical possession of the box is the actual trust boundary here.
  Serial.printf("Hucheor WiFi AP: %s / password: %s\n", apName, apPassword);
  Serial.printf("Hucheor config login: %s / password: %s\n", HTTP_USER, httpPassword);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!requireAuth(request)) return;
    request->send_P(200, "text/html", INDEX_HTML);
  });

  server.on(
      "/upload", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        if (!requireAuth(request)) return;
        request->send(200, "text/plain", "Message audio mis a jour.");
      },
      [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data,
         size_t len, bool final) {
        if (!request->authenticate(HTTP_USER, httpPassword)) return;

        if (index == 0) uploadedBytes = 0;
        uploadedBytes += len;
        if (uploadedBytes > MAX_UPLOAD_BYTES) {
          Serial.println("ConfigServer: upload rejected, over size limit");
          return;
        }

        File file = index == 0 ? LittleFS.open(MESSAGE_PATH, "w") : LittleFS.open(MESSAGE_PATH, "a");
        if (file) {
          file.write(data, len);
          file.close();
        }
      });

  server.begin();
}

} // namespace ConfigServer
