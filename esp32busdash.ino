#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <SD_MMC.h>
#include <driver/twai.h>
#include "cyd35_pins.h"
#include "dashboard.h"

// The ESP32-S3 TWAI controller is connected through a 3.3 V CAN transceiver.
// This firmware is receive-only; it cannot acknowledge or transmit onto the vehicle bus.
constexpr gpio_num_t CAN_RX_PIN = static_cast<gpio_num_t>(Cyd35Pins::CAN_RX);
constexpr gpio_num_t CAN_TX_PIN = static_cast<gpio_num_t>(Cyd35Pins::CAN_TX);
constexpr uint32_t CAN_BITRATE = 250000;
constexpr size_t CAPTURE_FLUSH_BYTES = 2048;

volatile float engineRPM = 0, vehicleSpeed = 0, coolantTemp = 0, fuelLevel = 0;
volatile float oilPressure = 0, transTemp = 0, airPrimary = 0, airSecondary = 0;
volatile float batteryVoltage = 0, turboBoostPSI = 0, fuelRateGPH = 0, engineLoadPct = 0;
volatile uint32_t totalOdometerMiles = 0, activeSPN = 0;
volatile uint8_t selectedGearRaw = 0, activeFMI = 0;
volatile uint8_t lampMIL = 0, lampRedStop = 0, lampAmberWarning = 0, lampProtect = 0, lampWaitToStart = 0;
volatile uint32_t receivedPackets = 0, droppedPackets = 0;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
bool sdReady = false;
bool captureEnabled = false;
bool canReady = false;
uint32_t captureSequence = 0;
uint32_t lastFaultSPN = 0;
uint8_t lastFaultFMI = 0;
uint32_t lastFaultLogMs = 0;
uint32_t lastBroadcastMs = 0;
String captureBuffer;
String captureFilename;

uint16_t le16(const uint8_t *data, uint8_t i) {
  return static_cast<uint16_t>(data[i]) | (static_cast<uint16_t>(data[i + 1]) << 8);
}

uint32_t le32(const uint8_t *data, uint8_t i) {
  return static_cast<uint32_t>(data[i]) | (static_cast<uint32_t>(data[i + 1]) << 8) |
         (static_cast<uint32_t>(data[i + 2]) << 16) | (static_cast<uint32_t>(data[i + 3]) << 24);
}

String isoTimestamp() {
  const uint64_t us = esp_timer_get_time();
  char text[24];
  snprintf(text, sizeof(text), "%lu.%06lu", static_cast<unsigned long>(us / 1000000ULL),
           static_cast<unsigned long>(us % 1000000ULL));
  return String(text);
}

bool mountStorage() {
  LittleFS.begin(true);
  SD_MMC.setPins(Cyd35Pins::SD_CLK, Cyd35Pins::SD_CMD, Cyd35Pins::SD_D0);
  sdReady = SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT);
  if (sdReady && !SD_MMC.exists("/diagnostics.csv")) {
    File f = SD_MMC.open("/diagnostics.csv", FILE_WRITE);
    if (f) { f.println("uptime_s,event,spn,fmi,odometer_mi,source"); f.close(); }
  }
  return sdReady;
}

void logDiagnostic(uint32_t spn, uint8_t fmi) {
  if (spn == 0 || spn == 524287 || !sdReady) return;
  const uint32_t now = millis();
  if (spn == lastFaultSPN && fmi == lastFaultFMI && now - lastFaultLogMs < 10000) return;
  lastFaultSPN = spn; lastFaultFMI = fmi; lastFaultLogMs = now;
  File f = SD_MMC.open("/diagnostics.csv", FILE_APPEND);
  if (!f) return;
  f.printf("%s,DM1,%lu,%u,%lu,%u\n", isoTimestamp().c_str(), static_cast<unsigned long>(spn), fmi,
           static_cast<unsigned long>(totalOdometerMiles), 0U);
  f.close();
}

void flushCapture() {
  if (!sdReady || captureBuffer.isEmpty() || captureFilename.isEmpty()) return;
  File f = SD_MMC.open(captureFilename, FILE_APPEND);
  if (!f) { droppedPackets++; return; }
  f.print(captureBuffer);
  f.close();
  captureBuffer = "";
}

bool setCapture(bool enabled) {
  if (enabled == captureEnabled) return true;
  if (enabled && !sdReady) return false;
  if (enabled) {
    captureFilename = "/captures/can_" + String(++captureSequence) + ".log";
    if (!SD_MMC.exists("/captures")) SD_MMC.mkdir("/captures");
    File f = SD_MMC.open(captureFilename, FILE_WRITE);
    if (!f) return false;
    f.println("# ESP32BusDash candump-compatible capture");
    f.println("# uptime_s CAN_ID#DATA; extended IDs are eight hex digits");
    f.close();
    captureBuffer.reserve(8192);
    captureEnabled = true;
  } else {
    flushCapture();
    captureEnabled = false;
  }
  return true;
}

void appendCapture(const twai_message_t &m) {
  if (!captureEnabled) return;
  char data[17] = {0};
  for (uint8_t i = 0; i < m.data_length_code; ++i) snprintf(data + i * 2, 3, "%02X", m.data[i]);
  char line[64];
  snprintf(line, sizeof(line), "%s %08lX#%s\n", isoTimestamp().c_str(),
           static_cast<unsigned long>(m.identifier), data);
  if (captureBuffer.length() + strlen(line) > 8192) flushCapture();
  captureBuffer += line;
  if (captureBuffer.length() >= CAPTURE_FLUSH_BYTES) flushCapture();
}

void decodeJ1939(const twai_message_t &m) {
  if (!m.extd || m.data_length_code != 8) return;
  const uint32_t pgn = (m.identifier >> 8) & 0x3FFFF;
  switch (pgn) {
    case 61444: { const uint16_t raw = le16(m.data, 3); if (raw != 0xFFFF) engineRPM = raw * 0.125f; break; }
    case 65265: vehicleSpeed = le16(m.data, 1) * 0.00390625f * 0.621371f; break;
    case 65262: coolantTemp = (m.data[0] - 40) * 1.8f + 32; break;
    case 65263: oilPressure = m.data[3] * 4.0f * 0.145038f; break;
    case 65272: transTemp = (le16(m.data, 4) * 0.03125f - 273.0f) * 1.8f + 32; break;
    case 65198: airPrimary = m.data[2] * 8.0f * 0.145038f; airSecondary = m.data[3] * 8.0f * 0.145038f; break;
    case 65276: fuelLevel = m.data[1] * 0.4f; break;
    case 65271: batteryVoltage = le16(m.data, 4) * 0.05f; break;
    case 65270: turboBoostPSI = m.data[1] * 2.0f * 0.145038f; break;
    case 65266: fuelRateGPH = le16(m.data, 0) * 0.05f * 0.264172f; break;
    case 61443: engineLoadPct = m.data[2]; break;
    case 61445: selectedGearRaw = m.data[0]; break;
    case 65252: lampWaitToStart = ((m.data[1] >> 6) & 3) == 1; break;
    case 65248: { const uint32_t raw = le32(m.data, 4); if (raw != 0xFFFFFFFF) totalOdometerMiles = raw * 0.125f * 0.621371f; break; }
    case 65226: {
      lampMIL = (m.data[0] >> 6) & 3; lampRedStop = (m.data[0] >> 4) & 3;
      lampAmberWarning = (m.data[0] >> 2) & 3; lampProtect = m.data[0] & 3;
      const uint32_t spn = m.data[2] | (m.data[3] << 8) | ((m.data[4] & 0xE0) << 11);
      activeSPN = (spn == 524287) ? 0 : spn; activeFMI = m.data[4] & 0x1F;
      logDiagnostic(activeSPN, activeFMI);
      break;
    }
  }
}

void broadcastState() {
  if (millis() - lastBroadcastMs < 200) return;
  lastBroadcastMs = millis();
  String json = "{\"rpm\":" + String(engineRPM, 0) + ",\"speed\":" + String(vehicleSpeed, 0) +
    ",\"coolant\":" + String(coolantTemp, 0) + ",\"oil\":" + String(oilPressure, 0) +
    ",\"air1\":" + String(airPrimary, 0) + ",\"air2\":" + String(airSecondary, 0) +
    ",\"volt\":" + String(batteryVoltage, 1) + ",\"fuel\":" + String(fuelLevel, 0) +
    ",\"gear\":" + String(selectedGearRaw) + ",\"odo\":" + String(totalOdometerMiles) +
    ",\"spn\":" + String(activeSPN) + ",\"fmi\":" + String(activeFMI) +
    ",\"capture\":" + String(captureEnabled ? "true" : "false") + ",\"sd\":" + String(sdReady ? "true" : "false") +
    ",\"packets\":" + String(receivedPackets) + ",\"dropped\":" + String(droppedPackets) + "}";
  ws.textAll(json);
}

void setupWeb() {
  ws.onEvent([](AsyncWebSocket *, AsyncWebSocketClient *, AwsEventType, void *, uint8_t *, size_t) {});
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r) { r->send_P(200, "text/html", index_html); });
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "application/json", String("{\"sd\":") + (sdReady ? "true" : "false") + ",\"capture\":" + (captureEnabled ? "true" : "false") + "}");
  });
  server.on("/api/capture", HTTP_POST, [](AsyncWebServerRequest *r) {
    const bool enable = r->hasParam("enabled", true) && r->getParam("enabled", true)->value() == "true";
    if (!setCapture(enable)) { r->send(503, "application/json", "{\"error\":\"SD card unavailable\"}"); return; }
    r->send(200, "application/json", String("{\"capture\":") + (captureEnabled ? "true" : "false") + "}");
  });
  server.on("/diagnostics.csv", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!sdReady || !SD_MMC.exists("/diagnostics.csv")) { r->send(404, "text/plain", "No diagnostic log on SD card."); return; }
    r->send(SD_MMC, "/diagnostics.csv", "text/csv", true);
  });
  server.on("/captures", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!sdReady) { r->send(503, "text/plain", "SD card unavailable."); return; }
    String list; File dir = SD_MMC.open("/captures"); File f;
    while ((f = dir.openNextFile())) { list += String(f.name()) + "\n"; f.close(); }
    r->send(200, "text/plain", list);
  });
  server.begin();
}

void setup() {
  Serial.begin(115200);
  mountStorage();
  WiFi.softAP("TruckDash", "12345678"); // Change this before road use.
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_LISTEN_ONLY);
  g.rx_queue_len = 256;
  canReady = twai_driver_install(&g, &TWAI_TIMING_CONFIG_250KBITS(), &TWAI_FILTER_CONFIG_ACCEPT_ALL()) == ESP_OK && twai_start() == ESP_OK;
  setupWeb();
}

void loop() {
  twai_message_t message;
  while (canReady && twai_receive(&message, 0) == ESP_OK) {
    receivedPackets++;
    appendCapture(message);
    decodeJ1939(message);
  }
  broadcastState();
  ws.cleanupClients();
}
