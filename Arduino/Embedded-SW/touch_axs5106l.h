// Minimal AXS5106L capacitive touch reader (polled over I2C).
#pragma once
#include <Arduino.h>

void touch_begin();  // call after lcd_begin(), which has already reset the chip
// Returns true while a finger is down; x/y are in screen pixels (portrait).
bool touch_read(int16_t &x, int16_t &y);
