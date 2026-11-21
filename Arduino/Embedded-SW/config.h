// Settings for one reminder device. Edit these, then re-flash.
#pragma once

// ---- Identity (for when more devices join the network) ----
#define DEVICE_NAME            "Kitchen"

// ---- Schedule (local time, 24 h clock) ----
#define REMINDER_HOUR          8
#define REMINDER_MINUTE        0

// ---- What the screen says ----
#define GREETING_TEXT          "Good morning!"
#define REMINDER_TITLE         "Morning meds"

// ---- If nobody taps Done ----
// 0 = the reminder stays on until Done is tapped (the screen draws power the whole
// time, roughly 60-100 mA, so it's best on USB power).
// Any other value = turn off after this many seconds, sleep NAG_INTERVAL_S, then
// remind again, up to MAX_NAGS times.
#define REMINDER_TIMEOUT_S     0
#define NAG_INTERVAL_S         (15 * 60)
#define MAX_NAGS               4

// ---- Time zone (POSIX TZ string) ----
// Atlantic (Halifax, with DST): "AST4ADT,M3.2.0,M11.1.0"
// Atlantic, no DST (most of Quebec's Lower North Shore): "AST4"
// Newfoundland: "NST3:30NDT,M3.2.0,M11.1.0"   Eastern: "EST5EDT,M3.2.0,M11.1.0"
// Central: "CST6CDT,M3.2.0,M11.1.0"           Pacific: "PST8PDT,M3.2.0,M11.1.0"
#define TZ_POSIX               "AST4ADT,M3.2.0,M11.1.0"

// ---- Display ----
#define BACKLIGHT_LEVEL        180        // 0-255
#define STATUS_SCREEN_S        8          // how long the "next reminder" screen stays up

// Touch orientation fixes, in case taps land in the wrong place on your unit.
#define TOUCH_SWAP_XY          0
#define TOUCH_MIRROR_X         0
#define TOUCH_MIRROR_Y         0

// ---- Development helpers ----
// 1 = show the reminder immediately after flashing or pressing RESET.
#define SHOW_REMINDER_ON_COLD_BOOT 1

// ---- Wi-Fi time sync (optional, recommended) ----
// Put WIFI_SSID / WIFI_PASSWORD in secrets.h (see secrets.example.h). Without it
// the clock is seeded from the build time and drifts during deep sleep.
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#endif
#define NTP_SERVER_1           "pool.ntp.org"
#define NTP_SERVER_2           "time.nist.gov"
#define TIME_SYNC_EVERY_S      (20 * 3600) // re-sync at most about once a day
