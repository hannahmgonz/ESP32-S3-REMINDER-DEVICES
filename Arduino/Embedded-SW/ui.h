// LVGL screens: the morning reminder, the "done" celebration and a status screen.
#pragma once
#include <Arduino.h>

void ui_begin();  // LVGL + display + touch glue
void ui_tick();   // call often from loop()

// reminderTime e.g. "8:00 AM"; battery in volts (<= 0 when unknown / USB only)
void ui_show_reminder(const char *reminderTime, float battery);
void ui_show_celebration();
// whenLabel e.g. "Tomorrow"; takenToday adds a check mark line
void ui_show_status(const char *reminderTime, const char *whenLabel, bool takenToday, float battery);

// Whether the clock was set over NTP recently; shown next to the live clock.
void ui_set_clock_synced(bool synced);

bool ui_done_pressed();  // Done button was tapped (latched until next screen)
bool ui_tapped();        // any tap on the status screen
