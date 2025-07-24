#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include <Preferences.h>
#include <SPI.h>
#include <RF24.h>
#include <RF24Network.h>

// change when uploading to each station
const int ID = 03;

const int HISTORY_LEN = 5;
const int STATIONS_COUNT = 5;

const int CURRENT_TIMES_BUFFER_LEN = 750;
const int PAST_TIMES_BUFFER_LEN = 5000;

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
Preferences preferences;
static const bool PREF_RO = true;
static const bool PREF_RW = false;
static const char *PREFERENCES_NAMESPACE = "preferences";
static const char *RESULTS_NAMESPACE = "results";

int MASTER_ID = -1;
const int TEAM1_LIGHT = 8;
const int TEAM2_LIGHT = 7;
const int TEAM1_BTN = 21;
const int TEAM2_BTN = 10;
bool team1_btn_last = HIGH;
bool team2_btn_last = HIGH;
int last_blink_millis = 0;
bool blink_last = LOW;
bool record = false;
String ssid = String("P") + ID;
String password = ssid + "password";
String Stations = "";
bool writingResults = false;
const IPAddress local_IP(10, 0, 0, 10);
const IPAddress gateway(0, 0, 0, 0);
const IPAddress subnet(255, 255, 255, 0);

AsyncWebServer server(80);
RF24 radio(20, 9);  // CE, CSN
RF24Network network(radio);
enum {
  START,
  PAUSED,
  PREP,
  NEUTRAL,
  TEAM1,
  TEAM2,
  END
};
int status = START;
int prev_status = START;
long prep_time = 0;
long game_time = 0;
long team1_times[STATIONS_COUNT + 1];
long team2_times[STATIONS_COUNT + 1];
long last_millis = millis();
long last_sent = millis();

const char *setup_html = R"(
<!DOCTYPE html>
<html>
  <head>
    <title>King of the Hill</title>
    <style>
        /* General Styling */
        body {
            font-family: 'Segoe UI', sans-serif;
            background: #f7f7f7;
            /* Light neutral background */
            color: #333;
            margin: 0;
            padding: 2%%;
            display: flex;
            flex-direction: column;
            align-items: center;
        }

        /* Title Styling */
        .page-title {
            font-size: 2.5rem; /* Slightly smaller title for better fit */
            font-weight: bold;
            text-align: center;
            margin-bottom: 0.5em; /* Reduced space between title and first card */
            letter-spacing: 2px;
            /* Slight letter spacing for readability */
        }

        /* Style for individual title letters */
        .title-letter {
            display: inline-block;
            transition: color 0.5s ease-in-out;
        }

        /* Form and Info Block Container */
        form,
        .info-block {
            max-width: 600px;
            /* Increased card width */
            width: 100%%;
            padding: 1.5em;
            margin: 1em 0;
            border-radius: 8px;
            box-shadow: 0 2px 6px rgba(0, 0, 0, 0.1);
            background: white;
            box-sizing: border-box;
        }

        /* Input, select, label, and button styles */
        label,
        input,
        select,
        button {
            font-size: 1.2rem;
            margin: 0.5em 0;
            width: 100%%;
            box-sizing: border-box;
            /* Ensures no overflow */
        }

        input,
        select {
            padding: 0.5em;
            border: 1px solid #ccc;
            border-radius: 4px;
        }

        button {
            background-color: #ff9800;
            color: white;
            border: none;
            padding: 0.7em;
            border-radius: 4px;
            cursor: pointer;
            transition: background-color 0.2s ease;
        }

        button:active {
            background-color: #f57c00;
        }

        /* Table inside the info-block */
        .info-block table {
            width: 100%%;
            table-layout: fixed;
            /* Makes each column equal width */
            border-collapse: collapse;
        }

        .info-block th {
            text-align: center;
            /* Center-align column headers */
            border: 1px solid #ddd;
            padding: 0.5em;
            background-color: #f9f9f9;
            color: #333;
        }

        .info-block td {
            border: 1px solid #ddd;
            padding: 0.5em;
            text-align: center;
            /* Center-align cells by default */
            font-size: 1rem;
        }

        .info-block td div {
            display: inline-block;
            /* Prevent div from stretching */
            text-align: right;
            /* Right-align content within the div */
            width: 100%%;
            /* Ensure it takes full width */
        }

        .info-block td div span.sub-time {
            display: block;
            /* Make sub-time appear on a new line */
            font-size: 0.85rem;
            color: #555;
        }

        .info-block tr:nth-child(even) {
            background-color: #f5f5f5;
        }

        /* Formatting for sub-time */
        .sub-time {
            font-size: 0.85rem;
            color: #555;
        }

        .info-block p {
            font-size: 1rem;
            margin: 0.5em 0;
            text-align: center;
        }

        /* Responsive Styling */
        @media (max-width: 640px) {
            .page-title {
                font-size: 2rem; /* Adjust title size for small screens */
                margin-bottom: 0.3em; /* Reduce space between title and form */
            }

            form,
            .info-block {
                max-width: 90%%;
                /* Card width adjustment for smaller screens */
                padding: 1em;
                /* Adjust padding */
                margin-top: 0.5em; /* Reduced space at the top of the form */
            }

            label,
            input,
            select,
            button {
                font-size: 1rem;
                /* Smaller font size for mobile */
            }

            .info-block th,
            .info-block td {
                font-size: 0.95rem;
                /* Adjust font size in table */
            }

            .sub-time {
                font-size: 0.75rem;
                /* Adjust sub-time font size */
            }

            button {
                width: 100%%;
                /* Full-width buttons on mobile */
            }
        }
    </style>
  </head>
  <body>
    <h1 class="page-title">
        <span class="title-letter" style="color: #ff5722;">K</span>
        <span class="title-letter" style="color: #9c27b0;">i</span>
        <span class="title-letter" style="color: #4caf50;">n</span>
        <span class="title-letter" style="color: #ff9800;">g</span>
        <span class="title-letter" style="color: #0288d1;"> </span> <!-- Space between words -->
        <span class="title-letter" style="color: #ff5722;">o</span>
        <span class="title-letter" style="color: #9c27b0;">f</span>
        <span class="title-letter" style="color: #4caf50;"> </span> <!-- Space between words -->
        <span class="title-letter" style="color: #ff9800;">t</span>
        <span class="title-letter" style="color: #0288d1;">h</span>
        <span class="title-letter" style="color: #ff5722;">e</span>
        <span class="title-letter" style="color: #9c27b0;"> </span> <!-- Space between words -->
        <span class="title-letter" style="color: #4caf50;">H</span>
        <span class="title-letter" style="color: #ff9800;">i</span>
        <span class="title-letter" style="color: #0288d1;">l</span>
        <span class="title-letter" style="color: #ff5722;">l</span>
        <span class="title-letter" style="color: #9c27b0;"> </span> <!-- Space between words -->
        <span class="title-letter" style="color: #4caf50;"></span>
    </h1>

    <form action="/game" method="POST">
        <label for="prep-time">Preparation duration:</label>
        <input type="text" id="prep-time" name="prep-time" />
        <label for="game-time">Game duration:</label>
        <input type="text" id="game-time" name="game-time" />
        <button type="submit">Start</button>
    </form>

    <div class="info-block">
      <p><strong>Station %StationID%</strong></p>
      <table>
          <thead>
              <tr>
                  <th>Team Red Times</th>
                  <th>Team Blue Times</th>
              </tr>
          </thead>
          <tbody>
            %PastTimes%
          </tbody>
      </table>
    </div>

    <form action="/" method="POST">
        <label for="master-id">Master ID: %MasterID%</p>
        <select name="master-id" id="master-id">
            <option value="-1" selected disabled hidden>Choose a master ID:</option>
            <option value="-1">Master</option>
            <option value="1">1</option>
            <option value="2">2</option>
            <option value="3">3</option>
            <option value="4">4</option>
            <option value="5">5</option>
        </select>
        <button type="submit">Select</button>
    </form>
  </body>
</html>
)";

const char *game_html = R"(
<!DOCTYPE html>
<html>
  <head>
    <title>King of the Hill</title>
    <style>
      /* General Styling */
      body {
        font-family: "Segoe UI", sans-serif;
        background: #f7f7f7; /* Light neutral background */
        color: #333;
        margin: 0;
        padding: 2%%;
        display: flex;
        flex-direction: column;
        align-items: center;
      }

      /* Title Styling */
      h3 {
        font-size: 2rem; /* Slightly smaller title for better fit */
        font-weight: bold;
        text-align: center;
        margin: 0 0 0.5em 0; /* Adjusted margin to reduce space */
        letter-spacing: 2px; /* Slight letter spacing for readability */
      }

      /* Title letters with different colors */
      h3 span {
        display: inline-block;
        transition: color 0.5s ease-in-out;
      }

      /* Styling for status text */
      p {
        font-size: 1.2rem;
        margin: 0.5em 0;
        text-align: center;
      }

      /* Button styling */
      button {
        background-color: #ff9800;
        color: white;
        border: none;
        padding: 0.7em;
        border-radius: 4px;
        cursor: pointer;
        transition: background-color 0.2s ease;
        margin: 1em;
        font-size: 1.2rem;
      }

      button:hover {
        background-color: #f57c00;
      }

      /* Form container */
      div {
        display: flex;
        justify-content: center;
        align-items: center;
        flex-direction: column;
        width: 100%%;
      }

      /* Info section (card style) */
      .info-section {
        max-width: 600px; /* Set a max width for the card */
        width: 100%%;
        padding: 2em;
        margin-top: 1.5em; /* Reduced margin-top to reduce space */
        background-color: white;
        border-radius: 8px;
        box-shadow: 0 2px 6px rgba(0, 0, 0, 0.1);
        display: flex;
        flex-direction: column;
        align-items: center;
      }

      /* Table for stations */
      .stations-table {
        width: 100%%;
        table-layout: fixed; /* Equal column width */
        border-collapse: collapse;
        margin-top: 1em;
      }

      .stations-table th,
      .stations-table td {
        padding: 1em;
        text-align: right; /* Right-align all cells */
        border: 1px solid #ddd;
        font-size: 1rem;
      }

      .stations-table th {
        background-color: #f9f9f9;
        color: #333;
      }

      .stations-table tr:nth-child(even) {
        background-color: #f5f5f5;
      }

      .stations-table tr:last-child {
        font-weight: bold;
      }

      /* Table for Prep and Game Time */
      .time-table {
        width: 100%%;
        max-width: 400px;
        table-layout: fixed;
        margin-top: 1em;
        border-collapse: collapse;
      }

      .time-table td {
        padding: 0.8em;
        text-align: right; /* Right-align cells */
        border: 1px solid #ddd;
        font-size: 1rem;
      }

      .time-table th {
        text-align: center;
        padding: 0.8em;
        background-color: #f9f9f9;
        color: #333;
      }

      /* Responsive Styling */
      @media (max-width: 640px) {
        button {
          width: 100%%; /* Full-width buttons on mobile */
        }

        /* Adjust card layout for small screens */
        .info-section {
          max-width: 90%%; /* Reduce card width for small screens */
          padding: 1.5em; /* Adjust padding */
          margin-top: 1em; /* Adjust top margin for smaller devices */
        }

        p {
          font-size: 1rem; /* Adjust text size on smaller screens */
        }

        button {
          font-size: 1rem; /* Adjust button size */
        }

        .stations-table th,
        .stations-table td {
          font-size: 0.95rem; /* Adjust font size for table */
        }
      }
    </style>
  </head>
  <body>
    <!-- Title: King of the Hill with colorful letters -->
    <h3>
      <span style="color: #ff5722">K</span>
      <span style="color: #9c27b0">i</span>
      <span style="color: #4caf50">n</span>
      <span style="color: #ff9800">g</span>
      <span style="color: #0288d1"> </span>
      <span style="color: #ff5722">o</span>
      <span style="color: #9c27b0">f</span>
      <span style="color: #4caf50"> </span>
      <span style="color: #ff9800">t</span>
      <span style="color: #0288d1">h</span>
      <span style="color: #ff5722">e</span>
      <span style="color: #9c27b0"> </span>
      <span style="color: #4caf50">H</span>
      <span style="color: #ff9800">i</span>
      <span style="color: #0288d1">l</span>
      <span style="color: #ff5722">l</span>
    </h3>

    <!-- Status & Timer Info -->
    <div class="info-section">
      <p><strong>Status: </strong>%Status%</p>

      <!-- Prep and Game Time Table -->
      <table class="time-table">
        <thead>
          <tr>
            <th>Prep Time Left</th>
            <th>Game Time Left</th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <td>%TimeP%</td>
            <td>%TimeG%</td>
          </tr>
        </tbody>
      </table>

      <!-- Stations Table with placeholders for each station's times -->
      <table class="stations-table">
        <thead>
          <tr>
            <th>Station</th>
            <th>Team Red Time(s)</th>
            <th>Team Blue Time(s)</th>
          </tr>
        </thead>
        <tbody>
          %CurrentTimes%
        </tbody>
      </table>
    </div>

    <!-- Button Controls -->
    <div>
      <form action="/" method="POST">
        <button type="submit">Stop</button>
      </form>
      <form action="/game" method="POST">
        <input type="hidden" name="_method" id="_method" value="pause" />
        <button type="submit">Pause/Unpause</button>
      </form>
      <form action="/game" method="GET">
        <button type="submit">Refresh Page</button>
      </form>
    </div>
  </body>
</html>
)";

const char *end_html = R"(
<!DOCTYPE html>
<html>
  <head>
    <title>King of the Hill</title>
    <style>
      body {
        font-family: "Segoe UI", sans-serif;
        background: #f7f7f7;
        color: #333;
        margin: 0;
        padding: 2%%;
        display: flex;
        flex-direction: column;
        align-items: center;
      }

      .page-title {
        font-size: 3rem;
        font-weight: bold;
        text-align: center;
        margin: 0 0 0.5em 0;
        line-height: 1.2;
        letter-spacing: 2px;
        display: flex;
        flex-wrap: wrap;
        justify-content: center;
      }

      .title-word {
        display: inline-block;
        white-space: nowrap;
        margin: 0 0.2em;
      }

      .title-letter {
        display: inline-block;
      }

      .card {
        max-width: 600px;
        width: 100%%;
        padding: 1.5em;
        margin: 1em 0;
        background-color: white;
        border-radius: 8px;
        box-shadow: 0 2px 6px rgba(0, 0, 0, 0.1);
        box-sizing: border-box;
        text-align: center;
      }

      table {
        width: 100%%;
        table-layout: fixed;
        border-collapse: collapse;
        margin-top: 1em;
      }

      th {
        text-align: center;
        padding: 0.5em;
        background-color: #f9f9f9;
        color: #333;
        border: 1px solid #ddd;
      }

      td {
        border: 1px solid #ddd;
        padding: 0.5em;
        text-align: center;
        font-size: 1rem;
      }

      td > div {
        display: inline-block;
        text-align: right;
        width: 100%%;
      }

      /* Styling for status text */
      p {
        font-size: 1.2rem;
        margin: 0.5em 0;
        text-align: center;
      }

      .sub-time {
        display: block;
        font-size: 0.85rem;
        color: #555;
      }

      tr:nth-child(even) {
        background-color: #f5f5f5;
      }
      .stations-table tr:last-child {
        font-weight: bold;
      }

      button {
        background-color: #ff9800;
        color: white;
        border: none;
        padding: 0.7em 1.2em;
        border-radius: 4px;
        font-size: 1.1rem;
        cursor: pointer;
        transition: background-color 0.2s ease;
        margin-top: 1.5em;
      }

      button:hover {
        background-color: #f57c00;
      }

      a {
        text-decoration: none;
        display: inline-block;
      }

      @media (max-width: 640px) {
        .card {
          max-width: 90%%;
          padding: 1em;
        }

        .page-title {
          font-size: 2.2rem;
          margin-bottom: 0.8em;
        }

        td,
        th {
          font-size: 0.95rem;
        }

        .sub-time {
          font-size: 0.75rem;
        }

        button {
          width: 100%%;
          font-size: 1rem;
        }

        p {
          font-size: 1rem; /* Adjust text size on smaller screens */
        }
      }
    </style>
  </head>
  <body>
    <!-- Title -->
    <h1 class="page-title">
      <div class="title-word">
        <span class="title-letter" style="color: #ff5722">K</span>
        <span class="title-letter" style="color: #9c27b0">i</span>
        <span class="title-letter" style="color: #4caf50">n</span>
        <span class="title-letter" style="color: #ff9800">g</span>
      </div>
      <div class="title-word">
        <span class="title-letter" style="color: #0288d1">o</span>
        <span class="title-letter" style="color: #ff5722">f</span>
      </div>
      <div class="title-word">
        <span class="title-letter" style="color: #9c27b0">t</span>
        <span class="title-letter" style="color: #4caf50">h</span>
        <span class="title-letter" style="color: #ff9800">e</span>
      </div>
      <div class="title-word">
        <span class="title-letter" style="color: #0288d1">H</span>
        <span class="title-letter" style="color: #ff5722">i</span>
        <span class="title-letter" style="color: #9c27b0">l</span>
        <span class="title-letter" style="color: #4caf50">l</span>
      </div>
    </h1>

    <!-- Main Info Card -->
    <div class="card">
      <p><strong>Winner:</strong> %Winner%</p>

      <!-- Total and Station Times -->
      <table class="stations-table">
        <thead>
          <tr>
            <th>Station</th>
            <th>Team Red Time(s)</th>
            <th>Team Blue Time(s)</th>
          </tr>
        </thead>
        <tbody>
          %CurrentTimes%
        </tbody>
      </table>

      <!-- Past Times -->
      <table>
        <thead>
          <tr>
            <th>Past Team Red Times</th>
            <th>Past Team Blue Times</th>
          </tr>
        </thead>
        <tbody>
          %PastTimes%
        </tbody>
      </table>

      <a href="/"><button>New Game</button></a>
    </div>
  </body>
</html>
)";

const char *current_stations_table_row_html = PSTR(R"(
<tr>
  <td>Station %d</td>
  <td>%s</td>
  <td>%s</td>
</tr>
)");


const char *current_stations_table_total_row_html = PSTR(R"(
<tr>
  <td><strong>Total</strong></td>
  <td><strong>%s</strong></td>
  <td><strong>%s</strong></td>
</tr>
)");


const char *past_times_table_row_start_html = PSTR(R"(
<tr>
  <td>
    <div>
)");

const char *past_times_table_row_total_html = PSTR(R"(
      <strong>Total: </strong>%s<br />
)");

const char *past_times_table_row_station_html = PSTR(R"(
      <span class="sub-time"><strong>Station %d: </strong>%s</span>
)");

const char *past_times_table_row_middle_html = PSTR(R"(
    </div>
  </td>
  <td>
    <div>
)");

const char *past_times_table_row_end_html = PSTR(R"(
    </div>
  </td>
</tr>
)");

String formatTime(long time, bool miliseconds) {
  String res = "";
  if (time / 6000 < 10) {
    res += 0;
  }
  res += time / 60000;
  res += ":";
  if ((time / 1000) % 60 < 10) {
    res += 0;
  }
  res += (time / 1000) % 60;
  if (miliseconds) {
    res += ".";
    if (time % 1000 < 100) {
      res += 0;
    }
    if (time % 1000 < 10) {
      res += 0;
    }
    res += time % 1000;
  }
  return res;
}

long sumTimes(long times[STATIONS_COUNT + 1]) {
  long sum = 0;
  for (int i = 1; i <= STATIONS_COUNT; ++i) {
    if (times[i] != -1) {
      sum += times[i];
    }
  }
  return sum;
}

void pastTimesCell(char *pastTimes, long results[5]) {
  long sum = 0;
  for (int i = 0; i < STATIONS_COUNT; ++i) {
    if (results[i] != -1) {
      sum += results[i];
      snprintf_P(pastTimes + strlen(pastTimes), (PAST_TIMES_BUFFER_LEN - strlen(pastTimes)) * sizeof(char), past_times_table_row_station_html, i + 1, formatTime(results[i], true));
    }
  }
  snprintf_P(pastTimes + strlen(pastTimes), (PAST_TIMES_BUFFER_LEN - strlen(pastTimes)) * sizeof(char), past_times_table_row_total_html, formatTime(sum, true));
}

String processor(const String &var) {
  if (var == "Time1") {
    return formatTime(sumTimes(team1_times), true);
  }
  if (var == "Time2") {
    return formatTime(sumTimes(team2_times), true);
  }
  if (var == "TimeP") { return formatTime(prep_time, false); }
  if (var == "TimeG") { return formatTime(game_time, false); }
  if (var == "StationID") { return String(ID); }
  if (var == "MasterID") {
    if (MASTER_ID == -1) {
      return "Master";
    }
    return String(MASTER_ID);
  }
  if (var == "CurrentTimes") {
    char times[CURRENT_TIMES_BUFFER_LEN];
    times[0] = 0;
    long sum1 = 0, sum2 = 0;
    for (int i = 1; i <= STATIONS_COUNT; ++i) {
      if (team1_times[i] != -1 && team2_times[i] != -1) {
        sum1 += team1_times[i];
        sum2 += team2_times[i];
        snprintf_P(times + strlen(times), (CURRENT_TIMES_BUFFER_LEN - strlen(times)) * sizeof(char), current_stations_table_row_html, i, formatTime(team1_times[i], true), formatTime(team2_times[i], true));
      }
    }
    snprintf_P(times + strlen(times), (CURRENT_TIMES_BUFFER_LEN - strlen(times)) * sizeof(char), current_stations_table_total_row_html, formatTime(sum1, true), formatTime(sum2, true));
    return String(times);
  }
  if (var == "PastTimes") {
    char pastTimes[PAST_TIMES_BUFFER_LEN];
    pastTimes[0] = 0;
    while (writingResults)
      ;
    long team1_results[HISTORY_LEN][STATIONS_COUNT], team2_results[HISTORY_LEN][STATIONS_COUNT];
    preferences.begin(RESULTS_NAMESPACE, PREF_RO);
    preferences.getBytes("team1", team1_results, sizeof(team1_results));
    preferences.getBytes("team2", team2_results, sizeof(team2_results));
    preferences.end();
    for (int i = 0; i < HISTORY_LEN; ++i) {
      snprintf_P(pastTimes + strlen(pastTimes), (PAST_TIMES_BUFFER_LEN - strlen(pastTimes)) * sizeof(char), past_times_table_row_start_html);
      pastTimesCell(pastTimes, team1_results[i]);
      snprintf_P(pastTimes + strlen(pastTimes), (PAST_TIMES_BUFFER_LEN - strlen(pastTimes)) * sizeof(char), past_times_table_row_middle_html);
      pastTimesCell(pastTimes, team2_results[i]);
      snprintf_P(pastTimes + strlen(pastTimes), (PAST_TIMES_BUFFER_LEN - strlen(pastTimes)) * sizeof(char), past_times_table_row_end_html);
    }
    return String(pastTimes);
  }
  if (var == "Winner") { return sumTimes(team1_times) == sumTimes(team2_times) ? "Draw" : (sumTimes(team1_times) > sumTimes(team2_times) ? "Team Red" : "Team Blue"); }
  if (var == "Status") {
    switch (status) {
      case PAUSED:
        return "Paused";
        break;
      case PREP:
        return "Preparation phase.";
        break;
      case TEAM1:
        return "Team Red control";
        break;
      case TEAM2:
        return "Team Blue control";
        break;
      case NEUTRAL:
        return "NEUTRAL";
        break;
      case END:
        return "Ended. Refresh!";
        break;
      default:
        return "Error";
        break;
    }
  }
  if (var == "") { return "%"; }
  return String();
}

void startGame(long prep_time_ms, long game_time_ms) {
  if (status != PREP && status != NEUTRAL && status != TEAM1 && status != TEAM2) {
    status = PREP;
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


// TODO: Speed up by sending once per loop cycle at most
void sendMessage(const void *data, uint8_t msg_type, size_t size) {
  RF24NetworkHeader header(00, msg_type);
  if (MASTER_ID == -1) {
    // master
    for (int i = 01; i <= STATIONS_COUNT; ++ i) {
      RF24NetworkHeader header(i, msg_type);
      if (!network.write(header, data, size)) {
        Serial.print("Error sending message to station ");
        Serial.println(i);
      }
    }
  } else {
    if (!network.multicast(header, data, size, 0)) {
      Serial.println("Error sending unicast message");
    }
  }
}

void startStations(long prep_time_ms, long game_time_ms) {
  StartMessage start_msg {prep_time_ms, game_time_ms};
  sendMessage(&start_msg, 'S', sizeof(start_msg));
  startGame(prep_time_ms, game_time_ms);
}

void stopStations() {
  sendMessage(NULL, 'E', 0);
  endGame();
  // TODO: Wait for an update from all stations before returning results
}

void togglePauseStations(bool pause) {
  PauseMessage pause_msg = { pause };
  sendMessage(&pause_msg, 'P', sizeof(pause_msg));
  pauseGame(pause);
}

void sendTimes(bool immediate) {
  // slave only
  // if (MASTER_ID != -1 && (immediate || millis() - last_sent >= 2000)) {
  //   TimeMessage time_msg = { team1_times[ID], team2_times[ID], status == END };
  //   sendMessage(&time_msg, 'T', sizeof(time_msg));
  //   last_sent = millis();
  // }
}

void recvRadio() {
  while (network.available()) {
    RF24NetworkHeader header;
    network.peek(header);
    Serial.println(char(header.type));
    Serial.println(header.from_node);
    uint16_t from_node = header.from_node;
    switch (header.type) {
      // time of another station
      case 'T':
        TimeMessage time_msg;
        network.read(header, &time_msg, sizeof(time_msg));
        if (from_node == 0) {
          from_node = MASTER_ID == -1 ? ID : MASTER_ID;
        }
        Serial.print(time_msg.team1_time);
        Serial.print(" ");
        Serial.println(time_msg.team2_time);
        team1_times[from_node] = time_msg.team1_time;
        team2_times[from_node] = time_msg.team2_time;
        break;
      // start game
      case 'S':
        StartMessage start_msg;
        network.read(header, &start_msg, sizeof(start_msg));
        startGame(start_msg.prep_time, start_msg.game_time);
        break;
      // pause game
      case 'P':
        PauseMessage pause_msg;
        network.read(header, &pause_msg, sizeof(pause_msg));
        pauseGame(pause_msg.pause);
        break;
      // end game
      case 'E':
        endGame();
        network.read(header, NULL, 0);
        sendTimes(true);
        break;
      // register slave
      default:
        // prevent buffer fill up
        network.read(header, NULL, 0);
        break;
    }
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

void recordResults() {
  writingResults = true;
  preferences.begin(RESULTS_NAMESPACE, PREF_RO);
  long team1Results[HISTORY_LEN][STATIONS_COUNT], team2Results[HISTORY_LEN][STATIONS_COUNT], team1ResultsNew[HISTORY_LEN][STATIONS_COUNT], team2ResultsNew[HISTORY_LEN][STATIONS_COUNT];
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

void setup() {
  Serial.begin(9600);
  pinMode(TEAM1_LIGHT, OUTPUT);
  pinMode(TEAM2_LIGHT, OUTPUT);
  digitalWrite(TEAM1_LIGHT, LOW);
  digitalWrite(TEAM2_LIGHT, LOW);
  pinMode(TEAM1_BTN, INPUT_PULLUP);
  pinMode(TEAM2_BTN, INPUT_PULLUP);
  Serial.println("Initializing...");
  blinkLightsBlocking(1);
  delay(100);
  // read MASTER_ID from memory and reset it with button combination
  preferences.begin(PREFERENCES_NAMESPACE, PREF_RO);
  MASTER_ID = preferences.getInt("master_id", -1);
  if (MASTER_ID == ID) {
    MASTER_ID = -1;
    preferences.begin(PREFERENCES_NAMESPACE, PREF_RW);
    preferences.putInt("master_id", -1);
    preferences.end();
  }
  preferences.end();
  if (MASTER_ID != -1 && !digitalRead(TEAM1_BTN) && !digitalRead(TEAM2_BTN)) {
    int startTimeHold = millis();
    while (!digitalRead(TEAM1_BTN) && !digitalRead(TEAM2_BTN) && millis() - startTimeHold < 3000)
      ;
    if (startTimeHold >= 3000) {
      MASTER_ID = -1;
      preferences.begin(PREFERENCES_NAMESPACE, PREF_RW);
      preferences.putInt("master_id", -1);
      preferences.end();
      blinkLightsBlocking(2);
    }
  }
  // results memory initialization
  preferences.begin(RESULTS_NAMESPACE, PREF_RW);
  if (preferences.getBytesLength("team1") != HISTORY_LEN * STATIONS_COUNT * sizeof(long) && preferences.getBytesLength("team2") != HISTORY_LEN * STATIONS_COUNT * sizeof(long)) {
    long team1_results[HISTORY_LEN][STATIONS_COUNT], team2_results[HISTORY_LEN][STATIONS_COUNT];
    for (int i = 0; i < HISTORY_LEN; ++i) {
      for (int j = 0; j < STATIONS_COUNT; ++j) {
        team1_results[i][j] = -1;
        team2_results[i][j] = -1;
      }
    }
    preferences.putBytes("team1", &team1_results, sizeof(team1_results));
    preferences.putBytes("team2", &team2_results, sizeof(team2_results));
  }
  preferences.end();
  // current times initialization
  for (int i = 0; i <= STATIONS_COUNT; ++i) {
    team1_times[i] = -1;
    team2_times[i] = -1;
  }

  Serial.print("MASTER_ID: ");
  Serial.println(MASTER_ID);
  radio.begin();
  radio.setPALevel(RF24_PA_MAX, true);
  radio.setDataRate(RF24_250KBPS);
  WiFi.mode(WIFI_AP);
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");
  Serial.println(WiFi.softAP(ssid, password));
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  Serial.println(WiFi.softAPIP());
  // server setup
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (status == START || status == END) {
      request->send_P(200, "text/html", setup_html, processor);
    } else {
      request->send_P(200, "text/html", game_html, processor);
    }
  });
  // change master/stop
  server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
    status = START;
    if (request->hasParam("master-id", true)) {
      preferences.begin(PREFERENCES_NAMESPACE, PREF_RW);
      preferences.putInt("master_id", request->getParam("master-id", true)->value().toInt());
      preferences.end();
      request->redirect("/");
      ESP.restart();
    } else {
      stopStations();
      request->redirect("/game");
    }
  });
  // start; pause/unpause
  server.on("/game", HTTP_POST, [](AsyncWebServerRequest *request) {
    blinkLightsBlocking(1);
    // paused button
    if (request->hasParam("_method", true) && request->getParam("_method", true)->value() == "pause") {
      // pause all stations, including this one
      togglePauseStations((status != PAUSED));
    } else {
      // process prep and game time from incoming request string
      String prep_time_str = request->getParam("prep-time", true)->value();
      String game_time_str = request->getParam("game-time", true)->value();
      int prep_delim = prep_time_str.indexOf(':');
      int game_delim = game_time_str.indexOf(':');
      int prep_mins = prep_delim == -1 ? prep_time_str.toInt() : prep_time_str.substring(0, prep_delim).toInt();
      int game_mins = game_delim == -1 ? game_time_str.toInt() : game_time_str.substring(0, game_delim).toInt();
      int prep_secs = prep_delim == -1 ? 0 : prep_time_str.substring(prep_delim + 1, prep_time_str.length()).toInt();
      int game_secs = game_delim == -1 ? 0 : game_time_str.substring(game_delim + 1, game_time_str.length()).toInt();
      long prep_time_ms = (prep_mins * 60 + prep_secs) * 1000;
      long game_time_ms = (game_mins * 60 + game_secs) * 1000;
      // start all stations, including this one
      startStations(prep_time_ms, game_time_ms);
    }
    request->redirect("/game");
  });
  // end
  server.on("/game", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (status == END) {
      request->send_P(200, "text/html", end_html, processor);
    } else {
      request->send_P(200, "text/html", game_html, processor);
    }
  });
  // end
  server.on("/end", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "texthtml", end_html, processor);
  });

  server.begin();
  blinkLightsBlocking(2);
  if (MASTER_ID != -1) {
    // slave
    network.begin(90 + MASTER_ID * 3, ID);
  } else {
    // master
    network.begin(90 + ID * 3, 00);
  }
  Serial.println("Network ready");
  blinkLightsBlocking(3);
}

void loop() {
  network.update();
  // read from network
  recvRadio();
  if (status == TEAM1 || status == TEAM2 || status == NEUTRAL) {
    if (status == TEAM1) {
      team1_times[ID] += millis() - last_millis;
      digitalWrite(TEAM1_LIGHT, HIGH);
      digitalWrite(TEAM2_LIGHT, LOW);
    }
    if (status == TEAM2) {
      team2_times[ID] += millis() - last_millis;
      digitalWrite(TEAM2_LIGHT, HIGH);
      digitalWrite(TEAM1_LIGHT, LOW);
    }
    sendTimes(false);
    if (team1_btn_last != digitalRead(TEAM1_BTN) && team1_btn_last == LOW) {
      status = TEAM1;
    }
    if (team2_btn_last != digitalRead(TEAM2_BTN) && team2_btn_last == LOW) {
      status = TEAM2;
    }
    game_time = max(game_time - (long(millis()) - last_millis), 0L);
    if (game_time <= 0) {
      endGame();
      sendTimes(true);
    }
    // blinking when neutral
    if (status == NEUTRAL && millis() - last_blink_millis >= 1000) {
      blink_last = !blink_last;
      digitalWrite(TEAM1_LIGHT, !blink_last);
      digitalWrite(TEAM2_LIGHT, !blink_last);
      last_blink_millis = millis();
    }
  } else if (status == PREP || status == PAUSED || status == NEUTRAL) {
    if (status == PREP) {
      prep_time = max(prep_time - (long(millis()) - last_millis), 0L);
    }
    if (prep_time <= 0 && status != PAUSED) { status = NEUTRAL; }
    if (status == PAUSED && millis() - last_blink_millis >= 1000) {
      blink_last = !blink_last;
      digitalWrite(TEAM1_LIGHT, !blink_last);
      digitalWrite(TEAM2_LIGHT, !blink_last);
      last_blink_millis = millis();
    }
  } else {
    digitalWrite(TEAM1_LIGHT, LOW);
    digitalWrite(TEAM2_LIGHT, LOW);
  }
  last_millis = millis();
  team1_btn_last = digitalRead(TEAM1_BTN);
  team2_btn_last = digitalRead(TEAM2_BTN);
  delay(50);
}