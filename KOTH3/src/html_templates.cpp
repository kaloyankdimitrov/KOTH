#include "html_templates.h"

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
  <strong>Total:</strong> %s
)");

const char *past_times_table_row_station_html = PSTR(R"(
  <span class="sub-time"><strong>Station %d:</strong> %s</span>
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
