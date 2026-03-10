#include "html_templates.h"

const char *setup_html = R"(
<!DOCTYPE html>
<html>
  <head>
    <title>King of the Hill</title>
    <style>
        body {
            font-family: 'Segoe UI', sans-serif;
            background: #f7f7f7;
            color: #333;
            margin: 0;
            padding: 2%%;
            display: flex;
            flex-direction: column;
            align-items: center;
        }
        .page-title {
            font-size: 2.5rem;
            font-weight: bold;
            text-align: center;
            margin-bottom: 0.5em;
            letter-spacing: 2px;
        }
        .title-letter {
            display: inline-block;
            transition: color 0.5s ease-in-out;
        }
        form, .info-block {
            max-width: 600px;
            width: 100%%;
            padding: 1.5em;
            margin: 1em 0;
            border-radius: 8px;
            box-shadow: 0 2px 6px rgba(0,0,0,0.1);
            background: white;
            box-sizing: border-box;
        }
        label, input, select, button {
            font-size: 1.2rem;
            margin: 0.5em 0;
            width: 100%%;
            box-sizing: border-box;
        }
        input, select {
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
        button:active { background-color: #f57c00; }
        .info-block table {
            width: 100%%;
            table-layout: fixed;
            border-collapse: collapse;
        }
        .info-block th {
            text-align: center;
            border: 1px solid #ddd;
            padding: 0.5em;
            background-color: #f9f9f9;
            color: #333;
        }
        .info-block td {
            border: 1px solid #ddd;
            padding: 0.5em;
            text-align: center;
            font-size: 1rem;
        }
        .info-block td div {
            display: inline-block;
            text-align: right;
            width: 100%%;
        }
        .info-block td div span.sub-time {
            display: block;
            font-size: 0.85rem;
            color: #555;
        }
        .info-block tr:nth-child(even) { background-color: #f5f5f5; }
        .sub-time { font-size: 0.85rem; color: #555; }
        .info-block p { font-size: 1rem; margin: 0.5em 0; text-align: center; }
        @media (max-width: 640px) {
            .page-title { font-size: 2rem; margin-bottom: 0.3em; }
            form, .info-block { max-width: 90%%; padding: 1em; margin-top: 0.5em; }
            label, input, select, button { font-size: 1rem; }
            .info-block th, .info-block td { font-size: 0.95rem; }
            .sub-time { font-size: 0.75rem; }
            button { width: 100%%; }
        }
    </style>
  </head>
  <body>
    <h1 class="page-title">
        <span class="title-letter" style="color:#ff5722;">K</span>
        <span class="title-letter" style="color:#9c27b0;">i</span>
        <span class="title-letter" style="color:#4caf50;">n</span>
        <span class="title-letter" style="color:#ff9800;">g</span>
        <span class="title-letter" style="color:#0288d1;"> </span>
        <span class="title-letter" style="color:#ff5722;">o</span>
        <span class="title-letter" style="color:#9c27b0;">f</span>
        <span class="title-letter" style="color:#4caf50;"> </span>
        <span class="title-letter" style="color:#ff9800;">t</span>
        <span class="title-letter" style="color:#0288d1;">h</span>
        <span class="title-letter" style="color:#ff5722;">e</span>
        <span class="title-letter" style="color:#9c27b0;"> </span>
        <span class="title-letter" style="color:#4caf50;">H</span>
        <span class="title-letter" style="color:#ff9800;">i</span>
        <span class="title-letter" style="color:#0288d1;">l</span>
        <span class="title-letter" style="color:#ff5722;">l</span>
        <span class="title-letter" style="color:#9c27b0;"></span>
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
        <label for="network-id">Network ID: <strong>%NetworkID%</strong></label>
        <input type="number" id="network-id" name="network-id" min="1" max="255"
               placeholder="Enter network ID (1–255) " />
        <button type="submit">Set Network ID</button>
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
      h3 {
        font-size: 2rem;
        font-weight: bold;
        text-align: center;
        margin: 0 0 0.5em 0;
        letter-spacing: 2px;
      }
      h3 ...

span { display: inline-block; transition: color 0.5s ease-in-out; }
      p { font-size: 1.2rem; margin: 0.5em 0; text-align: center; }
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
      button:hover { background-color: #f57c00; }
      div {
        display: flex;
        justify-content: center;
        align-items: center;
        flex-direction: column;
        width: 100%%;
      }
      .info-section {
        max-width: 600px;
        width: 100%%;
        padding: 2em;
        margin-top: 1.5em;
        background-color: white;
        border-radius: 8px;
        box-shadow: 0 2px 6px rgba(0,0,0,0.1);
        display: flex;
        flex-direction: column;
        align-items: center;
      }
      .stations-table {
        width: 100%%;
        table-layout: fixed;
        border-collapse: collapse;
        margin-top: 1em;
      }
      .stations-table th, .stations-table td {
        padding: 1em;
        text-align: right;
        border: 1px solid #ddd;
        font-size: 1rem;
      }
      .stations-table th { background-color: #f9f9f9; color: #333; }
      .stations-table tr:nth-child(even) { background-color: #f5f5f5; }
      .stations-table tr:last-child { font-weight: bold; }
      .time-table {
        width: 100%%;
        max-width: 400px;
        table-layout: fixed;
        margin-top: 1em;
        border-collapse: collapse;
      }
      .time-table td { padding: 0.8em; text-align: right; border: 1px solid #ddd; font-size: 1rem; }
      .time-table th { text-align: center; padding: 0.8em; background-color: #f9f9f9; color: #333; }
      @media (max-width: 640px) {
        button { width: 100%%; }
        .info-section { max-width: 90%%; padding: 1.5em; margin-top: 1em; }
        p { font-size: 1rem; }
        button { font-size: 1rem; }
        .stations-table th, .stations-table td { font-size: 0.95rem; }
      }
    </style>
  </head>
  <body>
    <h3>
      <span style="color:#ff5722">K</span><span style="color:#9c27b0">i</span>
      <span style="color:#4caf50">n</span><span style="color:#ff9800">g</span>
      <span style="color:#0288d1"> </span><span style="color:#ff5722">o</span>
      <span style="color:#9c27b0">f</span><span style="color:#4caf50"> </span>
      <span style="color:#ff9800">t</span><span style="color:#0288d1">h</span>
      <span style="color:#ff5722">e</span><span style="color:#9c27b0"> </span>
      <span style="color:#4caf50">H</span><span style="color:#ff9800">i</span>
      <span style="color:#0288d1">l</span><span style="color:#ff5722">l</span>
    </h3>

    <div class="info-section">
      <p><strong>Status: </strong>%Status%</p>
      <table class="time-table">
        <thead><tr><th>Prep Time Left</th><th>Game Time Left</th></tr></thead>
        <tbody><tr><td>%TimeP%</td><td>%TimeG%</td></tr></tbody>
      </table>
      <table class="stations-table">
        <thead>
          <tr><th>Station</th><th>Team Red Time(s)</th><th>Team Blue Time(s)</th></tr>
        </thead>
        <tbody>%CurrentTimes%</tbody>
      </table>
    </div>

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
      .title-word  { display: inline-block; white-space: nowrap; margin: 0 0.2em; }
      .title-letter { display: inline-block; }
      .card {
        max-width: 600px;
        width: 100%%;
        padding: 1.5em;
        margin: 1em 0;
        background-color: white;
        border-radius: 8px;
        box-shadow: 0 2px 6px rgba(0,0,0,0.1);
        box-sizing: border-box;
        text-align: center;
      }
      table { width: 100%%; table-layout: fixed; border-collapse: collapse; margin-top: 1em; }
      th { text-align: center; padding: 0.5em; background-color: #f9f9f9; color: #333; border: 1px solid #ddd; }
      td { border: 1px solid #ddd; padding: 0.5em; text-align: center; font-size: 1rem; }
      td > div { display: inline-block; text-align: right; width: 100%%; }
      p { font-size: 1.2rem; margin: 0.5em 0; text-align: center; }
      .sub-time { display: block; font-size: 0.85rem; color: #555; }
      tr:nth-child(even) { background-color: #f5f5f5; }
      .stations-table tr:last-child { font-weight: bold; }
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
      button:hover { background-color: #f57c00; }
      a { text-decoration: none; display: inline-block; }
      @media (max-width: 640px) {
        .card { max-width: 90%%; padding: 1em; }
        .page-title { font-size: 2.2rem; margin-bottom: 0.8em; }
        td, th { font-size: 0.95rem; }
        .sub-time { font-size: 0.75rem; }
        button { width: 100%%; font-size: 1rem; }
        p { font-size: 1rem; }
      }
    </style>
  </head>
  <body>
    <h1 class="page-title">
      <div class="title-word">
        <span class="title-letter" style="color:#ff5722">K</span>
        <span class="title-letter" style="color:#9c27b0">i</span>
        <span class="title-letter" style="color:#4caf50">n</span>
        <span class="title-letter" style="color:#ff9800">g</span>
      </div>
      <div class="title-word">
        <span class="title-letter" style="color:#0288d1">o</span>
        <span class="title-letter" style="color:#ff5722">f</span>
      </div>
      <div class="title-word">
        <span class="title-letter" style="color:#9c27b0">t</span>
        <span class="title-letter" style="color:#4caf50">h</span>
        <span class="title-letter" style="color:#ff9800">e</span>
      </div>
      <div class="title-word">
        <span class="title-letter" style="color:#0288d1">H</span>
        <span class="title-letter" style="color:#ff5722">i</span>
        <span class="title-letter" style="color:#9c27b0">l</span>
        <span class="title-letter" style="color:#4caf50">l</span>
      </div>
    </h1>

    <div class="card">
      <p><strong>Winner:</strong> %Winner%</p>
      <table class="stations-table">
        <thead>
          <tr><th>Station</th><th>Team Red Time(s)</th><th>Team Blue Time(s)</th></tr>
        </thead>
        <tbody>%CurrentTimes%</tbody>
      </table>
      <table>
        <thead>
          <tr><th>Past Team Red Times</th><th>Past Team Blue Times</th></tr>
        </thead>
        <tbody>%PastTimes%</tbody>
      </table>
      <a href="/"><button>New Game</button></a>
    </div>
  </body>
</html>
)";

// ─── HTML row templates ───────────────────────────────────────────────────────
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
