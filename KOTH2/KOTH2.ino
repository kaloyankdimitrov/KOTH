#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include <Preferences.h>
#include <SPI.h>
#include <RF24.h>
#include <RF24Network.h>
#include <RF24Mesh.h>
#include <DNSServer.h>

const int ID = 2;
  
Preferences preferences;
static const bool PREF_RO = true;
static const bool PREF_RW = false;
static const char *PREFERENCES_NAMESPACE = "preferences";
static const char *RESULTS_NAMESPACE = "results";
bool slaves[5];
int MASTER_ID = -1;
const int TEAM1_LIGHT = 8;
const int TEAM2_LIGHT = 6;
const int TEAM1_BTN = 21;
const int TEAM2_BTN = 10;
bool team1_btn_last = HIGH;
bool team2_btn_last = HIGH;
int last_blink_millis = 0;
bool blink_last = LOW;
bool record = false;
String ssid = String("P") + ID;
String password =  ssid + "password";
String Stations = "";
bool writingResults = false;
const int CHANNEL = 5;
const IPAddress local_IP(192,168,ID*10,8);
const IPAddress gateway(192,168,ID*10,1);
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
      * {
        font-size: 36pt;
      } 
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
    // <form action='/' method='POST'>
    //   <select name="master-id" id="master-id">
    //     <option value="-1" selected disabled hidden>Choose a master ID:</option>
    //     <option value="1">1</option>
    //     <option value="2">2</option>
    //     <option value="3">3</option>
    //     <option value="4">4</option>
    //     <option value="5">5</option>
    //   </select> 
    //   <button type='submit'>Select</button>
    // </form>

const char * game_html = R"(
<!DOCTYPE html>
<html>
  <head>
    <title>King of the Hill</title>
    <style>
      * {
        font-size: 36pt;
      }
      body > * {
        display: block;
      }
      button {
        margin: 2em;
      }
    </style>
  </head>
  <body>
    <h3>%Status%</h3>
    <p>Prep time left: %TimeP%</p>
    <p>Game time left: %TimeG%</p>
    <p>Team Red time: %Time1%</p>
    <p>Team Blue time: %Time2%</p>
    %Stations%
    <div>
      <form action='/' method='POST'>
        <button type='submit'>Stop</button>
      </form>
      <form action='/game' method='POST'>
        <input type='hidden' name='_method' id='_method' value='pause' />
        <button type='submit'>Pause/Unpause</button>
      </form>
      <form action='/game' method='GET'>
        <button type='submit'>Refresh Page</button>
      </form>
    </div>
  </body>
</html>
)";

const char * game_html_short = R"(
<h2>Station: %StationID%</h2>
<h3>%Status%</h3>
<p>Prep time left: %TimeP%</p>
<p>Game time left: %TimeG%</p>
<p>Team Red time: %Time1%</p>
<p>Team Blue time: %Time2%</p>
)";

const char * end_html = R"(
<!DOCTYPE html>
<html>
  <head>
    <title>King of the Hill</title>
    <style>
      * {
        font-size: 36pt;
      }
      body > * {
        display: block;
      }
    </style>
  </head>
  <body>
    <h3>Winner: %Winner%</h3>
    <p>Team Red time: %Time1%</p>
    <p>Team Blue time: %Time2%</p>

    <p>Past Team Red times: %Time1P%</p>
    <p>Past Team Blue times: %Time2P%</p>
    %Stations%
    <a href='/'><button>New Game</button></a>
  </body>
</html>
)";


const char * end_html_short = R"(
<h2>Station: %StationID%</h2>
<h3>Winner: %Winner%</h3>
<p>Team Red time:%Time1%</p>
<p>Team Blue time:%Time2%</p>
)";

AsyncWebServer server(80);
DNSServer dnsServer;

String formatTime(long time, bool miliseconds) {
  String res = "";
  if (time / 6000 < 10) {
    res += 0;
  }
  res += time / 60000;
  res += ":";
  if ((time / 1000) % 60 < 10) {
    res += 0;
  } 
  res += (time / 1000) % 60;
  if (miliseconds) {
    res += ".";
    if (time % 1000 < 100) {
      res += 0;
    } 
    if (time % 1000 < 10) {
      res += 0;
    } 
    res += time % 1000;
  }
  return res;
}

String processor(const String& var)
{
  if(var == "Time1") { return formatTime(team1_time, true); }
  if(var == "Time2") { return formatTime(team2_time, true); }
  if(var == "Time1P" || var == "Time2P") {
    while(writingResults);
    String res = ""; 
    long teamResults[5];
    preferences.begin(RESULTS_NAMESPACE, PREF_RO);
    preferences.getBytes(var == "Time1P" ? "team1" : "team2", teamResults, 5 * sizeof(long));
    preferences.end();
    for (int i = 0; i < 5; ++ i) {
      res += formatTime(teamResults[i], true) + " ";
    }
    return res;
  }
  if(var == "TimeP") { return formatTime(prep_time, false); }
  if(var == "TimeG") { return formatTime(game_time, false); }
  if (var == "StationID") { return String(ID); }
  if (var == "MasterID") { return String(MASTER_ID); }
  if (var == "Stations") { return Stations; }
  if(var == "Winner") { return team1_time == team2_time ? "Draw" : (team1_time > team2_time ? "Team Red" : "Team Blue"); }
  if (var == "Status") {
    switch (status) {
      case PAUSED:
        return "Paused";
        break;
      case PREP:
        return "Preparation phase.";
        break;
      case TEAM1:
        return "Team Red control";
        break;
      case TEAM2:
        return "Team Blue control";
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

void getRelay() {
  Stations = "";
  for (int i = 0; i < 5; ++ i) {
    if (slaves[i]) {
      IPAddress server(192, 168, ID*10, i+11);
      const int port = 80;
      if (httpClients[i].connect(server, port, 500)) {
        httpClients[i].print("GET /gameShort?\r\n");
        while (httpClients[i].available()) {
            Stations += httpClients[i].readStringUntil('\n');
        }
        httpClients[i].stop();
      }
    }
  }
}
void postRelay(bool pause, String prepTime, String gameTime) {
  Stations = "";
  for (int i = 0; i < 5; ++ i) {
    if (i + 1 != ID) {
      IPAddress server(192, 168, ID*10, i+11);
      const int port = 80;
      if (httpClients[i].connect(server, port, 500)) {
        slaves[i] = true;
        String params = "POST /game?_short=short";
        if (!pause) {
          params += "&prep-time=" + prepTime + "&game-time=" + gameTime;
        } else {
          params += "&_method=pause";
        }
        params += " HTTP/1.1\r\nHost: ";
        params += "192.168." + String(ID * 10) + ".8\r\n";
        params += "Content-Type: application/x-www-form-urlencoded\r\n";
        params += "Connection: keep-alive\r\n";
        httpClients[i].print(params);
        while (httpClients[i].available()) {
            Stations += httpClients[i].readStringUntil('\n');
        }
        httpClients[i].stop();
      }
    }
    
  }
}

void blinkLightsBlocking(int count) {
  digitalWrite(TEAM1_LIGHT, LOW);
  digitalWrite(TEAM2_LIGHT, LOW);
  delay(500);
  for (int i = 0; i < count; ++ i) {
    digitalWrite(TEAM1_LIGHT, HIGH);
    digitalWrite(TEAM2_LIGHT, HIGH);
    delay(500);
    digitalWrite(TEAM1_LIGHT, LOW);
    digitalWrite(TEAM2_LIGHT, LOW);
    delay(500);
  }
}

void recordResults() {

  writingResults = true;
  preferences.begin(RESULTS_NAMESPACE, PREF_RO);
  long team1Results[5], team2Results[5], team1ResultsNew[5], team2ResultsNew[5];
  preferences.getBytes("team1", team1Results, sizeof(team1Results));
  preferences.getBytes("team2", team2Results, sizeof(team2Results));
  for (int i = 4; i > 0; -- i) {
    team1ResultsNew[i] = team1Results[i - 1];
    team2ResultsNew[i] = team2Results[i - 1];
  }
  preferences.end();
  team1ResultsNew[0] = team1_time;
  team2ResultsNew[0] = team2_time;
  preferences.begin(RESULTS_NAMESPACE, PREF_RW);
  preferences.putBytes("team1", &team1ResultsNew, sizeof(team1ResultsNew));
  preferences.putBytes("team2", &team2ResultsNew, sizeof(team2ResultsNew));
  preferences.end();
  writingResults = false;
}

void setup(){
  Serial.begin(115200);
  pinMode(TEAM1_LIGHT, OUTPUT);
  pinMode(TEAM2_LIGHT, OUTPUT);
  digitalWrite(TEAM1_LIGHT, LOW);
  digitalWrite(TEAM2_LIGHT, LOW);
  pinMode(TEAM1_BTN, INPUT_PULLUP);
  pinMode(TEAM2_BTN, INPUT_PULLUP);
  delay(1000);
  Serial.println("Initializing...");
  blinkLightsBlocking(1);
  delay(1000);
  preferences.begin(PREFERENCES_NAMESPACE, PREF_RO);
  MASTER_ID = preferences.getInt("master_id", -1);
  preferences.end();
  // FIX!!
  // MASTER_ID = 1;
  if (MASTER_ID != -1 && !digitalRead(TEAM1_BTN) && !digitalRead(TEAM2_BTN)) {
    int startTimeHold = millis();
    while(!digitalRead(TEAM1_BTN) && !digitalRead(TEAM2_BTN) && millis() - startTimeHold < 3000);
    if (startTimeHold >= 3000) {
      MASTER_ID = -1;
      preferences.begin(PREFERENCES_NAMESPACE, PREF_RW);
      preferences.putInt("master_id", -1);
      preferences.end();
      blinkLightsBlocking(2);
    }
  }
  preferences.begin(RESULTS_NAMESPACE, PREF_RW);
  if(preferences.getBytesLength("team1") != 5 * sizeof(long) && preferences.getBytesLength("team2") != 5 * sizeof(long)) {
    long team1Results[5], team2Results[5];
    for (int i = 0; i < 5; ++ i) {
      team1Results[i] = 0;
      team2Results[i] = 0;
    }
    preferences.putBytes("team1", &team1Results, sizeof(team1Results));
    preferences.putBytes("team2", &team2Results, sizeof(team2Results));
  }
  preferences.end();


  Serial.println(MASTER_ID);
  if (MASTER_ID != -1) {
    // slave
    const IPAddress slave_local_IP(192,168,MASTER_ID*10,ID+10);
    const IPAddress slave_gateway(192,168,MASTER_ID*10,1);
    // FIX!!
    String conn_ssid = "P" + String(MASTER_ID);
    Serial.println(conn_ssid);
    String conn_password =  conn_ssid + "password";
    WiFi.mode(WIFI_STA);
    Serial.println(WiFi.config(slave_local_IP, slave_gateway, subnet) ? "Ready" : "Failed!");
    while(!(WiFi.status() == WL_CONNECTED)) {
      WiFi.begin(conn_ssid.c_str(), conn_password.c_str(), CHANNEL);
      WiFi.setTxPower(WIFI_POWER_8_5dBm);
      delay(5000);
    }
    Serial.println(WiFi.localIP());
    blinkLightsBlocking(3);
  } else {
    // master
    WiFi.mode(WIFI_AP);
    Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");
    Serial.println(WiFi.softAP(ssid, password, CHANNEL));
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    Serial.println(WiFi.softAPIP());
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  }

  // setup
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    status = START;
    request->send_P(200, "text/html", setup_html, processor);
  });
  // change master/stop
  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request){
    status = START;
    if (request->hasParam("master-id", true)) {
      preferences.begin(PREFERENCES_NAMESPACE, PREF_RW);
      preferences.putInt("master_id", request->getParam("master-id", true)->value().toInt());
      preferences.end();
      request->send_P(200, "text/html", setup_html, processor);
    } else {
      request->send_P(200, "text/html", end_html, processor);
    }
  });
  // start; pause/unpause
  server.on("/game", HTTP_POST, [](AsyncWebServerRequest *request) {
    blinkLightsBlocking(1);
    // paused button
    if (request->hasParam("_method", true) && request->getParam("_method", true)->value() == "pause") {
      // postRelay(true, "", "");
      if (status == PAUSED) {
        status = prev_status;
        // Serial.println(prev_status);
      } else {
        prev_status = status;
        status = PAUSED;
      }
    } else {
      String prep_time_str = request->getParam("prep-time", true)->value();
      String game_time_str = request->getParam("game-time", true)->value();
      // postRelay(false, prep_time_str, game_time_str);
      status = PREP;
      int prep_delim = prep_time_str.indexOf(':');
      int game_delim = game_time_str.indexOf(':');
      int prep_mins = prep_delim == -1 ? prep_time_str.toInt() : prep_time_str.substring(0, prep_delim).toInt();
      int game_mins = game_delim == -1 ? game_time_str.toInt() : game_time_str.substring(0, game_delim).toInt();
      int prep_secs = prep_delim == -1 ? 0 : prep_time_str.substring(prep_delim + 1, prep_time_str.length()).toInt();
      int game_secs = game_delim == -1 ? 0 : game_time_str.substring(game_delim + 1, game_time_str.length()).toInt();
      prep_time = (prep_mins * 60 + prep_secs) * 1000;
      game_time = (game_mins * 60 + game_secs) * 1000;
      team1_time = 0;
      team2_time = 0;
      last_millis = millis();
    }
    if (request->hasParam("_short", true) && request->getParam("_short", true)->value() == "short") {
      request->send_P(200, "text/httml", game_html_short, processor);
    } else {
      request->send_P(200, "text/html", game_html, processor);
    }
  });
  // end
  server.on("/game", HTTP_GET, [](AsyncWebServerRequest *request){
    // getRelay();
    if (status == END) {
      request->send_P(200, "text/html", end_html, processor);
    } else {
      request->send_P(200, "text/html", game_html, processor);
    }
  });
  // end
  server.on("/end", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "texthtml", end_html, processor);
  });
  // end
  server.on("/gameShort", HTTP_GET, [](AsyncWebServerRequest *request){
    if (status == END) {
      request->send_P(200, "text/html", end_html_short, processor);
    } else {
      request->send_P(200, "text/html", game_html_short, processor);
    }
  });
  // for dns server
  server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request){
    request->redirect("/");
  });
  server.on("/fwlink", HTTP_GET, [](AsyncWebServerRequest *request){
    request->redirect("/");
  });

  server.begin();
  blinkLightsBlocking(2);
}

void loop(){
  dnsServer.processNextRequest();
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
    if (game_time <= 0) {
      recordResults();
      status = END;
    }
    if  (status == NEUTRAL && millis() - last_blink_millis >= 1000) {
      blink_last = !blink_last;
      digitalWrite(TEAM1_LIGHT, !blink_last);
      digitalWrite(TEAM2_LIGHT, !blink_last);
      last_blink_millis = millis();
    }
  } else if (status == PREP || status == PAUSED || status == NEUTRAL) {
    if (status == PREP) {
      prep_time = max(prep_time - (long(millis()) - last_millis), 0L);
    }
    if (prep_time <= 0 && status != PAUSED) { status = NEUTRAL; }
    if  (status == PAUSED && millis() - last_blink_millis >= 1000) {
      blink_last = !blink_last;
      digitalWrite(TEAM1_LIGHT, !blink_last);
      digitalWrite(TEAM2_LIGHT, !blink_last);
      last_blink_millis = millis();
    }
  } else {
    digitalWrite(TEAM1_LIGHT, LOW);
    digitalWrite(TEAM2_LIGHT, LOW);
  }
  last_millis = millis();
  team1_btn_last = digitalRead(TEAM1_BTN);
  team2_btn_last = digitalRead(TEAM2_BTN);  
  delay(50);
}