#include "game_state.h"

// ─── Game state variables definitions ─────────────────────────────────────────
int  NETWORK_ID = 0;
bool team1_btn_last = HIGH;
bool team2_btn_last = HIGH;
int  last_blink_millis = 0;
bool blink_last  = LOW;
bool record      = false;
bool writingResults = false;

String Stations = "";

Preferences preferences;

int  status      = START;
int  prev_status = START;
long prep_time   = 0;
long game_time   = 0;
long team1_times[STATIONS_COUNT + 1];
long team2_times[STATIONS_COUNT + 1];
long last_millis = 0;
long last_sent   = 0;

// ─── Game logic functions implementations ─────────────────────────────────────

void recordResults() {
  writingResults = true;
  preferences.begin(RESULTS_NAMESPACE, PREF_RO);
  long team1Results[HISTORY_LEN][STATIONS_COUNT];
  long team2Results[HISTORY_LEN][STATIONS_COUNT];
  long team1ResultsNew[HISTORY_LEN][STATIONS_COUNT];
  long team2ResultsNew[HISTORY_LEN][STATIONS_COUNT];
  preferences.getBytes("team1", team1Results, sizeof(team1Results));
  preferences.getBytes("team2", team2Results, sizeof(team2Results));
  for (int i = HISTORY_LEN - 1; i > 0; --i) {
    memcpy(team1ResultsNew[i], team1Results[i - 1], sizeof(team1Results[i - 1]));
    memcpy(team2ResultsNew[i], team2Results[i - 1], sizeof(team2Results[i - 1]));
  }
  preferences.end();
  memcpy(team1ResultsNew[0], team1_times + 1, STATIONS_COUNT * sizeof(long));
  memcpy(team2ResultsNew[0], team2_times + 1, STATIONS_COUNT * sizeof(long));
  preferences.begin(RESULTS_NAMESPACE, PREF_RW);
  preferences.putBytes("team1", &team1ResultsNew, sizeof(team1ResultsNew));
  preferences.putBytes("team2", &team2ResultsNew, sizeof(team2ResultsNew));
  preferences.end();
  writingResults = false;
}

void startGame(long prep_time_ms, long game_time_ms) {
  if (status != PREP && status != NEUTRAL && status != TEAM1 && status != TEAM2) {
    status    = PREP;
    prep_time = prep_time_ms;
    game_time = game_time_ms;
    Serial.print(prep_time_ms);
    Serial.print(" ");
    Serial.println(game_time_ms);
    team1_times[ID] = 0;
    team2_times[ID] = 0;
    last_millis = millis();
  }
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

void blinkLightsBlocking(int count) {
  digitalWrite(TEAM1_LIGHT, LOW);
  digitalWrite(TEAM2_LIGHT, LOW);
  delay(500);
  for (int i = 0; i < count; ++i) {
    digitalWrite(TEAM1_LIGHT, HIGH);
    digitalWrite(TEAM2_LIGHT, HIGH);
    delay(500);
    digitalWrite(TEAM1_LIGHT, LOW);
    digitalWrite(TEAM2_LIGHT, LOW);
    delay(500);
  }
}

void initGameState() {
  // ── Current-game time arrays initialisation ────────────────────────────────
  for (int i = 0; i <= STATIONS_COUNT; ++i) {
    team1_times[i] = -1;
    team2_times[i] = -1;
  }
}
