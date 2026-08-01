# ESP32-8048S050C Recovery Demo

A minimal recovery/diagnostic firmware for **ESP32-8048S050C_1** with:

- ESP32-S3-WROOM-1-N16R8
- 16 MB flash / 8 MB octal PSRAM
- 800×480 ST7262 RGB panel
- Backlight on GPIO2

This first-stage firmware intentionally excludes LVGL, touch, Wi-Fi, OTA and SD card. It initializes the RGB LCD using ESP-IDF 5.5.4, lights the backlight, cycles red/green/blue/white, then shows colour bars. The serial monitor prints a one-second heartbeat.

## Flashing the full image

Use `ESP32-8048S050C_recovery_full.bin` at offset `0x0` with:

- Chip: ESP32-S3
- SPI mode/frequency/size: KEEP if supported; otherwise DIO / 80 MHz / 16 MB
- Compression: enabled

Do not add another file at offset `0x0`.
