#include "web_server.h"
#include "game_state.h"
#include "html_templates.h"
#include "utils.h"
#include "lora_handler.h"

// ─── Web server instance ──────────────────────────────────────────────────────
AsyncWebServer server(80);

// ─── Network configuration definitions ────────────────────────────────────────
String ssid     = String("P") + ID;
String password = ssid + "password";
const IPAddress local_IP(10, 0, 0, 10);
const IPAddress gateway(0, 0, 0, 0);
const IPAddress subnet(255, 255, 255, 0);

void setupWiFi() {
  WiFi.mode(WIFI_AP);
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "AP config: OK" : "AP config: FAILED");
  Serial.println(WiFi.softAP(ssid, password));
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  Serial.println(WiFi.softAPIP());
}

void setupWebServer() {
  // GET / → setup or game page depending on status
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (status == START || status == END)
      request->send(200, "text/html", setup_html, processor);
    else
      request->send(200, "text/html", game_html, processor);
  });

  // POST / → set network ID or stop game
  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
    status = START;
    if (request->hasParam("network-id", true)) {
      int newId = request->getParam("network-id", true)->value().toInt();
      if (newId >= 1 && newId <= 255) {
        preferences.begin(PREFERENCES_NAMESPACE, PREF_RW);
        preferences.putInt("network_id", newId);
        preferences.end();
      }
      request->redirect("/");
      ESP.restart();
    } else {
      stopStations();
      request->redirect("/game");
    }
  });

  // POST /game → start or pause/unpause
  server.on("/game", HTTP_POST, [](AsyncWebServerRequest *request) {
    blinkLightsBlocking(1);
    if (request->hasParam("_method", true) &&
        request->getParam("_method", true)->value() == "pause") {
      togglePauseStations(status != PAUSED);
    } else {
      String prep_time_str = request->getParam("prep-time", true)->value();
      String game_time_str = request->getParam("game-time", true)->value();
      int prep_delim = prep_time_str.indexOf(':');
      int game_delim = game_time_str.indexOf(':');
      int prep_mins  = prep_delim == -1 ? prep_time_str.toInt() : prep_time_str.substring(0, prep_delim).toInt();
      int game_mins  = game_delim == -1 ? game_time_str.toInt() : game_time_str.substring(0, game_delim).toInt();
      int prep_secs  = prep_delim == -1 ? 0 : prep_time_str.substring(prep_delim + 1).toInt();
      int game_secs  = game_delim == -1 ? 0 : game_time_str.substring(game_delim + 1).toInt();
      long prep_time_ms = (prep_mins * 60 + prep_secs) * 1000L;
      long game_time_ms = (game_mins * 60 + game_secs) * 1000L;
      startStations(prep_time_ms, game_time_ms);
    }
    request->redirect("/game");
  });

  // GET /game → game or end page
  server.on("/game", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (status == END)
      request->send(200, "text/html", end_html, processor);
    else
      request->send(200, "text/html", game_html, processor);
  });

  // GET /end
  server.on("/end", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", end_html, processor);
  });

  server.begin();
}
