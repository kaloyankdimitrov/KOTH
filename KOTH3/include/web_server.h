#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include "config.h"

// ─── Web server instance ──────────────────────────────────────────────────────
extern AsyncWebServer server;

// ─── Web server functions ─────────────────────────────────────────────────────
void setupWiFi();
void setupWebServer();

#endif // WEB_SERVER_H
