// Pin map for the Waveshare ESP32-S3-Touch-LCD-1.47
// (172x320 JD9853 LCD over SPI, AXS5106L capacitive touch over I2C).
#pragma once

// ---- LCD (SPI) ----
#define PIN_LCD_SCLK   38
#define PIN_LCD_MOSI   39
#define PIN_LCD_CS     21
#define PIN_LCD_DC     45
#define PIN_LCD_BL     46   // backlight, PWM
// Sources disagree on whether the LCD reset is on GPIO 40 or shared with the
// touch reset on GPIO 47, so both are pulsed together at start-up.
#define PIN_LCD_RST    40

// ---- Touch (I2C) ----
#define PIN_TP_SDA     42
#define PIN_TP_SCL     41
#define PIN_TP_RST     47
#define PIN_TP_INT     48   // not an RTC GPIO, so it can't wake the chip from deep sleep
#define TP_I2C_ADDR    0x63

// ---- Misc ----
#define PIN_BOOT_BTN   0    // BOOT button, active low, RTC-capable -> used as a wake button
#define PIN_BAT_ADC    12   // battery voltage through a 1/3 divider

// ---- Panel geometry ----
#define LCD_WIDTH      172
#define LCD_HEIGHT     320
#define LCD_COL_OFFSET 34   // the 172-px panel sits in the middle of a 240-column controller
#define LCD_ROW_OFFSET 0
