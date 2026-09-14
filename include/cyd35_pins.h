#pragma once

// Elecrow ESP32-S3 3.5-inch CYD hardware profile.
// The values match the ST77922/CTP/Speaker/Microphone board layout.
// GPIO45 and GPIO46 are intentionally reserved for an external CAN transceiver.

namespace Cyd35Pins {
constexpr int CAN_RX = 45;  // transceiver RXD -> ESP32 input
constexpr int CAN_TX = 46;  // ESP32 output -> transceiver TXD

// SD card socket, SD_MMC 1-bit mode. Do not use SD.begin() on this board.
constexpr int SD_CLK = 5;
constexpr int SD_CMD = 4;
constexpr int SD_D0 = 6;

// ST77922 QSPI panel, 320x480 native; firmware selects 480x320 landscape.
constexpr int LCD_CS = 10;
constexpr int LCD_BL = 41;
constexpr int LCD_SCLK = 12;
constexpr int LCD_D0 = 11;
constexpr int LCD_D1 = 13;
constexpr int LCD_D2 = 14;
constexpr int LCD_D3 = 9;

// Capacitive touch controller (Sitronix-compatible address 0x55).
constexpr int TOUCH_SDA = 38;
constexpr int TOUCH_SCL = 39;
constexpr int TOUCH_RST = 48;
constexpr int TOUCH_INT = 47;

constexpr int DISPLAY_WIDTH = 480;
constexpr int DISPLAY_HEIGHT = 320;
}  // namespace Cyd35Pins
