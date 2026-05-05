#include "web_server.h"
#include "game_state.h"
#include "utils.h"
#include "lora_handler.h"
#include <SPIFFS.h>

// ─── Web server instance ──────────────────────────────────────────────────────
AsyncWebServer server(80);
AsyncEventSource events("/events");

// ─── Network configuration definitions ────────────────────────────────────────
String ssid     = String("P") + ID;
String password = ssid + "password";
const IPAddress local_IP(10, 0, 0, 10);
const IPAddress gateway(0, 0, 0, 0);
const IPAddress subnet(255, 255, 255, 0);

const unsigned long SSE_INTERVAL_MS = 500;
static unsigned long last_sse_millis = 0;

static String escapeJsonString(const String &input) {
  String output;
  output.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); ++i) {
    char c = input[i];
    switch (c) {
      case '"': output += "\\\""; break;
      case '\\': output += "\\\\"; break;
      case '\n': output += "\\n"; break;
      case '\r': output += "\\r"; break;
      case '\t': output += "\\t"; break;
      default: output += c; break;
    }
  }
  return output;
}

static String buildGameDataJson() {
  String json = "{";
  json += "\"status\":\"" + escapeJsonString(processor("Status")) + "\",";
  json += "\"timeP\":\"" + escapeJsonString(processor("TimeP")) + "\",";
  json += "\"timeG\":\"" + escapeJsonString(processor("TimeG")) + "\",";
  json += "\"currentTimes\":\"" + escapeJsonString(processor("CurrentTimes")) + "\"";
  json += "}";
  return json;
}

void sendGameEventIfDue(unsigned long now) {
  if (now - last_sse_millis < SSE_INTERVAL_MS) return;
  last_sse_millis = now;
  if (events.count() == 0) return;
  String json = buildGameDataJson();
  events.send(json.c_str(), "game", now);
}

void setupWiFi() {
  WiFi.mode(WIFI_AP);
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "AP config: OK" : "AP config: FAILED");
  Serial.println(WiFi.softAP(ssid, password));
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  Serial.println(WiFi.softAPIP());
}

void setupWebServer() {
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
  }

  events.onConnect([](AsyncEventSourceClient *client) {
    String json = buildGameDataJson();
    client->send(json.c_str(), "game", millis());
  });

  // GET / → setup or game page depending on status
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (status == START || status == END)
      request->send(SPIFFS, "/setup.html", "text/html", false, processor);
    else
      request->send(SPIFFS, "/game.html", "text/html", false, processor);
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
      request->send(SPIFFS, "/end.html", "text/html", false, processor);
    else
      request->send(SPIFFS, "/game.html", "text/html", false, processor);
  });

  // GET /end
  server.on("/end", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/end.html", "text/html", false, processor);
  });

  server.addHandler(&events);
  server.begin();
}
