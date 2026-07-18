// Part 1: Configuration, Globals, and Fault Logging
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "esp_partition.h"
#include "driver/twai.h"
#include "dashboard.h"

#define RX_PIN GPIO_NUM_4
#define TX_PIN GPIO_NUM_5
#define BUZZER_PIN GPIO_NUM_13

volatile float engineRPM = 0, vehicleSpeed = 0, coolantTemp = 0, fuelLevel = 0;
volatile float oilPressure = 0, transTemp = 0, airPrimary = 0, airSecondary = 0;
volatile float batteryVoltage = 0, turboBoostPSI = 0, fuelRateGPH = 0, engineLoadPct = 0;
volatile uint32_t totalOdometerMiles = 0; 
volatile uint8_t selectedGearRaw = 0; 

volatile uint8_t lampMIL = 0, lampRedStop = 0, lampAmberWarning = 0, lampProtect = 0, lampWaitToStart = 0; 
volatile uint32_t activeSPN = 0;
volatile uint8_t activeFMI = 0;

const float alphaFast = 0.15;
const float alphaSlow = 0.05;
unsigned long lastBroadcastTime = 0;
const unsigned long broadcastInterval = 40; 
bool alarmActive = false;
unsigned long lastBuzzerToggle = 0;
bool buzzerState = false;

uint32_t lastLoggedSPN = 0;
uint8_t lastLoggedFMI = 0;
unsigned long lastLogTime = 0;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

void logFaultToFlash(uint32_t spn, uint8_t fmi) {
  if (spn == 0 || spn == 524287) return;
  if (spn == lastLoggedSPN && fmi == lastLoggedFMI && (millis() - lastLogTime < 10000)) return; 
  
  lastLoggedSPN = spn;
  lastLoggedFMI = fmi;
  lastLogTime = millis();

  File file = LittleFS.open("/faultlog.txt", FILE_APPEND);
  if (file) {
    file.printf("SPN: %d | FMI: %d | Miles: %d\n", spn, fmi, (int)totalOdometerMiles);
    file.close();
    Serial.printf("Saved Fault to LittleFS: SPN %d FMI %d\n", spn, fmi);
  }
}

// Part 2: Partition Auto-Discovery & Initialization

bool autodetectAndMountLittleFS() {
  const char* targetLabel = NULL;
  esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
  while (it != NULL) {
    const esp_partition_t *part = esp_partition_get(it);
    if (strcmp(part->label, "spiffs") == 0 || 
        strcmp(part->label, "littlefs") == 0 || 
        strcmp(part->label, "ffat") == 0 || 
        strcmp(part->label, "storage") == 0 || 
        (strcmp(part->label, "coredump") != 0 && part->type == 0x01 && part->subtype == 0x82)) {
      targetLabel = part->label;
      Serial.printf("Found flash storage block: '%s' (%d bytes)\n", part->label, part->size);
      break;
    }
    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);

  if (targetLabel == NULL) {
    targetLabel = "spiffs"; 
    Serial.println("No named storage found. Using default label 'spiffs'.");
  }
  return LittleFS.begin(true, "/littlefs", 10, targetLabel);
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  if (autodetectAndMountLittleFS()) {
    Serial.println("LittleFS filesystem mounted and active.");
  } else {
    Serial.println("LittleFS critical initialization failure.");
  }

  WiFi.softAP("TruckDash", "12345678");

  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX_PIN, RX_PIN, TWAI_MODE_LISTEN_ONLY);
  g_config.rx_queue_len = 64; 
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS(); 
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK && twai_start() == ESP_OK) {
    Serial.println("Unified J1939 Transceiver Pipeline Online.");
  } else {
    Serial.println("TWAI Hardware Init Critical Fault.");
  }

  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
  server.on("/faults", HTTP_GET, [](AsyncWebServerRequest *request){
    if (LittleFS.exists("/faultlog.txt")) {
      request->send(LittleFS, "/faultlog.txt", "text/plain");
    } else {
      request->send(200, "text/plain", "No saved fault logs found in Flash memory.");
    }
  });
  server.on("/clearfaults", HTTP_GET, [](AsyncWebServerRequest *request){
    LittleFS.remove("/faultlog.txt");
    lastLoggedSPN = 0; lastLoggedFMI = 0;
    request->send(200, "text/plain", "Flash Log File Wiped Successfully.");
  });
  server.begin();
}

//Part 3: Main Processing Loop (Bus Decoders)

void loop() {
  twai_message_t message;
  
  while (twai_receive(&message, pdMS_TO_TICKS(2)) == ESP_OK) {
    if (!message.extd || message.data_length_code != 8) continue;
    uint32_t pgn = (message.identifier >> 8) & 0x3FFFF;
    
    switch(pgn) {
      case 61444:
        {
          float raw = ((message.data[1] << 8) | message.data[0]) * 0.125;
          if (raw <= 8000.0) engineRPM = (alphaFast * raw) + ((1.0 - alphaFast) * engineRPM);
        }
        break;
      case 65265:
        {
          float raw = (((message.data[2] << 8) | message.data[1]) * 0.00390625) * 0.621371;
          if (raw <= 120.0) vehicleSpeed = (alphaFast * raw) + ((1.0 - alphaFast) * vehicleSpeed);
        }
        break;
      case 65262:
        {
          float raw = ((message.data[0] - 40) * 1.8) + 32;
          if (raw > -40.0 && raw < 300.0) coolantTemp = (alphaSlow * raw) + ((1.0 - alphaSlow) * coolantTemp);
        }
        break;
      case 65263:
        {
          float raw = (message.data[3] * 4.0) * 0.145038;
          if (raw <= 150.0) oilPressure = (alphaFast * raw) + ((1.0 - alphaFast) * oilPressure);
        }
        break;
      case 65272:
        {
          float raw = (((message.data[5] << 8) | message.data[4]) * 0.03125 - 273.0) * 1.8 + 32;
          if (raw > -40.0 && raw < 400.0) transTemp = (alphaSlow * raw) + ((1.0 - alphaSlow) * transTemp);
        }
        break;
      case 65257:
        {
          float raw1 = (message.data[0] * 4.0) * 0.145038;
          float raw2 = (message.data[1] * 4.0) * 0.145038;
          if (raw1 <= 200.0) airPrimary = (alphaFast * raw1) + ((1.0 - alphaFast) * airPrimary);
          if (raw2 <= 200.0) airSecondary = (alphaFast * raw2) + ((1.0 - alphaFast) * airSecondary);
        }
        break;
      case 65276:
        {
          float raw = message.data[1] * 0.4;
          if (raw <= 100.0) fuelLevel = (alphaSlow * raw) + ((1.0 - alphaSlow) * fuelLevel);
        }
        break;
      case 65271:
        {
          float raw = ((message.data[5] << 8) | message.data[4]) * 0.05;
          if (raw > 5.0 && raw < 32.0) batteryVoltage = (alphaSlow * raw) + ((1.0 - alphaSlow) * batteryVoltage);
        }
        break;
      case 65270:
        {
          float rawKpa = message.data[1] * 2.0; 
          float rawPSI = rawKpa * 0.145038;
          if (rawPSI <= 60.0) turboBoostPSI = (alphaFast * rawPSI) + ((1.0 - alphaFast) * turboBoostPSI);
        }
        break;
      case 65266:
        {
          float rawLph = ((message.data[1] << 8) | message.data[0]) * 0.05; 
          float rawGph = rawLph * 0.264172; 
          if (rawGph <= 50.0) fuelRateGPH = (alphaSlow * rawGph) + ((1.0 - alphaSlow) * fuelRateGPH);
        }
        break;
      case 61443:
        {
          float rawPct = message.data[2]; 
          if (rawPct <= 100.0) engineLoadPct = (alphaFast * rawPct) + ((1.0 - alphaFast) * engineLoadPct);
        }
        break;
      case 61445:
        selectedGearRaw = message.data[3]; 
        break;
      case 65252:
        lampWaitToStart = (((message.data[1] >> 6) & 0x03) == 0x01) ? 1 : 0;
        break;
      case 65248:
        {
          uint32_t rawKm = ((uint32_t)message.data[3] << 24) | ((uint32_t)message.data[2] << 16) | 
                           ((uint32_t)message.data[1] << 8)  | message.data[0];
          if (rawKm != 0xFFFFFFFF && rawKm > 0) totalOdometerMiles = (rawKm * 0.125) * 0.621371;
        }
        break;
      case 65226:
        lampMIL          = (message.data[0] >> 6) & 0x03;
        lampRedStop      = (message.data[0] >> 4) & 0x03;
        lampAmberWarning = (message.data[0] >> 2) & 0x03;
        lampProtect      = (message.data[0]) & 0x03;
        uint32_t spn = message.data[2] | (message.data[3] << 8) | ((message.data[4] & 0xE0) << 11);
        uint8_t fmi = message.data[4] & 0x1F;
        
        if (spn == 0 || spn == 524287) { 
          activeSPN = 0; activeFMI = 0; 
        } else { 
          activeSPN = spn; activeFMI = fmi; 
          logFaultToFlash(spn, fmi); 
        }
        break;
    }
  }

  if (engineRPM > 400) {
    alarmActive = (airPrimary < 90.0 || airSecondary < 90.0 || coolantTemp > 220.0 || (batteryVoltage < 11.8 && batteryVoltage > 5.0));
  } else { alarmActive = false; }

  if (alarmActive) {
    unsigned long cur = millis();
    if (cur - lastBuzzerToggle >= 150) {
      lastBuzzerToggle = cur; buzzerState = !buzzerState;
      digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
    }
  } else { digitalWrite(BUZZER_PIN, LOW); }

  if (millis() - lastBroadcastTime >= broadcastInterval) {
    lastBroadcastTime = millis();
    if (ws.count() > 0) {
      String json = "{";
      json += "\"rpm\":" + String(engineRPM) + ",\"speed\":" + String(vehicleSpeed) + ",";
      json += "\"boost\":" + String(turboBoostPSI) + ",\"gph\":" + String(fuelRateGPH) + ",";
      json += "\"gear\":" + String(selectedGearRaw) + ",\"load\":" + String(engineLoadPct) + ","; 
      json += "\"oil\":" + String(oilPressure) + ",\"coolant\":" + String(coolantTemp) + ",";
      json += "\"trans\":" + String(transTemp) + ",\"air1\":" + String(airPrimary) + ",";
      json += "\"air2\":" + String(airSecondary) + ",\"volt\":" + String(batteryVoltage) + ",";
      json += "\"odo\":" + String(totalOdometerMiles) + ",\"fuel\":" + String(fuelLevel) + ",";
      json += "\"lMIL\":" + String(lampMIL) + ",\"lRED\":" + String(lampRedStop) + ",";
      json += "\"lAMB\":" + String(lampAmberWarning) + ",\"lPRT\":" + String(lampProtect) + ",";
      json += "\"lWTS\":" + String(lampWaitToStart) + ",\"spn\":" + String(activeSPN) + ",\"fmi\":" + String(activeFMI);
      json += "}";
      ws.textAll(json);
    }
    ws.cleanupClients();
  }
}


