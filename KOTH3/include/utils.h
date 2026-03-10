#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include "config.h"

// ─── Helper utility functions ─────────────────────────────────────────────────
String formatTime(long time, bool milliseconds);
long sumTimes(long times[STATIONS_COUNT + 1]);
void pastTimesCell(char *pastTimes, long results[5]);
String processor(const String &var);

#endif // UTILS_H
