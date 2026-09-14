#include <Arduino.h>
#include <Wire.h>
#include <esp_display_panel.hpp>
#include <lvgl.h>
#include "cyd35_pins.h"
#include "cyd_display.h"
#include "lvgl_v8_port.h"

using namespace esp_panel::drivers;

extern volatile float engineRPM, vehicleSpeed, coolantTemp, oilPressure, airPrimary, airSecondary, batteryVoltage;
extern volatile uint32_t activeSPN, receivedPackets, droppedPackets;
extern volatile uint8_t activeFMI;
extern bool captureEnabled;
extern bool setPacketCapture(bool enabled);

namespace {
constexpr uint8_t TOUCH_ADDRESS = 0x55;
constexpr uint16_t TOUCH_POINT0 = 0x0014;
constexpr uint16_t SCREEN_W = Cyd35Pins::DISPLAY_WIDTH;
constexpr uint16_t SCREEN_H = Cyd35Pins::DISPLAY_HEIGHT;
LCD *lcd = nullptr;
BacklightPWM_LEDC *backlight = nullptr;
lv_obj_t *rpmLabel = nullptr, *speedLabel = nullptr, *coolantLabel = nullptr, *oilLabel = nullptr;
lv_obj_t *airLabel = nullptr, *voltageLabel = nullptr, *faultLabel = nullptr, *captureLabel = nullptr, *busLabel = nullptr;
uint32_t lastUpdate = 0;

void setLabel(lv_obj_t *label, const char *format, ...) {
  char text[64];
  va_list args;
  va_start(args, format);
  vsnprintf(text, sizeof(text), format, args);
  va_end(args);
  lv_label_set_text(label, text);
}

lv_obj_t *gauge(lv_obj_t *parent, const char *title, int x, int y, int width) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_size(card, width, 74);
  lv_obj_set_style_radius(card, 8, 0);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x101b2a), 0);
  lv_obj_set_style_border_color(card, lv_color_hex(0x24364e), 0);
  lv_obj_set_style_pad_all(card, 5, 0);
  lv_obj_t *caption = lv_label_create(card);
  lv_label_set_text(caption, title);
  lv_obj_set_style_text_color(caption, lv_color_hex(0x91a2b9), 0);
  lv_obj_align(caption, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_t *value = lv_label_create(card);
  lv_obj_set_style_text_font(value, LV_FONT_DEFAULT, 0);
  lv_obj_set_style_text_color(value, lv_color_hex(0x32e6bf), 0);
  lv_label_set_text(value, "--");
  lv_obj_align(value, LV_ALIGN_BOTTOM_MID, 0, 0);
  return value;
}

void captureClicked(lv_event_t *) {
  setPacketCapture(!captureEnabled);
}

void touchRead(lv_indev_drv_t *, lv_indev_data_t *data) {
  uint8_t reg[] = {static_cast<uint8_t>(TOUCH_POINT0 >> 8), static_cast<uint8_t>(TOUCH_POINT0)};
  uint8_t point[7] = {};
  Wire.beginTransmission(TOUCH_ADDRESS);
  Wire.write(reg, sizeof(reg));
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(TOUCH_ADDRESS, sizeof(point)) != sizeof(point)) {
    data->state = LV_INDEV_STATE_REL;
    return;
  }
  if (!(point[0] & 0x80)) {
    data->state = LV_INDEV_STATE_REL;
    return;
  }
  const uint16_t rawX = ((point[0] & 0x3F) << 8) | point[1];
  const uint16_t rawY = ((point[2] & 0x3F) << 8) | point[3];
  // Same clockwise landscape transform used by the vendor ST77922 CTP driver.
  data->point.x = rawY >= SCREEN_W ? SCREEN_W - 1 : rawY;
  data->point.y = rawX >= SCREEN_H ? 0 : SCREEN_H - 1 - rawX;
  data->state = LV_INDEV_STATE_PR;
}

void initTouchInput() {
  pinMode(Cyd35Pins::TOUCH_RST, OUTPUT);
  digitalWrite(Cyd35Pins::TOUCH_RST, LOW);
  delay(20);
  digitalWrite(Cyd35Pins::TOUCH_RST, HIGH);
  Wire.begin(Cyd35Pins::TOUCH_SDA, Cyd35Pins::TOUCH_SCL, 100000);
  static lv_indev_drv_t input;
  lv_indev_drv_init(&input);
  input.type = LV_INDEV_TYPE_POINTER;
  input.read_cb = touchRead;
  lv_indev_drv_register(&input);
}
}  // namespace

bool cydDisplayBegin() {
  backlight = new BacklightPWM_LEDC(Cyd35Pins::LCD_BL, 1);
  backlight->begin();
  backlight->off();
  auto *bus = new BusQSPI(Cyd35Pins::LCD_CS, Cyd35Pins::LCD_SCLK, Cyd35Pins::LCD_D0,
                          Cyd35Pins::LCD_D1, Cyd35Pins::LCD_D2, Cyd35Pins::LCD_D3);
  bus->configQSPI_FreqHz(40000000);
  lcd = new LCD_ST77922(bus, 320, 480, 16, -1);
  if (!lcd->begin()) return false;
  // Portrait-native panel, rotated clockwise into 480x320 landscape.
  lcd->swapXY(true);
  lcd->displayOn();
  backlight->on();
  if (!lvgl_port_init(lcd, nullptr) || !lvgl_port_lock(-1)) return false;

  lv_obj_t *screen = lv_scr_act();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x070b12), 0);
  lv_obj_t *title = lv_label_create(screen);
  lv_label_set_text(title, "ESP32BusDash  |  CAN LISTEN ONLY");
  lv_obj_set_style_text_color(title, lv_color_hex(0xeaf2ff), 0);
  lv_obj_set_pos(title, 12, 8);
  rpmLabel = gauge(screen, "ENGINE RPM", 10, 35, 110);
  speedLabel = gauge(screen, "SPEED MPH", 130, 35, 110);
  coolantLabel = gauge(screen, "COOLANT F", 250, 35, 110);
  oilLabel = gauge(screen, "OIL PSI", 370, 35, 100);
  airLabel = gauge(screen, "AIR 1 / 2 PSI", 10, 118, 140);
  voltageLabel = gauge(screen, "BATTERY V", 160, 118, 120);
  faultLabel = gauge(screen, "ACTIVE DTC", 290, 118, 180);
  lv_obj_t *button = lv_btn_create(screen);
  lv_obj_set_pos(button, 10, 206);
  lv_obj_set_size(button, 230, 48);
  lv_obj_add_event_cb(button, captureClicked, LV_EVENT_CLICKED, nullptr);
  captureLabel = lv_label_create(button);
  lv_obj_center(captureLabel);
  busLabel = lv_label_create(screen);
  lv_obj_set_pos(busLabel, 252, 217);
  lv_obj_set_style_text_color(busLabel, lv_color_hex(0x91a2b9), 0);
  initTouchInput();
  lvgl_port_unlock();
  return true;
}

void cydDisplayUpdate() {
  if (!lcd || millis() - lastUpdate < 250 || !lvgl_port_lock(0)) return;
  lastUpdate = millis();
  setLabel(rpmLabel, "%.0f", engineRPM);
  setLabel(speedLabel, "%.0f", vehicleSpeed);
  setLabel(coolantLabel, "%.0f", coolantTemp);
  setLabel(oilLabel, "%.0f", oilPressure);
  setLabel(airLabel, "%.0f / %.0f", airPrimary, airSecondary);
  setLabel(voltageLabel, "%.1f", batteryVoltage);
  if (activeSPN) {
    setLabel(faultLabel, "%lu / %u", static_cast<unsigned long>(activeSPN), activeFMI);
  } else {
    lv_label_set_text(faultLabel, "NONE");
  }
  lv_label_set_text(captureLabel, captureEnabled ? "STOP PACKET CAPTURE" : "START PACKET CAPTURE");
  setLabel(busLabel, "Packets: %lu\nDrops: %lu", static_cast<unsigned long>(receivedPackets), static_cast<unsigned long>(droppedPackets));
  lvgl_port_unlock();
}
