#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include <Preferences.h>
#include <SPI.h>
#include <RF24.h>
#include <RF24Network.h>
#include <DNSServer.h>

// change when uploading to each station
const int ID = 2;

const int DNS_PORT = 53;
const int STATIONS_COUNT = 5;
const int STATIONS_LEN = STATIONS_COUNT + 1;

struct TimeMessage {
  long team1_time;
  long team2_time;
};
struct StartMessage {
  long prep_time;
  long game_time;
};
struct PauseMessage {
  bool pause;
};
Preferences preferences;
static const bool PREF_RO = true;
static const bool PREF_RW = false;
static const char *PREFERENCES_NAMESPACE = "preferences";
static const char *RESULTS_NAMESPACE = "results";

int MASTER_ID = -1;
const int TEAM1_LIGHT = 8;
const int TEAM2_LIGHT = 7;
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
const int WIFI_CHANNEL = 6; // TODO: check
const IPAddress local_IP(192,168,ID*10,8);
const IPAddress gateway(192,168,ID*10,1);
const IPAddress subnet(255,255,255,0);

AsyncWebServer server(80);
DNSServer dnsServer;
RF24 radio(20, 9); // CE, CSN
RF24Network network(radio);
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
long team1_times[] = {-1, -1, -1, -1, -1, -1};
long team2_times[] = {-1, -1, -1, -1, -1, -1};
long last_millis = millis();
long last_sent = millis();

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
    <p>Past Team Red times: %Time1P%</p>
    <p>Past Team Blue times: %Time2P%</p>
    <p>Master ID: %MasterID%</p>
    <form action='/' method='POST'>
      <select name="master-id" id="master-id">
        <option value="-1" selected disabled hidden>Choose a master ID:</option>
        <option value="-1">Master</option>
        <option value="1">1</option>
        <option value="2">2</option>
        <option value="3">3</option>
        <option value="4">4</option>
        <option value="5">5</option>
      </select> 
      <button type='submit'>Select</button>
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

long sumTimes(long times[STATIONS_LEN]) {
  long sum = 0;
  for (int i = 0; i < STATIONS_LEN; ++ i) {
    if (times[i] != -1) {
      sum += times[i];
    }
  }
  return sum;
}

String processor(const String& var)
{
  if(var == "Time1") { 
    return formatTime(sumTimes(team1_times), true); 
  }
  if(var == "Time2") {
    return formatTime(sumTimes(team2_times), true); 
  }
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
  if (var == "MasterID") { 
    if (MASTER_ID == -1) {
      return "Master";
    }
    return String(MASTER_ID); 
  }
  if (var == "Stations") { return Stations; }
  if(var == "Winner") { return sumTimes(team1_times) == sumTimes(team2_times) ? "Draw" : (sumTimes(team1_times) > sumTimes(team2_times) ? "Team Red" : "Team Blue"); }
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

void startGame(long prep_time_ms, long game_time_ms) {
  if (status != PREP && status != )
  status = PREP;
  prep_time = prep_time_ms;
  game_time = game_time_ms;
  Serial.print(prep_time_ms);
  Serial.print(" ");
  Serial.println(game_time_ms);
  team1_times[ID] = 0;
  team2_times[ID] = 0;
  last_millis = millis();
}

void endGame() {
  recordResults();
  status = END;
}

void pauseGame(bool pause) {
  if (status == PAUSED && !pause) {
    status = prev_status;
  } else {
    prev_status = status;
    status = PAUSED;
  }
}


// TODO: Speed up by sending once per loop cycle at most
void sendMessage(const void * data, uint8_t msg_type, size_t size) {
  for (int i = 0; i <= STATIONS_COUNT; ++ i) {
    RF24NetworkHeader header(i, msg_type);
    if (!network.write(header, data, size)) {
      Serial.print("Error sending message to station ");
      Serial.println(i);
    }
  }
}

void startStations(long prep_time_ms, long game_time_ms) {
  StartMessage start_msg(prep_time_ms, game_time_ms);
  sendMessage(&start_msg, 'S', sizeof(start_msg));
  startGame(prep_time_ms, game_time_ms);
}

void stopStations() {
  sendMessage(NULL, 'E', 0);
  endGame();
  // TODO: Wait for an update from all stations before returning results
}

void togglePauseStations(bool pause) {
  PauseMessage pause_msg = {pause};
  sendMessage(&pause_msg, 'P', sizeof(pause_msg));
  pauseGame(pause);
}

void sendTimes(bool immediate) {
  if (immedaite || millis() - last_sent >= 2000) {
    TimeMessage time_msg = {team1_times[ID], team2_times[ID]};
    sendMessage(&time_msg, 'T', sizeof(time_msg));
    last_sent = millis();
  }
}

void recvRadio() {
  while (network.available()) {
    RF24NetworkHeader header;
    network.peek(header);
    Serial.println(char(header.type));
    switch (header.type) {
      // time of another station
      case 'T':
        TimeMessage time_msg;
        network.read(header, &time_msg, sizeof(time_msg));
        if (header.from_node == 0) { 
          header.from_node = MASTER_ID == -1 ? ID : MASTER_ID; 
        }
        Serial.print(time_msg.team1_time);
        Serial.print(" ");
        Serial.println(time_msg.team2_time);
        team1_times[header.from_node] = time_msg.team1_time;
        team2_times[header.from_node] = time_msg.team2_time;
        break;
      // start game
      case 'S':
        StartMessage start_msg;
        network.read(header, &start_msg, sizeof(start_msg));
        startGame(start_msg.prep_time, start_msg.game_time);
        break;
      // pause game
      case 'P':
        PauseMessage pause_msg;
        network.read(header, &pause_msg, sizeof(pause_msg));
        pauseGame(pause_msg.pause);
        break;
      // end game
      case 'E':
        endGame();
        network.read(header, NULL, 0);
        sendTimes(true);
        break;
      default:
        // prevent buffer fill up
        network.read(header, NULL, 0);
        break;
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
  team1ResultsNew[0] = sumTimes(team1_times);
  team2ResultsNew[0] = sumTimes(team2_times);
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

  Serial.print("MASTER_ID: ");
  Serial.println(MASTER_ID);
  radio.begin();
  radio.setPALevel(RF24_PA_MIN, 0);
  WiFi.mode(WIFI_AP);
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");
  Serial.println(WiFi.softAP(ssid, password)); // Serial.println(WiFi.softAP(ssid, password, CHANNEL));
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  Serial.println(WiFi.softAPIP());
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  // server setup
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
      ESP.restart();
    } else { 
      stopStations();
      request->send_P(200, "text/html", end_html, processor);
    }
  });
  // start; pause/unpause
  server.on("/game", HTTP_POST, [](AsyncWebServerRequest *request) {
    blinkLightsBlocking(1);
    // paused button
    if (request->hasParam("_method", true) && request->getParam("_method", true)->value() == "pause") {
      // pause all stations, including this one
      togglePauseStations((status != PAUSED));
    } else {
      // process prep and game time from incoming request string
      String prep_time_str = request->getParam("prep-time", true)->value();
      String game_time_str = request->getParam("game-time", true)->value();
      int prep_delim = prep_time_str.indexOf(':');
      int game_delim = game_time_str.indexOf(':');
      int prep_mins = prep_delim == -1 ? prep_time_str.toInt() : prep_time_str.substring(0, prep_delim).toInt();
      int game_mins = game_delim == -1 ? game_time_str.toInt() : game_time_str.substring(0, game_delim).toInt();
      int prep_secs = prep_delim == -1 ? 0 : prep_time_str.substring(prep_delim + 1, prep_time_str.length()).toInt();
      int game_secs = game_delim == -1 ? 0 : game_time_str.substring(game_delim + 1, game_time_str.length()).toInt();
      long prep_time_ms = (prep_mins * 60 + prep_secs) * 1000;
      long game_time_ms = (game_mins * 60 + game_secs) * 1000;
      // start all stations, including this one
      startStations(prep_time_ms, game_time_ms);
    }
    if (request->hasParam("_short", true) && request->getParam("_short", true)->value() == "short") {
      request->send_P(200, "text/httml", game_html_short, processor);
    } else {
      request->send_P(200, "text/html", game_html, processor);
    }
  });
  // end
  server.on("/game", HTTP_GET, [](AsyncWebServerRequest *request){
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
  if (MASTER_ID != -1) {
    // slave
    network.begin(90 + MASTER_ID, ID);
  } else {
    // master
    // Connect to the mesh
    network.begin(90 + ID, 0);
  }
  // Serial.println(mesh.checkConnection());
  Serial.println("Network ready");
  blinkLightsBlocking(3);
}

void loop(){
  network.update();
  dnsServer.processNextRequest();
  // read from network
  recvRadio();
  if (status == TEAM1 || status == TEAM2 || status == NEUTRAL) {
    if (status == TEAM1) {
      team1_times[ID] += millis() - last_millis;
      digitalWrite(TEAM1_LIGHT, HIGH);     
      digitalWrite(TEAM2_LIGHT, LOW);
    }
    if (status == TEAM2) {
      team2_times[ID] += millis() - last_millis;
      digitalWrite(TEAM2_LIGHT, HIGH);     
      digitalWrite(TEAM1_LIGHT, LOW);
    }
    sendTimes(false);
    if (team1_btn_last != digitalRead(TEAM1_BTN) && team1_btn_last == LOW) {
      status = TEAM1;
    }
    if (team2_btn_last != digitalRead(TEAM2_BTN) && team2_btn_last == LOW) {
      status = TEAM2;
    }
    game_time = max(game_time - (long(millis()) - last_millis), 0L);
    if (game_time <= 0) {
      endGame();
      sendTime(true);
    }
    // blinking when neutral  
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