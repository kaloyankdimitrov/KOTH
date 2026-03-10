#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <IPAddress.h>

// ─── Station identity ────────────────────────────────────────────────────────
// Change when uploading to each station (1–5).
extern const int ID;

// ─── Game constants ───────────────────────────────────────────────────────────
const int HISTORY_LEN           = 5;
const int STATIONS_COUNT        = 5;
const int CURRENT_TIMES_BUFFER_LEN = 750;
const int PAST_TIMES_BUFFER_LEN    = 5000;

// ─── SX1280 pin assignments ───────────────────────────────────────────────────
// NOTE: BUSY_PIN was originally 21 in mesh_final.cpp but pin 21 is used by
// TEAM1_BTN in this application. Reassign BUSY to GPIO 33 on your hardware.
const int NSS_PIN  = 5;
const int DIO1_PIN = 4;
const int NRST_PIN = 14;
const int BUSY_PIN = 33;   // ← was 21 in mesh_final.cpp; changed to avoid clash with TEAM1_BTN
const int TX_EN    = 26;
const int RX_EN    = 27;

// ─── Game I/O pins ────────────────────────────────────────────────────────────
const int TEAM1_LIGHT = 8;
const int TEAM2_LIGHT = 7;
const int TEAM1_BTN   = 21;
const int TEAM2_BTN   = 10;

// ─── Time-sync interval (ms) ──────────────────────────────────────────────────
// Each station broadcasts its local times every SYNC_INTERVAL ms.
static const uint32_t SYNC_INTERVAL = 2000;

// ─── Preferences / persistent storage ────────────────────────────────────────
static const bool PREF_RO = true;
static const bool PREF_RW = false;
static const char *PREFERENCES_NAMESPACE = "preferences";
static const char *RESULTS_NAMESPACE     = "results";

// ─── Network configuration ────────────────────────────────────────────────────
extern String ssid;
extern String password;
extern const IPAddress local_IP;
extern const IPAddress gateway;
extern const IPAddress subnet;

// ─── Game status enum ─────────────────────────────────────────────────────────
enum GameStatus {
  START,
  PAUSED,
  PREP,
  NEUTRAL,
  TEAM1,
  TEAM2,
  END
};

#endif // CONFIG_H
