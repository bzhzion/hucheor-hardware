#include "config_server.h"

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFi.h>

namespace {

AsyncWebServer server(80);
const char *MESSAGE_PATH = "/message.wav";

// Plain HTML, no external assets (works fully offline on the AP), large
// touch targets and real labels for the person configuring the beacon.
const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="fr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Hucheor - Configuration</title>
<style>
  body { font-family: sans-serif; max-width: 480px; margin: 2rem auto; padding: 0 1rem; }
  label { display: block; font-weight: bold; margin-bottom: 0.5rem; }
  input[type=file] { margin-bottom: 1.5rem; }
  button {
    font-size: 1.1rem; padding: 0.75rem 1.5rem; border-radius: 8px;
    border: none; background: #b1451f; color: white; cursor: pointer;
  }
  button:focus-visible { outline: 3px solid #241708; outline-offset: 2px; }
</style>
</head>
<body>
  <h1>Hucheor</h1>
  <p>Choisissez le fichier audio (.wav) que le boîtier annoncera.</p>
  <form method="POST" action="/upload" enctype="multipart/form-data">
    <label for="wav">Fichier audio (.wav)</label>
    <input type="file" id="wav" name="wav" accept="audio/wav" required>
    <button type="submit">Envoyer</button>
  </form>
</body>
</html>
)HTML";

} // namespace

namespace ConfigServer {

void begin() {
  char apName[32];
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(apName, sizeof(apName), "Hucheor-%02X%02X", mac[4], mac[5]);
  WiFi.softAP(apName);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", INDEX_HTML);
  });

  server.on(
      "/upload", HTTP_POST,
      [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Message audio mis a jour.");
      },
      [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data,
         size_t len, bool final) {
        File file = index == 0 ? LittleFS.open(MESSAGE_PATH, "w") : LittleFS.open(MESSAGE_PATH, "a");
        if (file) {
          file.write(data, len);
          file.close();
        }
      });

  server.begin();
}

} // namespace ConfigServer
