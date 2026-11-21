#include "lcd_jd9853.h"
#include <SPI.h>
#include "board_pins.h"

static SPIClass lcdSpi(FSPI);
static const SPISettings kSpi(40000000, MSBFIRST, SPI_MODE0);

static void lcd_cmd(uint8_t cmd, const uint8_t *data = nullptr, size_t len = 0) {
  lcdSpi.beginTransaction(kSpi);
  digitalWrite(PIN_LCD_CS, LOW);
  digitalWrite(PIN_LCD_DC, LOW);
  lcdSpi.write(cmd);
  if (len) {
    digitalWrite(PIN_LCD_DC, HIGH);
    lcdSpi.writeBytes(data, len);
  }
  digitalWrite(PIN_LCD_CS, HIGH);
  lcdSpi.endTransaction();
}

// Vendor init sequence for the JD9853 on this board.
// Format: cmd, data length, data...   A cmd of 0xFF means "delay <length> ms".
static const uint8_t kInit[] = {
  0x11, 0,
  0xFF, 120,
  0xDF, 2, 0x98, 0x53,
  0xB2, 1, 0x23,
  0xB7, 4, 0x00, 0x47, 0x00, 0x6F,
  0xBB, 6, 0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0,
  0xC0, 2, 0x44, 0xA4,
  0xC1, 1, 0x16,
  0xC3, 8, 0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77,
  0xC4, 12, 0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A, 0x16, 0x79, 0x0B, 0x0A, 0x16, 0x82,
  0xC8, 32, 0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
            0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
  0xD0, 5, 0x04, 0x06, 0x6B, 0x0F, 0x00,
  0xD7, 2, 0x00, 0x30,
  0xE6, 1, 0x14,
  0xDE, 1, 0x01,
  0xB7, 5, 0x03, 0x13, 0xEF, 0x35, 0x35,
  0xC1, 3, 0x14, 0x15, 0xC0,
  0xC2, 2, 0x06, 0x3A,
  0xC4, 2, 0x72, 0x12,
  0xBE, 1, 0x00,
  0xDE, 1, 0x02,
  0xE5, 3, 0x00, 0x02, 0x00,
  0xE5, 3, 0x01, 0x02, 0x00,
  0xDE, 1, 0x00,
  0x35, 1, 0x00,          // tearing effect on
  0x3A, 1, 0x05,          // 16-bit RGB565
  0x2A, 4, 0x00, 0x22, 0x00, 0xCD,
  0x2B, 4, 0x00, 0x00, 0x01, 0x3F,
  0xDE, 1, 0x02,
  0xE5, 3, 0x00, 0x02, 0x00,
  0xDE, 1, 0x00,
  0x36, 1, 0x00,          // portrait, RGB order
  0x21, 0,                // colour inversion on (IPS panel)
  0xFF, 10,
  0x29, 0,                // display on
};

void lcd_begin() {
  pinMode(PIN_LCD_CS, OUTPUT);
  pinMode(PIN_LCD_DC, OUTPUT);
  digitalWrite(PIN_LCD_CS, HIGH);

  // Backlight off until the first frame is drawn.
  ledcAttach(PIN_LCD_BL, 20000, 8);
  ledcWrite(PIN_LCD_BL, 0);

  // Hardware reset. Pulses both candidate reset lines; timing also suits the touch chip.
  pinMode(PIN_LCD_RST, OUTPUT);
  pinMode(PIN_TP_RST, OUTPUT);
  digitalWrite(PIN_LCD_RST, LOW);
  digitalWrite(PIN_TP_RST, LOW);
  delay(200);
  digitalWrite(PIN_LCD_RST, HIGH);
  digitalWrite(PIN_TP_RST, HIGH);
  delay(300);

  lcdSpi.begin(PIN_LCD_SCLK, -1, PIN_LCD_MOSI, -1);

  for (size_t i = 0; i < sizeof(kInit);) {
    uint8_t cmd = kInit[i++];
    uint8_t len = kInit[i++];
    if (cmd == 0xFF) {
      delay(len);
    } else {
      lcd_cmd(cmd, &kInit[i], len);
      i += len;
    }
  }
}

void lcd_push(int x1, int y1, int x2, int y2, const uint8_t *px, size_t bytes) {
  const uint16_t xs = x1 + LCD_COL_OFFSET, xe = x2 + LCD_COL_OFFSET;
  const uint16_t ys = y1 + LCD_ROW_OFFSET, ye = y2 + LCD_ROW_OFFSET;
  const uint8_t ca[4] = {uint8_t(xs >> 8), uint8_t(xs), uint8_t(xe >> 8), uint8_t(xe)};
  const uint8_t ra[4] = {uint8_t(ys >> 8), uint8_t(ys), uint8_t(ye >> 8), uint8_t(ye)};
  lcd_cmd(0x2A, ca, 4);
  lcd_cmd(0x2B, ra, 4);
  lcd_cmd(0x2C, px, bytes);
}

void lcd_sleep() {
  lcd_backlight(0);
  lcd_cmd(0x28);  // display off
  lcd_cmd(0x10);  // sleep in
  delay(120);
}

void lcd_backlight(uint8_t level) { ledcWrite(PIN_LCD_BL, level); }

void lcd_fade(uint8_t from, uint8_t to, uint32_t ms) {
  const int steps = 32;
  for (int i = 1; i <= steps; i++) {
    lcd_backlight(from + (int(to) - int(from)) * i / steps);
    delay(ms / steps);
  }
}
