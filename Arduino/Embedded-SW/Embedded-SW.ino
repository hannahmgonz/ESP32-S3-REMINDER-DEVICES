// Medication reminder for the Waveshare ESP32-S3-Touch-LCD-1.47.
//
// Life cycle (the chip is in deep sleep almost all the time):
//   1. The RTC timer wakes the chip at the reminder time.
//   2. The groovy reminder screen comes up and waits for a tap on DONE.
//   3. DONE -> short celebration, backlight fades, LCD sleeps, chip deep-sleeps
//      until the same time tomorrow.
//   4. With REMINDER_TIMEOUT_S = 0 the screen stays on until DONE is tapped.
//      Otherwise, no tap within REMINDER_TIMEOUT_S -> sleep NAG_INTERVAL_S and
//      remind again (up to MAX_NAGS times).
//   The BOOT button wakes the device at any time to show the next reminder,
//   or the pending reminder if today's dose hasn't been marked done.

#include <WiFi.h>
#include <esp_sleep.h>
#include <esp_sntp.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <time.h>
#include <sys/time.h>

#include "board_pins.h"
#include "config.h"
#include "lcd_jd9853.h"
#include "touch_axs5106l.h"
#include "ui.h"

// ---- State that survives deep sleep (lost on power-off or reset) ----
RTC_DATA_ATTR int16_t rtcTakenYear = -1;   // date the dose was last marked done
RTC_DATA_ATTR int16_t rtcTakenYday = -1;
RTC_DATA_ATTR int16_t rtcNagYday = -1;     // day the nag counter belongs to
RTC_DATA_ATTR uint8_t rtcNagCount = 0;
RTC_DATA_ATTR time_t rtcLastSync = 0;

enum class Mode { Reminder, Celebrate, Status };
static Mode mode;
static uint32_t modeStart;
static bool demoRun = false;  // cold-boot preview: Done doesn't count as today's dose
// Date the reminder on screen belongs to, so a tap after midnight still counts
// for the day it was shown, not the next one.
static int16_t shownYear = -1, shownYday = -1;

// ------------------------------------------------------------------ time
static bool clockValid() { return time(nullptr) > 1735689600; }  // after 2025-01-01

static bool wifiConfigured() { return WIFI_SSID[0] != '\0'; }

// Joins Wi-Fi just long enough to set the clock over NTP.
static bool syncClock(uint32_t timeoutMs = 12000) {
  if (!wifiConfigured()) return false;
  Serial.println("[time] syncing over Wi-Fi");
  const uint32_t t0 = millis();
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) delay(100);

  bool ok = false;
  if (WiFi.status() == WL_CONNECTED) {
    configTzTime(TZ_POSIX, NTP_SERVER_1, NTP_SERVER_2);
    while (millis() - t0 < timeoutMs) {
      if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) { ok = true; break; }
      delay(100);
    }
  }
  const wl_status_t wifiStatus = WiFi.status();
  esp_sntp_stop();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  if (ok) rtcLastSync = time(nullptr);
  if (ok) {
    char buf[32];
    time_t now = time(nullptr);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", localtime(&now));
    Serial.printf("[time] sync ok after %lu ms: %s\n", millis() - t0, buf);
  } else {
    // 1 = network not found (check the name, and that it's 2.4 GHz), 4 = connect failed
    // (often a wrong password), 6 = disconnected, 3 = connected but NTP didn't answer.
    Serial.printf("[time] sync FAILED after %lu ms, Wi-Fi status %d\n", millis() - t0, int(wifiStatus));
  }
  return ok;
}

// Fallback when there's no Wi-Fi: assume the clock reads the time this was compiled.
static void seedClockFromBuild() {
  static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char mon[4] = {0};
  struct tm t = {};
  sscanf(__DATE__, "%3s %d %d", mon, &t.tm_mday, &t.tm_year);
  sscanf(__TIME__, "%d:%d:%d", &t.tm_hour, &t.tm_min, &t.tm_sec);
  t.tm_mon = (strstr(months, mon) - months) / 3;
  t.tm_year -= 1900;
  t.tm_isdst = -1;
  timeval tv = {mktime(&t), 0};
  settimeofday(&tv, nullptr);
  Serial.println("[time] no Wi-Fi: clock seeded from build time");
}

static time_t reminderOn(const struct tm &day, int addDays = 0) {
  struct tm t = day;
  t.tm_hour = REMINDER_HOUR;
  t.tm_min = REMINDER_MINUTE;
  t.tm_sec = 0;
  t.tm_mday += addDays;
  t.tm_isdst = -1;
  return mktime(&t);
}

static void localNow(time_t &now, struct tm &tm) {
  now = time(nullptr);
  localtime_r(&now, &tm);
}

static bool takenToday(const struct tm &tm) {
  return rtcTakenYear == tm.tm_year && rtcTakenYday == tm.tm_yday;
}

// Today's dose is due once the reminder time has passed and it isn't marked done.
static bool reminderDue(time_t now, const struct tm &tm) {
  return !takenToday(tm) && now >= reminderOn(tm);
}

static void formatClock(time_t t, char *out, size_t n) {
  struct tm tm;
  localtime_r(&t, &tm);
  strftime(out, n, "%I:%M %p", &tm);
  if (out[0] == '0') memmove(out, out + 1, strlen(out));  // "08:00 AM" -> "8:00 AM"
}

static float batteryVolts() {
  return analogReadMilliVolts(PIN_BAT_ADC) * 3.0f / 1000.0f;
}

// ------------------------------------------------------------------ power
static void releaseSleepHolds() {
  gpio_hold_dis((gpio_num_t)PIN_LCD_BL);
  gpio_hold_dis((gpio_num_t)PIN_TP_RST);
  gpio_deep_sleep_hold_dis();
}

static void deepSleepFor(uint64_t seconds, bool displayWasOn) {
  if (seconds < 5) seconds = 5;
  Serial.printf("[power] deep sleep for %llu s\n", seconds);

  if (displayWasOn) lcd_sleep();

  // Hold backlight off and the touch controller in reset while asleep.
  ledcDetach(PIN_LCD_BL);
  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, LOW);
  pinMode(PIN_TP_RST, OUTPUT);
  digitalWrite(PIN_TP_RST, LOW);
  gpio_hold_en((gpio_num_t)PIN_LCD_BL);
  gpio_hold_en((gpio_num_t)PIN_TP_RST);
  gpio_deep_sleep_hold_en();

  esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
  rtc_gpio_pullup_en((gpio_num_t)PIN_BOOT_BTN);
  rtc_gpio_pulldown_dis((gpio_num_t)PIN_BOOT_BTN);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BOOT_BTN, 0);

  Serial.flush();
  esp_deep_sleep_start();
}

// Works out when to wake next, does the daily clock sync, then sleeps.
static void sleepUntilNext(bool displayWasOn) {
  if (displayWasOn) lcd_sleep();

  if (wifiConfigured() && time(nullptr) - rtcLastSync > TIME_SYNC_EVERY_S) syncClock();

  time_t now;
  struct tm tm;
  localNow(now, tm);

  time_t wake;
  if (reminderDue(now, tm) && rtcNagCount < MAX_NAGS) {
    wake = now + NAG_INTERVAL_S;  // still not taken: nag again soon
  } else {
    wake = reminderOn(tm);
    if (now >= wake) wake = reminderOn(tm, 1);
  }
  char buf[16];
  formatClock(wake, buf, sizeof(buf));
  Serial.printf("[power] next wake at %s\n", buf);
  deepSleepFor(wake - now, false);
}

// ------------------------------------------------------------------ screens
static void showReminder() {
  time_t now;
  struct tm tm;
  localNow(now, tm);
  if (rtcNagYday != tm.tm_yday) {  // first reminder of a new day
    rtcNagYday = tm.tm_yday;
    rtcNagCount = 0;
  }
  shownYear = tm.tm_year;
  shownYday = tm.tm_yday;
  char buf[16];
  formatClock(reminderOn(tm), buf, sizeof(buf));
  ui_show_reminder(buf, batteryVolts());
  lcd_fade(0, BACKLIGHT_LEVEL, 400);
  mode = Mode::Reminder;
  modeStart = millis();
}

static void showStatus() {
  time_t now;
  struct tm tm;
  localNow(now, tm);
  time_t next = reminderOn(tm);
  const bool today = now < next && !takenToday(tm);
  if (!today) next = reminderOn(tm, 1);
  char buf[16];
  formatClock(next, buf, sizeof(buf));
  ui_show_status(buf, today ? "Today" : "Tomorrow", takenToday(tm), batteryVolts());
  lcd_fade(0, BACKLIGHT_LEVEL, 400);
  mode = Mode::Status;
  modeStart = millis();
}

// ------------------------------------------------------------------ main
void setup() {
  Serial.begin(115200);
  releaseSleepHolds();
  setenv("TZ", TZ_POSIX, 1);
  tzset();

  const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  const bool coldBoot = cause != ESP_SLEEP_WAKEUP_TIMER && cause != ESP_SLEEP_WAKEUP_EXT0;
  Serial.printf("\n[boot] wake cause %d\n", int(cause));

  if (coldBoot || !clockValid()) {
    if (!syncClock() && !clockValid()) seedClockFromBuild();
  }

  time_t now;
  struct tm tm;
  localNow(now, tm);
  const bool due = reminderDue(now, tm);

  // A timer wake with nothing due (e.g. the RTC ran slightly fast): go straight
  // back to sleep without ever turning the screen on.
  if (cause == ESP_SLEEP_WAKEUP_TIMER && !due) sleepUntilNext(false);

  ui_begin();
  ui_set_clock_synced(rtcLastSync != 0 && now - rtcLastSync < 36 * 3600);
  if (due) {
    showReminder();
  } else if (coldBoot && SHOW_REMINDER_ON_COLD_BOOT) {
    demoRun = true;
    showReminder();
  } else {
    showStatus();
  }
}

void loop() {
  ui_tick();
  delay(5);
  const uint32_t elapsed = millis() - modeStart;

  switch (mode) {
    case Mode::Reminder:
      if (ui_done_pressed()) {
        if (!demoRun) {
          rtcTakenYear = shownYear;
          rtcTakenYday = shownYday;
          rtcNagCount = 0;
        }
        Serial.printf("[ui] Done tapped%s\n", demoRun ? " (preview, not recorded)" : "");
        ui_show_celebration();
        mode = Mode::Celebrate;
        modeStart = millis();
      } else if (REMINDER_TIMEOUT_S > 0 && elapsed > REMINDER_TIMEOUT_S * 1000UL) {
        rtcNagCount++;
        Serial.printf("[ui] no response, nag %u/%u\n", rtcNagCount, MAX_NAGS);
        lcd_fade(BACKLIGHT_LEVEL, 0, 600);
        sleepUntilNext(true);
      }
      break;

    case Mode::Celebrate:
      if (elapsed > 2500) {
        lcd_fade(BACKLIGHT_LEVEL, 0, 800);
        sleepUntilNext(true);
      }
      break;

    case Mode::Status:
      if (ui_tapped() || elapsed > STATUS_SCREEN_S * 1000UL) {
        lcd_fade(BACKLIGHT_LEVEL, 0, 400);
        sleepUntilNext(true);
      }
      break;
  }
}
