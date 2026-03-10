#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

// ─── LoRaMesher message types ─────────────────────────────────────────────────
// Single-byte tag carried in every LoRaGamePacket so the receiver knows
// which union member to read.
enum MsgType : uint8_t {
  MSG_TIME  = 'T',
  MSG_START = 'S',
  MSG_PAUSE = 'P',
  MSG_END   = 'E',
};

// ─── Wire-format structs ──────────────────────────────────────────────────────
struct TimeMessage {
  long team1_time;
  long team2_time;
  bool end;
};
struct StartMessage {
  long prep_time;
  long game_time;
};
struct PauseMessage {
  bool pause;
};

// Wrapper sent over LoRaMesher — fixed-size, large enough for any message type.
struct LoRaGamePacket {
  uint8_t  type;          // MsgType
  uint8_t  src_id;        // originating station ID (1-5)
  uint8_t  net_id;        // network ID — packets with a different net_id are ignored
  uint8_t  _pad;          // alignment
  union {
    TimeMessage  time_msg;
    StartMessage start_msg;
    PauseMessage pause_msg;
  };
};

// ─── Game state variables ─────────────────────────────────────────────────────
extern int  NETWORK_ID;
extern bool team1_btn_last;
extern bool team2_btn_last;
extern int  last_blink_millis;
extern bool blink_last;
extern bool record;
extern bool writingResults;

extern String Stations;

extern Preferences preferences;

extern int  status;
extern int  prev_status;
extern long prep_time;
extern long game_time;
extern long team1_times[STATIONS_COUNT + 1];
extern long team2_times[STATIONS_COUNT + 1];
extern long last_millis;
extern long last_sent;

// ─── Game logic functions ─────────────────────────────────────────────────────
void recordResults();
void startGame(long prep_time_ms, long game_time_ms);
void endGame();
void pauseGame(bool pause);
void blinkLightsBlocking(int count);

// ─── Game state initialization ────────────────────────────────────────────────
void initGameState();

#endif // GAME_STATE_H
