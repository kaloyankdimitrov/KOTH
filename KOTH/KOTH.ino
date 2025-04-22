#include <ESP8266WiFi.h>
#include "ESPAsyncWebServer.h"

const int TEAM1_LIGHT = 0;
const int TEAM2_LIGHT = 2;
const int TEAM1_BTN = 4;
const int TEAM2_BTN = 5;
bool team1_btn_last = HIGH;
bool team2_btn_last = HIGH;

const char* ssid = "P1";
const char* password =  "p1password";

const IPAddress local_IP(192,168,1,2);
const IPAddress gateway(192,168,1,1);
const IPAddress subnet(255,255,255,0);
enum {
  START,
  PAUSED,
  PREP,
  NEUTRAL,
  TEAM1,
  TEAM2,
  END
};
int status = START;
int prev_status = START;
long prep_time = 0;
long game_time = 0;
long team1_time = 0;
long team2_time = 0;
long last_millis = millis();

const char * setup_html = R"(
<!DOCTYPE html>
<html>
  <head>
    <title>King of the Hill</title>
    <style>
      form > * {
        display: block;
      }
    </style>
  </head>
  <body>
    <form action='/game' method='POST'>
      <label for='prep-time'>Preparation duration:</label>
      <input type='text' id='prep-time' name='prep-time' />
      <label for='game-time'>Game duration:</label>
      <input type='text' id='game-time' name='game-time' />
      <button type='submit'>Start</button>
    </form>
  </body>
</html>
)";

const char * game_html = R"(
<!DOCTYPE html>
<html>
  <head>
    <title>King of the Hill</title>
    <style>
      body > * {
        display: block;
      }
    </style>
  </head>
  <body>
    <h3>%Status%</h3>
    <p>Prep time left: %TimeP%</p>
    <p>Game time left: %TimeG%</p>
    <p>Team 1 time: %Time1%</p>
    <p>Team 2 time: %Time2%</p>
    <div>
      <form action='/' method='POST'>
        <button type='submit'>Stop</button>
      </form>
      <a href='/game'><button>Refresh Page</button></a>
    </div>
  </body>
</html>
)";

const char * end_html = R"(
<!DOCTYPE html>
<html>
  <head>
    <title>King of the Hill</title>
    <style>
      body > * {
        display: block;
      }
    </style>
  </head>
  <body>
    <h3>Winner: %Winner%</h3>
    <p>Team 1 time:%Time1%</p>
    <p>Team 2 time:%Time2%</p>
  </body>
</html>
)";

AsyncWebServer server(80);

String processor(const String& var)
{
  Serial.println(prep_time);
  if(var == "Time1") { return String(team1_time / 60000) + ":" + String((team1_time / 1000) % 60) + "." + String(team1_time % 1000); }
  if(var == "Time2") { return String(team2_time / 60000) + ":" + String((team2_time / 1000) % 60) + "." + String(team2_time % 1000); }
  if(var == "TimeP") { return String(prep_time / 60000) + ":" + String((prep_time / 1000) % 60); }
  if(var == "TimeG") { return String(game_time / 60000) + ":" + String((game_time / 1000) % 60); }
  if(var == "Winner") { return team1_time > team2_time ? "Team 1" : "Team 2"; }
  if (var == "Status") {
    switch (status) {
      case PAUSED:
        return "Paused";
        break;
      case PREP:
        return "Preparation phase.";
        break;
      case TEAM1:
        return "Team 1 control";
        break;
      case TEAM2:
        return "Team 2 control";
        break;
      case NEUTRAL:
        return "NEUTRAL";
        break;
      case END:
        return "Ended. Refresh!";
        break;
      default:
        return "Error";
        break;
    }
  } 
  return String();
}

void setup(){
  Serial.begin(115200);
  pinMode(TEAM1_LIGHT, OUTPUT);
  pinMode(TEAM2_LIGHT, OUTPUT);
  pinMode(TEAM1_BTN, INPUT_PULLUP);
  pinMode(TEAM2_BTN, INPUT_PULLUP);
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();

  // setup
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    status = START;
    request->send_P(200, "text/html", setup_html);
  });
  // start; pause/unpause
  server.on("/game", HTTP_POST, [](AsyncWebServerRequest *request){
    // paused button
    if (request->hasParam("_method", true) && request->getParam("_method", true)->value() == "pause") {
      if (status == PAUSED) {
        status = prev_status;
        last_millis = millis();
      } else {
        prev_status = status;
        status = PAUSED;
      }
    } else {
      status = PREP;
      last_millis = millis();
      String prep_time_str = request->getParam("prep-time", true)->value();
      String game_time_str = request->getParam("game-time", true)->value();
      int prep_delim = prep_time_str.indexOf(':');
      int game_delim = game_time_str.indexOf(':');
      int prep_mins = prep_time_str.substring(0, prep_delim).toInt();
      int game_mins = game_time_str.substring(0, game_delim).toInt();
      int prep_secs = prep_time_str.substring(prep_delim + 1, prep_time_str.length()).toInt();
      int game_secs = game_time_str.substring(game_delim + 1, game_time_str.length()).toInt();
      prep_time = (prep_mins * 60 + prep_secs) * 1000;
      game_time = (game_mins * 60 + game_secs) * 1000;
    }
    request->send_P(200, "text/html", game_html, processor);
  });
  // end
  server.on("/game", HTTP_GET, [](AsyncWebServerRequest *request){
    if (status == END) {
      request->send_P(200, "text/html", end_html, processor);
    } else {
      request->send_P(200, "text/html", game_html, processor);
    }
  });
  // stop
  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request){
    status = START;
    request->send_P(200, "text/html", setup_html, processor);
  });

  server.begin();
}

void loop(){
  if (status == TEAM1 || status == TEAM2 || status == NEUTRAL) {
    if (status == TEAM1) {
      team1_time += millis() - last_millis;
      digitalWrite(TEAM1_LIGHT, HIGH);     
      digitalWrite(TEAM2_LIGHT, LOW);
    }
    if (status == TEAM2) {
      team2_time += millis() - last_millis;
      digitalWrite(TEAM2_LIGHT, HIGH);     
      digitalWrite(TEAM1_LIGHT, LOW);
    }
    if (team1_btn_last != digitalRead(TEAM1_BTN) && team1_btn_last == LOW) {
      status = TEAM1;
    }
    if (team2_btn_last != digitalRead(TEAM2_BTN) && team2_btn_last == LOW) {
      status = TEAM2;
    }
    game_time = max(game_time - (long(millis()) - last_millis), 0L);
    last_millis = millis();
    if (game_time <= 0) {
      status = END;
    }
  } else if (status == PREP || status == NEUTRAL) {
    prep_time = max(prep_time - (long(millis()) - last_millis), 0L);
    last_millis = millis();
    if (prep_time <= 0) { status = NEUTRAL; }
    digitalWrite(TEAM1_LIGHT, HIGH);
    digitalWrite(TEAM2_LIGHT, HIGH);
  } else {
    digitalWrite(TEAM1_LIGHT, LOW);
    digitalWrite(TEAM2_LIGHT, LOW);
  }
  team1_btn_last = digitalRead(TEAM1_BTN);
  team2_btn_last = digitalRead(TEAM2_BTN);  
  delay(50);
}