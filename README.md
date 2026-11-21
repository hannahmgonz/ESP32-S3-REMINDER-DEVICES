# ESP32-S3-REMINDER-DEVICES
IoT for reminders: low-power medication reminders for the home, starting with one
device and growing into a small network.

## Device: Waveshare ESP32-S3-Touch-LCD-1.47

| Part | Chip | Pins |
|---|---|---|
| LCD 172x320 | JD9853 (SPI) | SCLK 38, MOSI 39, CS 21, DC 45, RST 40 (+47), BL 46 |
| Touch | AXS5106L (I2C 0x63) | SDA 42, SCL 41, RST 47, INT 48 |
| Wake button | BOOT | GPIO 0 |
| Battery sense | 1/3 divider | GPIO 12 |

## How it behaves

1. Deep sleep almost all day (display and touch powered down).
2. The RTC timer wakes the device at `REMINDER_HOUR:REMINDER_MINUTE`.
3. A 70s-sunset reminder screen appears with a glowing **DONE** button.
4. Tap **DONE**: a short "Far out!" check-mark animation plays, the backlight fades,
   the LCD sleeps and the chip deep-sleeps until the same time tomorrow.
5. The reminder stays on until **DONE** is tapped. To save battery instead, set
   `REMINDER_TIMEOUT_S` in `config.h`: the screen then turns off after that long,
   sleeps 15 minutes and reminds again (up to 4 times).
6. Press **BOOT** any time to see the next reminder time, or the pending reminder
   if today's dose hasn't been marked done.

Once a day, right before it goes to sleep, the device joins Wi-Fi for a few seconds
to correct its clock over NTP.

## Setup (Arduino IDE 2)

1. Board package: **esp32 by Espressif** 3.x. Board: **Waveshare ESP32-S3-LCD-1.47**
   or **ESP32S3 Dev Module**, *USB CDC On Boot: Enabled* (so serial logs appear
   over USB). The firmware is about 1.4 MB, too big for the default 1.2 MB app
   partition, so the sketch includes its own `partitions.csv` (3 MB app). The
   IDE picks it up automatically and ignores the Partition Scheme menu.
2. Library: **lvgl** 9.x (tested to compile with 9.6.0).
3. Copy [Arduino/lv_conf.h](Arduino/lv_conf.h) to your Arduino libraries folder, next
   to the `lvgl` folder (`Documents/Arduino/libraries/lv_conf.h`).
4. Copy `Arduino/Embedded-SW/secrets.example.h` to `secrets.h` and enter your Wi-Fi.
   Git ignores `secrets.h`.
5. Edit [config.h](Arduino/Embedded-SW/config.h): reminder time, time zone, text.
6. Open `Arduino/Embedded-SW/Embedded-SW.ino` and upload.

After flashing, the reminder shows straight away as a preview
(`SHOW_REMINDER_ON_COLD_BOOT`). Tapping DONE in the preview doesn't count as
today's dose.

### Uploading while it's asleep
In deep sleep the USB port disappears. To upload, hold **BOOT**, tap **RESET**,
release **BOOT**, then upload. Press **RESET** afterwards to run the new firmware.

## Notes and limits
- **Clock:** sleep state and the clock survive deep sleep, but not power loss or
  RESET. Without Wi-Fi the clock is set to the time the firmware was compiled and
  can drift by minutes a day. Use Wi-Fi.
- **Touch orientation:** if taps land in the wrong spot, flip the `TOUCH_*` flags in
  `config.h`.
- **LCD reset pin:** published sources disagree on whether it's GPIO 40 or 47, so
  the driver pulses both.

## Roadmap
- Provision more devices (one `DEVICE_NAME` and schedule per device).
- Device-to-device / hub messaging (ESP-NOW or Wi-Fi) so a hub can see
  who took their meds, push schedule changes, and raise an alert on missed doses.
- Several reminders per day.
