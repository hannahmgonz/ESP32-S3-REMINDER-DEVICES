#include "touch_axs5106l.h"
#include <Wire.h>
#include "board_pins.h"
#include "config.h"

void touch_begin() {
  Wire.begin(PIN_TP_SDA, PIN_TP_SCL, 400000);
  pinMode(PIN_TP_INT, INPUT_PULLUP);
}

bool touch_read(int16_t &x, int16_t &y) {
  uint8_t d[14];
  Wire.beginTransmission(TP_I2C_ADDR);
  Wire.write(0x01);  // touch data register
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(TP_I2C_ADDR, sizeof(d)) != sizeof(d)) return false;
  for (auto &b : d) b = Wire.read();

  const uint8_t points = d[1];
  if (points == 0 || points > 2) return false;

  int16_t rx = ((d[2] & 0x0F) << 8) | d[3];
  int16_t ry = ((d[4] & 0x0F) << 8) | d[5];
#if TOUCH_SWAP_XY
  int16_t t = rx; rx = ry; ry = t;
#endif
#if TOUCH_MIRROR_X
  rx = LCD_WIDTH - 1 - rx;
#endif
#if TOUCH_MIRROR_Y
  ry = LCD_HEIGHT - 1 - ry;
#endif
  x = constrain(rx, 0, LCD_WIDTH - 1);
  y = constrain(ry, 0, LCD_HEIGHT - 1);
  return true;
}
