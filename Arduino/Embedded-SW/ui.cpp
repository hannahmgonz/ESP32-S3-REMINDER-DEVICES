#include "ui.h"
#include <lvgl.h>
#include <time.h>
#include "board_pins.h"
#include "config.h"
#include "lcd_jd9853.h"
#include "touch_axs5106l.h"

// ---- Palette: 70s sunset ----
#define C_PLUM    lv_color_hex(0x3B1060)
#define C_RUST    lv_color_hex(0xE8553D)
#define C_CREAM   lv_color_hex(0xFFF1D0)
#define C_MUSTARD lv_color_hex(0xF9C03C)
#define C_ORANGE  lv_color_hex(0xFF8C32)
#define C_PINK    lv_color_hex(0xFF4F8B)
#define C_TEAL    lv_color_hex(0x2EC4B6)

static lv_display_t *s_disp;
static lv_obj_t *s_content;  // swapped per screen; background + rainbow stay
static volatile bool s_done = false;
static volatile bool s_tapped = false;

// Two 40-line partial render buffers in internal RAM.
#define BUF_LINES 40
static uint8_t s_buf1[LCD_WIDTH * BUF_LINES * 2] __attribute__((aligned(4)));
static uint8_t s_buf2[LCD_WIDTH * BUF_LINES * 2] __attribute__((aligned(4)));

// ---------------------------------------------------------------- glue
static void flush_cb(lv_display_t *disp, const lv_area_t *a, uint8_t *px) {
  const size_t bytes = size_t(lv_area_get_width(a)) * lv_area_get_height(a) * 2;
  lcd_push(a->x1, a->y1, a->x2, a->y2, px, bytes);
  lv_display_flush_ready(disp);
}

static void touch_cb(lv_indev_t *, lv_indev_data_t *data) {
  int16_t x, y;
  if (touch_read(x, y)) {
    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

static uint32_t tick_cb() { return millis(); }

void ui_begin() {
  lcd_begin();
  touch_begin();

  lv_init();
  lv_tick_set_cb(tick_cb);

  s_disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
  lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565_SWAPPED);  // panel wants big-endian
  lv_display_set_flush_cb(s_disp, flush_cb);
  lv_display_set_buffers(s_disp, s_buf1, s_buf2, sizeof(s_buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touch_cb);
}

void ui_tick() { lv_timer_handler(); }
bool ui_done_pressed() { return s_done; }
bool ui_tapped() { return s_tapped; }

// ---------------------------------------------------------------- helpers
static void anim_y(void *o, int32_t v) { lv_obj_set_y((lv_obj_t *)o, v); }
static void anim_arc(void *o, int32_t v) { lv_arc_set_value((lv_obj_t *)o, v); }
static void anim_shadow(void *o, int32_t v) { lv_obj_set_style_shadow_width((lv_obj_t *)o, v, 0); }
static void anim_size(void *o, int32_t v) { lv_obj_set_size((lv_obj_t *)o, v, v); }

static lv_obj_t *plain(lv_obj_t *parent) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}

static lv_obj_t *label(lv_obj_t *parent, const char *txt, const lv_font_t *font, lv_color_t c, int y) {
  lv_obj_t *l = lv_label_create(parent);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, c, 0);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(l, LV_ALIGN_TOP_MID, 0, y);
  return l;
}

static void start_anim(void *obj, lv_anim_exec_xcb_t cb, int32_t from, int32_t to, uint32_t ms,
                       uint32_t delay, bool loop) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_exec_cb(&a, cb);
  lv_anim_set_values(&a, from, to);
  lv_anim_set_duration(&a, ms);
  lv_anim_set_delay(&a, delay);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
  if (loop) {
    lv_anim_set_reverse_duration(&a, ms);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  }
  lv_anim_start(&a);
}

// Lava-lamp bubbles drifting up behind everything.
static void add_bubbles(lv_obj_t *scr) {
  static const struct { int x, size; uint32_t ms, delay; uint32_t color; } b[] = {
    {18, 22, 9000, 0, 0xFF4F8B},    {130, 30, 11000, 2500, 0xF9C03C},
    {70, 14, 7500, 1200, 0x2EC4B6}, {150, 16, 8500, 5000, 0xFF8C32},
    {40, 26, 12000, 6500, 0xF9C03C},
  };
  for (auto &d : b) {
    lv_obj_t *o = plain(scr);
    lv_obj_set_size(o, d.size, d.size);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(d.color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_30, 0);
    lv_obj_set_x(o, d.x);
    lv_obj_set_y(o, LCD_HEIGHT + 10);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_exec_cb(&a, anim_y);
    lv_anim_set_values(&a, LCD_HEIGHT + 10, -40);
    lv_anim_set_duration(&a, d.ms);
    lv_anim_set_delay(&a, d.delay);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
  }
}

// Retro sunrise: four concentric half-rings that sweep in one after another.
static void add_rainbow(lv_obj_t *scr) {
  static const uint32_t colors[] = {0xFF4F8B, 0xFF8C32, 0xF9C03C, 0x2EC4B6};
  const int centerY = 92, width = 11, step = 14;
  for (int i = 0; i < 4; i++) {
    const int d = 150 - i * step * 2;
    lv_obj_t *arc = lv_arc_create(scr);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_set_size(arc, d, d);
    lv_obj_align(arc, LV_ALIGN_TOP_MID, 0, centerY - d / 2);
    lv_arc_set_bg_angles(arc, 180, 360);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 0);
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_hex(colors[i]), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    start_anim(arc, anim_arc, 0, 100, 700, 150 + i * 180, false);
  }
}

// A little two-tone capsule that bobs inside the rainbow.
static void add_pill(lv_obj_t *parent, int y) {
  lv_obj_t *pill = plain(parent);
  lv_obj_set_size(pill, 42, 18);
  lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_clip_corner(pill, true, 0);
  lv_obj_set_style_bg_color(pill, C_CREAM, 0);
  lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
  lv_obj_set_x(pill, (LCD_WIDTH - 42) / 2);

  lv_obj_t *half = plain(pill);
  lv_obj_set_size(half, 21, 18);
  lv_obj_set_style_bg_color(half, C_PINK, 0);
  lv_obj_set_style_bg_opa(half, LV_OPA_COVER, 0);
  lv_obj_align(half, LV_ALIGN_RIGHT_MID, 0, 0);

  start_anim(pill, anim_y, y, y - 6, 900, 0, true);
}

static void add_battery(lv_obj_t *parent, float v) {
  const char *sym;
  if (v <= 2.5f) sym = LV_SYMBOL_USB;
  else if (v > 4.0f) sym = LV_SYMBOL_BATTERY_FULL;
  else if (v > 3.8f) sym = LV_SYMBOL_BATTERY_3;
  else if (v > 3.6f) sym = LV_SYMBOL_BATTERY_2;
  else if (v > 3.4f) sym = LV_SYMBOL_BATTERY_1;
  else sym = LV_SYMBOL_BATTERY_EMPTY;
  lv_obj_t *l = label(parent, sym, &lv_font_montserrat_14, C_CREAM, 4);
  lv_obj_align(l, LV_ALIGN_TOP_RIGHT, -8, 4);
  lv_obj_set_style_text_opa(l, LV_OPA_70, 0);
}

// Small live clock in the top-left corner: Wi-Fi icon = synced over NTP,
// warning icon = the clock may be wrong.
static lv_obj_t *s_clock = nullptr;
static bool s_synced = false;

void ui_set_clock_synced(bool synced) { s_synced = synced; }

static void clock_update(lv_timer_t *) {
  if (!s_clock) return;
  time_t now = time(nullptr);
  struct tm tm;
  localtime_r(&now, &tm);
  char hm[12];
  strftime(hm, sizeof(hm), "%I:%M %p", &tm);
  const char *t = hm[0] == '0' ? hm + 1 : hm;
  lv_label_set_text_fmt(s_clock, "%s %s", s_synced ? LV_SYMBOL_WIFI : LV_SYMBOL_WARNING, t);
}

static void add_clock(lv_obj_t *parent) {
  static lv_timer_t *timer = nullptr;
  if (!timer) timer = lv_timer_create(clock_update, 1000, NULL);
  s_clock = label(parent, "", &lv_font_montserrat_14, C_CREAM, 4);
  lv_obj_align(s_clock, LV_ALIGN_TOP_LEFT, 8, 4);
  lv_obj_set_style_text_opa(s_clock, LV_OPA_70, 0);
  clock_update(nullptr);
}

static void tap_cb(lv_event_t *) { s_tapped = true; }
static void done_cb(lv_event_t *) { s_done = true; }

// Background gradient, bubbles, rainbow and an empty content layer.
static lv_obj_t *new_screen() {
  s_done = false;
  s_tapped = false;
  s_clock = nullptr;  // the old screen (and its clock label) is about to be deleted

  lv_obj_t *scr = lv_obj_create(NULL);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(scr, C_PLUM, 0);
  lv_obj_set_style_bg_grad_color(scr, C_RUST, 0);
  lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_add_event_cb(scr, tap_cb, LV_EVENT_CLICKED, NULL);

  add_bubbles(scr);
  add_rainbow(scr);

  s_content = plain(scr);
  lv_obj_set_size(s_content, LCD_WIDTH, LCD_HEIGHT);
  add_clock(scr);  // on the screen itself so it survives the celebration swap

  lv_obj_t *old = lv_screen_active();
  lv_screen_load(scr);
  if (old) lv_obj_delete(old);
  return scr;
}

// ---------------------------------------------------------------- screens
void ui_show_reminder(const char *reminderTime, float battery) {
  new_screen();
  add_battery(s_content, battery);
  add_pill(s_content, 68);

  label(s_content, GREETING_TEXT, &lv_font_montserrat_20, C_MUSTARD, 102);
  label(s_content, reminderTime, &lv_font_montserrat_40, C_CREAM, 126);
  label(s_content, REMINDER_TITLE, &lv_font_montserrat_20, C_CREAM, 184);

  lv_obj_t *btn = lv_button_create(s_content);
  lv_obj_set_size(btn, 140, 56);
  lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -14);
  lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(btn, C_MUSTARD, 0);
  lv_obj_set_style_bg_color(btn, C_ORANGE, LV_STATE_PRESSED);
  lv_obj_set_style_shadow_color(btn, C_MUSTARD, 0);
  lv_obj_set_style_shadow_opa(btn, LV_OPA_60, 0);
  lv_obj_set_style_border_width(btn, 3, 0);
  lv_obj_set_style_border_color(btn, C_CREAM, 0);
  lv_obj_add_event_cb(btn, done_cb, LV_EVENT_CLICKED, NULL);
  start_anim(btn, anim_shadow, 2, 22, 1100, 0, true);  // groovy glow

  lv_obj_t *t = lv_label_create(btn);
  lv_label_set_text(t, LV_SYMBOL_OK " DONE");
  lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(t, C_PLUM, 0);
  lv_obj_center(t);

  lv_refr_now(s_disp);
}

void ui_show_celebration() {
  lv_obj_clean(s_content);

  lv_obj_t *ring = plain(s_content);
  lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(ring, C_TEAL, 0);
  lv_obj_set_style_bg_opa(ring, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(ring, 4, 0);
  lv_obj_set_style_border_color(ring, C_CREAM, 0);
  lv_obj_align(ring, LV_ALIGN_TOP_MID, 0, 120);
  lv_obj_set_size(ring, 0, 0);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, ring);
  lv_anim_set_exec_cb(&a, anim_size);
  lv_anim_set_values(&a, 0, 92);
  lv_anim_set_duration(&a, 450);
  lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
  lv_anim_start(&a);

  lv_obj_t *check = lv_label_create(ring);
  lv_label_set_text(check, LV_SYMBOL_OK);
  lv_obj_set_style_text_font(check, &lv_font_montserrat_40, 0);
  lv_obj_set_style_text_color(check, C_CREAM, 0);
  lv_obj_center(check);

  label(s_content, "Yay!", &lv_font_montserrat_28, C_MUSTARD, 228);
  label(s_content, "See you tomorrow", &lv_font_montserrat_16, C_CREAM, 264);
}

void ui_show_status(const char *reminderTime, const char *whenLabel, bool takenToday, float battery) {
  new_screen();
  add_battery(s_content, battery);
  add_pill(s_content, 68);

  label(s_content, DEVICE_NAME " - next reminder", &lv_font_montserrat_14, C_MUSTARD, 106);
  label(s_content, reminderTime, &lv_font_montserrat_40, C_CREAM, 126);
  label(s_content, whenLabel, &lv_font_montserrat_20, C_CREAM, 178);
  if (takenToday) label(s_content, LV_SYMBOL_OK " Taken today", &lv_font_montserrat_16, C_TEAL, 214);
  lv_obj_t *hint = label(s_content, "Tap to sleep", &lv_font_montserrat_14, C_CREAM, 0);
  lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -12);
  lv_obj_set_style_text_opa(hint, LV_OPA_60, 0);

  lv_refr_now(s_disp);
}
