#include "utils.h"
#include "game_state.h"
#include "html_templates.h"

String formatTime(long time, bool milliseconds) {
  String res = "";
  if (time / 6000 < 10) res += 0;
  res += time / 60000;
  res += ":";
  if ((time / 1000) % 60 < 10) res += 0;
  res += (time / 1000) % 60;
  if (milliseconds) {
    res += ".";
    if (time % 1000 < 100) res += 0;
    if (time % 1000 < 10)  res += 0;
    res += time % 1000;
  }
  return res;
}

long sumTimes(long times[STATIONS_COUNT + 1]) {
  long sum = 0;
  for (int i = 1; i <= STATIONS_COUNT; ++i) {
    if (times[i] != -1) sum += times[i];
  }
  return sum;
}

void pastTimesCell(char *pastTimes, long results[5]) {
  long sum = 0;
  for (int i = 0; i < STATIONS_COUNT; ++i) {
    if (results[i] != -1) {
      sum += results[i];
      snprintf_P(pastTimes + strlen(pastTimes),
                 (PAST_TIMES_BUFFER_LEN - strlen(pastTimes)) * sizeof(char),
                 past_times_table_row_station_html, i + 1,
                 formatTime(results[i], true).c_str());
    }
  }
  snprintf_P(pastTimes + strlen(pastTimes),
             (PAST_TIMES_BUFFER_LEN - strlen(pastTimes)) * sizeof(char),
             past_times_table_row_total_html,
             formatTime(sum, true).c_str());
}

String processor(const String &var) {
  if (var == "Time1")     return formatTime(sumTimes(team1_times), true);
  if (var == "Time2")     return formatTime(sumTimes(team2_times), true);
  if (var == "TimeP")     return formatTime(prep_time, false);
  if (var == "TimeG")     return formatTime(game_time, false);
  if (var == "StationID") return String(ID);
  if (var == "NetworkID") return NETWORK_ID == 0 ? "Not set" : String(NETWORK_ID);

  if (var == "CurrentTimes") {
    char times[CURRENT_TIMES_BUFFER_LEN];
    times[0] = 0;
    long sum1 = 0, sum2 = 0;
    for (int i = 1; i <= STATIONS_COUNT; ++i) {
      if (team1_times[i] != -1 && team2_times[i] != -1) {
        sum1 += team1_times[i];
        sum2 += team2_times[i];
        snprintf_P(times + strlen(times),
                   (CURRENT_TIMES_BUFFER_LEN - strlen(times)) * sizeof(char),
                   current_stations_table_row_html, i,
                   formatTime(team1_times[i], true).c_str(),
                   formatTime(team2_times[i], true).c_str());
      }
    }
    snprintf_P(times + strlen(times),
               (CURRENT_TIMES_BUFFER_LEN - strlen(times)) * sizeof(char),
               current_stations_table_total_row_html,
               formatTime(sum1, true).c_str(),
               formatTime(sum2, true).c_str());
    return String(times);
  }

  if (var == "PastTimes") {
    char pastTimes[PAST_TIMES_BUFFER_LEN];
    pastTimes[0] = 0;
    while (writingResults);
    long team1_results[HISTORY_LEN][STATIONS_COUNT];
    long team2_results[HISTORY_LEN][STATIONS_COUNT];
    preferences.begin(RESULTS_NAMESPACE, PREF_RO);
    preferences.getBytes("team1", team1_results, sizeof(team1_results));
    preferences.getBytes("team2", team2_results, sizeof(team2_results));
    preferences.end();
    for (int i = 0; i < HISTORY_LEN; ++i) {
      snprintf_P(pastTimes + strlen(pastTimes),
                 (PAST_TIMES_BUFFER_LEN - strlen(pastTimes)) * sizeof(char),
                 past_times_table_row_start_html);
      pastTimesCell(pastTimes, team1_results[i]);
      snprintf_P(pastTimes + strlen(pastTimes),
                 (PAST_TIMES_BUFFER_LEN - strlen(pastTimes)) * sizeof(char),
                 past_times_table_row_middle_html);
      pastTimesCell(pastTimes, team2_results[i]);
      snprintf_P(pastTimes + strlen(pastTimes),
                 (PAST_TIMES_BUFFER_LEN - strlen(pastTimes)) * sizeof(char),
                 past_times_table_row_end_html);
    }
    return String(pastTimes);
  }

  if (var == "Winner") {
    return sumTimes(team1_times) == sumTimes(team2_times)
      ? "Draw"
      : (sumTimes(team1_times) > sumTimes(team2_times) ? "Team Red" : "Team Blue");
  }

  if (var == "Status") {
    switch (status) {
      case PAUSED:  return "Paused";
      case PREP:    return "Preparation phase.";
      case TEAM1:   return "Team Red control";
      case TEAM2:   return "Team Blue control";
      case NEUTRAL: return "NEUTRAL";
      case END:     return "Ended. Refresh!";
      default:      return "Error";
    }
  }
  if (var == "") return "%";
  return String();
}
