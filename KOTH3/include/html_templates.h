#ifndef HTML_TEMPLATES_H
#define HTML_TEMPLATES_H

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════════
//  HTML / template strings
// ═══════════════════════════════════════════════════════════════════════════════

extern const char *setup_html;
extern const char *game_html;
extern const char *end_html;

// ─── HTML row templates ───────────────────────────────────────────────────────
extern const char *current_stations_table_row_html;
extern const char *current_stations_table_total_row_html;
extern const char *past_times_table_row_start_html;
extern const char *past_times_table_row_total_html;
extern const char *past_times_table_row_station_html;
extern const char *past_times_table_row_middle_html;
extern const char *past_times_table_row_end_html;

#endif // HTML_TEMPLATES_H
