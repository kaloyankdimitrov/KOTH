#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include "config.h"
#include "game_state.h"
#include "lora_handler.h"
#include "web_server.h"
#include "utils.h"

// ─── Station identity ────────────────────────────────────────────────────────
// Change when uploading to each station (1–5).
const int ID = 1;

// ═══════════════════════════════════════════════════════════════════════════════
//  setup()
// ═══════════════════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);

  pinMode(TEAM1_LIGHT, OUTPUT);
  pinMode(TEAM2_LIGHT, OUTPUT);
  digitalWrite(TEAM1_LIGHT, LOW);
  digitalWrite(TEAM2_LIGHT, LOW);
  pinMode(TEAM1_BTN, INPUT_PULLUP);
  pinMode(TEAM2_BTN, INPUT_PULLUP);

  Serial.println("Initializing...");
  blinkLightsBlocking(1);
  delay(100);

  // ── Read NETWORK_ID from NVS ───────────────────────────────────────────────
  preferences.begin(PREFERENCES_NAMESPACE, PREF_RO);
  NETWORK_ID = preferences.getInt("network_id", 0);
  preferences.end();

  Serial.println(NETWORK_ID == 0 ? "  (not configured)" : "");

  // ── Results NVS initialisation ─────────────────────────────────────────────
  preferences.begin(RESULTS_NAMESPACE, PREF_RW);
  if (preferences.getBytesLength("team1") != HISTORY_LEN * STATIONS_COUNT * sizeof(long) &&
      preferences.getBytesLength("team2") != HISTORY_LEN * STATIONS_COUNT * sizeof(long)) {
    long t1[HISTORY_LEN][STATIONS_COUNT], t2[HISTORY_LEN][STATIONS_COUNT];
    for (int i = 0; i < HISTORY_LEN; ++i)
      for (int j = 0; j < STATIONS_COUNT; ++j)
        t1[i][j] = t2[i][j] = -1;
    preferences.putBytes("team1", &t1, sizeof(t1));
    preferences.putBytes("team2", &t2, sizeof(t2));
  }
  preferences.end();

  // ── Initialize game state arrays ───────────────────────────────────────────
  initGameState();

  // ── LoRaMesher (replaces RF24 / RF24Network) ───────────────────────────────
  setupLoraMesher();

  // ── Wi-Fi AP + web server ──────────────────────────────────────────────────
  setupWiFi();
  setupWebServer();

  last_millis = millis();
  last_sent   = millis();

  blinkLightsBlocking(2);
  Serial.println("Setup complete");
  blinkLightsBlocking(3);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  loop()
// ═══════════════════════════════════════════════════════════════════════════════
//
//  LoRaMesher runs entirely on its own FreeRTOS tasks — no network.update()
//  call is needed here.  The only radio-related call in loop() is sendTimes(),
//  which broadcasts this station's times on a background interval.
//
// ═══════════════════════════════════════════════════════════════════════════════

void loop() {
  long now = millis();

  if (status == TEAM1 || status == TEAM2 || status == NEUTRAL) {
    if (status == TEAM1) {
      team1_times[ID] += now - last_millis;
      digitalWrite(TEAM1_LIGHT, HIGH);
      digitalWrite(TEAM2_LIGHT, LOW);
    }
    if (status == TEAM2) {
      team2_times[ID] += now - last_millis;
      digitalWrite(TEAM2_LIGHT, HIGH);
      digitalWrite(TEAM1_LIGHT, LOW);
    }

    // Broadcast this station's times (rate-limited by sendTimes).
    sendTimes(false);

    // Button debounce — capture state at start of loop cycle.
    bool t1_btn_now = digitalRead(TEAM1_BTN);
    bool t2_btn_now = digitalRead(TEAM2_BTN);

    if (team1_btn_last != t1_btn_now && team1_btn_last == LOW) status = TEAM1;
    if (team2_btn_last != t2_btn_now && team2_btn_last == LOW) status = TEAM2;

    team1_btn_last = t1_btn_now;
    team2_btn_last = t2_btn_now;

    game_time = max(game_time - (now - last_millis), 0L);
    if (game_time <= 0) {
      endGame();
      sendTimes(true);
    }

    // Blinking in NEUTRAL mode.
    if (status == NEUTRAL && now - last_blink_millis >= 1000) {
      blink_last = !blink_last;
      digitalWrite(TEAM1_LIGHT, !blink_last);
      digitalWrite(TEAM2_LIGHT, !blink_last);
      last_blink_millis = now;
    }

  } else if (status == PREP || status == PAUSED) {
    bool t1_btn_now = digitalRead(TEAM1_BTN);
    bool t2_btn_now = digitalRead(TEAM2_BTN);
    team1_btn_last = t1_btn_now;
    team2_btn_last = t2_btn_now;  // keep debounce vars current

    if (status == PREP) {
      prep_time = max(prep_time - (now - last_millis), 0L);
      if (prep_time <= 0) status = NEUTRAL;
    }
    if (status == PAUSED && now - last_blink_millis >= 1000) {
      blink_last = !blink_last;
      digitalWrite(TEAM1_LIGHT, !blink_last);
      digitalWrite(TEAM2_LIGHT, !blink_last);
      last_blink_millis = now;
    }

  } else {
    // START or END state — lights off, read buttons to keep vars fresh.
    digitalWrite(TEAM1_LIGHT, LOW);
    digitalWrite(TEAM2_LIGHT, LOW);
    team1_btn_last = digitalRead(TEAM1_BTN);
    team2_btn_last = digitalRead(TEAM2_BTN);
  }

  last_millis = millis();
  delay(50);
}
