#include "lora_handler.h"

// ─── LoRaMesher instance ──────────────────────────────────────────────────────
LoraMesher& radio = LoraMesher::getInstance();
TaskHandle_t receiveLoRaMessage_Handle = NULL;

// ═══════════════════════════════════════════════════════════════════════════════
//  LoRaMesher — send helpers
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Send a LoRaGamePacket to all other nodes (broadcast).
 *
 * LoRaMesher routes the broadcast through the mesh so every node receives it
 * regardless of topology.  The function is safe to call from any task or from
 * loop() because LoraMesher::createPacketAndSend() is internally thread-safe.
 */
static void loraSendPacket(const LoRaGamePacket &pkt) {
  radio.createPacketAndSend(BROADCAST_ADDR,
                            const_cast<LoRaGamePacket *>(&pkt), 1);
}

void sendMessage(MsgType type, const void *payload, size_t payloadSize) {
  if (NETWORK_ID == 0) return; // not yet configured — do not transmit
  LoRaGamePacket pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.type   = static_cast<uint8_t>(type);
  pkt.src_id = static_cast<uint8_t>(ID);
  pkt.net_id = static_cast<uint8_t>(NETWORK_ID);
  if (payload && payloadSize) {
    memcpy(&pkt.time_msg, payload, payloadSize); // union — any member works
  }
  loraSendPacket(pkt);
}

// ─── Public game-command wrappers (same API as original) ─────────────────────

void startStations(long prep_time_ms, long game_time_ms) {
  StartMessage m { prep_time_ms, game_time_ms };
  sendMessage(MSG_START, &m, sizeof(m));
  startGame(prep_time_ms, game_time_ms);
}

void stopStations() {
  sendMessage(MSG_END, nullptr, 0);
  endGame();
}

void togglePauseStations(bool pause) {
  PauseMessage m { pause };
  sendMessage(MSG_PAUSE, &m, sizeof(m));
  pauseGame(pause);
}

/**
 * @brief Broadcast this station's current team times over LoRa.
 *
 * Called from loop() on a timed interval or immediately on game-end
 * (all nodes).  Every node collects these broadcasts via processReceivedPackets
 * and stores them in team1_times / team2_times.
 *
 * @param immediate  true → send right now; false → send only if SYNC_INTERVAL
 *                   has elapsed since last transmission.
 */
void sendTimes(bool immediate) {
  if (immediate || (millis() - last_sent >= SYNC_INTERVAL)) {
    TimeMessage m { team1_times[ID], team2_times[ID], status == END };
    sendMessage(MSG_TIME, &m, sizeof(m));
    last_sent = millis();
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  LoRaMesher — receive task
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief FreeRTOS task — woken by LoRaMesher whenever an app-layer packet
 *        arrives.
 *
 *        All nodes are peers: every node acts on Start/Pause/End commands it
 *        receives from any other node on the same network, and every node
 *        aggregates time updates from all peers.  Packets whose net_id does not
 *        match NETWORK_ID are silently discarded, isolating separate games that
 *        happen to be within radio range of each other.
 */
void processReceivedPackets(void *) {
  for (;;) {
    ulTaskNotifyTake(pdPASS, portMAX_DELAY);

    while (radio.getReceivedQueueSize() > 0) {
      AppPacket<LoRaGamePacket> *packet =
          radio.getNextAppPacket<LoRaGamePacket>();

      if (!packet) continue;

      LoRaGamePacket *data     = packet->payload;
      size_t          pktCount = packet->getPayloadLength();

      for (size_t i = 0; i < pktCount; ++i) {
        LoRaGamePacket &p = data[i];

        // ── Network isolation ─────────────────────────────────────────────
        // Drop packets that belong to a different game network, or that
        // arrive before this node has been assigned a network ID.
        if (NETWORK_ID == 0 || p.net_id != static_cast<uint8_t>(NETWORK_ID)) {
          continue;
        }

        int from_id = static_cast<int>(p.src_id);
        Serial.printf("[LoRa] type=%c from=%d net=%d\n",
                      static_cast<char>(p.type), from_id, p.net_id);

        switch (static_cast<MsgType>(p.type)) {

          // ── Time update from a peer station ──────────────────────────────
          // Every node maintains the full times table so any node's web UI
          // shows the aggregated scores of the whole game.
          case MSG_TIME: {
            if (from_id >= 1 && from_id <= STATIONS_COUNT) {
              team1_times[from_id] = p.time_msg.team1_time;
              team2_times[from_id] = p.time_msg.team2_time;
              Serial.printf("[LoRa] Station %d times: %ld / %ld\n",
                            from_id,
                            p.time_msg.team1_time,
                            p.time_msg.team2_time);
            }
            break;
          }

          // ── Start command from any peer ───────────────────────────────────
          case MSG_START: {
            startGame(p.start_msg.prep_time, p.start_msg.game_time);
            break;
          }

          // ── Pause/unpause command from any peer ───────────────────────────
          case MSG_PAUSE: {
            pauseGame(p.pause_msg.pause);
            break;
          }

          // ── End command from any peer ─────────────────────────────────────
          case MSG_END: {
            endGame();
            sendTimes(true); // Broadcast final times immediately.
            break;
          }

          default:
            Serial.printf("[LoRa] Unknown packet type: 0x%02X\n", p.type);
            break;
        }
      }

      radio.deletePacket(packet);
    }
  }
}

// ─── Receive task creation ────────────────────────────────────────────────────

void createReceiveMessages() {
  int res = xTaskCreate(
      processReceivedPackets,
      "LoRa Recv Task",
      4096,
      (void *) 1,
      2,
      &receiveLoRaMessage_Handle);
  if (res != pdPASS) {
    Serial.printf("[LoRa] Receive task creation error: %d\n", res);
  }
}

// ─── LoRaMesher initialisation ────────────────────────────────────────────────

void setupLoraMesher() {
  LoraMesher::LoraMesherConfig config;

  config.loraCs  = NSS_PIN;
  config.loraRst = NRST_PIN;
  config.loraIo1 = DIO1_PIN;
  config.loraIrq = BUSY_PIN;
  config.module  = LoraMesher::LoraModules::SX1280_MOD;

  pinMode(TX_EN, OUTPUT);
  pinMode(RX_EN, OUTPUT);
  digitalWrite(TX_EN, LOW);  // Enable TX PA
  digitalWrite(RX_EN, HIGH); // Enable RX LNA

  radio.begin(config);

  createReceiveMessages();
  radio.setReceiveAppDataTaskHandle(receiveLoRaMessage_Handle);
  radio.start();

  Serial.println("[LoRa] LoRaMesher initialized");
}
