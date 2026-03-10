#ifndef LORA_HANDLER_H
#define LORA_HANDLER_H

#include <Arduino.h>
#include "LoraMesher.h"
#include "config.h"
#include "game_state.h"

// ─── LoRaMesher instance ──────────────────────────────────────────────────────
extern LoraMesher& radio;
extern TaskHandle_t receiveLoRaMessage_Handle;

// ─── LoRaMesher functions ─────────────────────────────────────────────────────
void sendMessage(MsgType type, const void *payload, size_t payloadSize);
void startStations(long prep_time_ms, long game_time_ms);
void stopStations();
void togglePauseStations(bool pause);
void sendTimes(bool immediate);
void processReceivedPackets(void *);
void createReceiveMessages();
void setupLoraMesher();

#endif // LORA_HANDLER_H
