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

// ─── Button debouncing (for ISR) ──────────────────────────────────────────────
volatile unsigned long team1_btn_last_time = 0;
volatile unsigned long team2_btn_last_time = 0;
volatile bool team1_btn_released = true;  // Track if button has been released since last press
volatile bool team2_btn_released = true;

// ─── Interrupt Service Routines ───────────────────────────────────────────────
void IRAM_ATTR team1ButtonISR() {
  unsigned long now = millis();
  
  // Only accept a press if button was previously released
  if (!team1_btn_released) {
    return;  // Ignore bounces while button is already held
  }
  
  // Debounce check: ensure enough time has passed since last valid press
  if (now - team1_btn_last_time < DEBOUNCE_DELAY) {
    return;
  }
  
  // Hardware state verification: confirm button is actually LOW
  if (digitalRead(TEAM1_BTN) != LOW) {
    return;  // False trigger (noise spike), ignore it
  }
  
  Serial.println("Team 1 button pressed");
  team1_btn_released = false;
  team1_btn_last_time = now;
  
  // Only change status if in a gameplay state
  if (status == TEAM1 || status == TEAM2 || status == NEUTRAL) {
    status = TEAM1;
  }
}

void IRAM_ATTR team2ButtonISR() {
  unsigned long now = millis();
  
  // Only accept a press if button was previously released
  if (!team2_btn_released) {
    return;  // Ignore bounces while button is already held
  }
  
  // Debounce check: ensure enough time has passed since last valid press
  if (now - team2_btn_last_time < DEBOUNCE_DELAY) {
    return;
  }
  
  // Hardware state verification: confirm button is actually LOW
  if (digitalRead(TEAM2_BTN) != LOW) {
    return;  // False trigger (noise spike), ignore it
  }
  
  Serial.println("Team 2 button pressed");
  team2_btn_released = false;
  team2_btn_last_time = now;
  
  // Only change status if in a gameplay state
  if (status == TEAM1 || status == TEAM2 || status == NEUTRAL) {
    status = TEAM2;
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  setup()
// ═══════════════════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  Serial.println("Starting up...");

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
  Serial.println("Game state initialized");

  // ── LoRaMesher (replaces RF24 / RF24Network) ───────────────────────────────
  setupLoraMesher();
  Serial.println("LoRaMesher initialized");

  // ── Wi-Fi AP + web server ──────────────────────────────────────────────────
  setupWiFi();
  Serial.println("Wi-Fi AP initialized");
  setupWebServer();
  Serial.println("Web server initialized");

  last_millis = millis();
  last_sent   = millis();

  // ── Attach button interrupts ───────────────────────────────────────────────
  attachInterrupt(digitalPinToInterrupt(TEAM1_BTN), team1ButtonISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(TEAM2_BTN), team2ButtonISR, FALLING);
  // Note: Release detection requires polling in main loop or additional ISRs
  // For now, using FALLING only with state tracking to prevent double-triggers

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

  // ── Button release detection (re-arm ISR for next press) ──────────────────
  if (!team1_btn_released && digitalRead(TEAM1_BTN) == HIGH) {
    team1_btn_released = true;
  }
  if (!team2_btn_released && digitalRead(TEAM2_BTN) == HIGH) {
    team2_btn_released = true;
  }

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
    // START or END state — lights off.
    digitalWrite(TEAM1_LIGHT, LOW);
    digitalWrite(TEAM2_LIGHT, LOW);
  }

  last_millis = millis();
  sendGameEventIfDue(now);
  delay(50);
}
