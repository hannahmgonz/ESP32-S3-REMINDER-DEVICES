// Minimal JD9853 SPI LCD driver: init, push pixels, sleep, backlight.
#pragma once
#include <Arduino.h>

void lcd_begin();
// Pixels must already be big-endian RGB565 (LVGL's RGB565_SWAPPED format).
void lcd_push(int x1, int y1, int x2, int y2, const uint8_t *px, size_t bytes);
void lcd_sleep();                       // display off + sleep-in (~ microamps)
void lcd_backlight(uint8_t level);      // 0-255
void lcd_fade(uint8_t from, uint8_t to, uint32_t ms);
