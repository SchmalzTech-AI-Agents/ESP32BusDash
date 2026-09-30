#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s_std.h>
#include "cyd_audio.h"
#include "cyd35_pins.h"

namespace {
constexpr uint8_t ES8311_ADDRESS = 0x18;
constexpr uint32_t SAMPLE_RATE = 16000;
constexpr gpio_num_t I2S_MCLK = GPIO_NUM_17;
constexpr gpio_num_t I2S_BCLK = GPIO_NUM_18;
constexpr gpio_num_t I2S_DOUT = GPIO_NUM_15;
constexpr gpio_num_t I2S_WS = GPIO_NUM_21;

i2s_chan_handle_t txChannel = nullptr;

bool codecWrite(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(ES8311_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool initCodec() {
  // Exact ES8311 16-bit/16 kHz setup derived from the manufacturer's music
  // demo. The codec shares I2C0 (GPIO38/39) with the touch controller.
  const struct { uint8_t reg; uint8_t value; } setup[] = {
      {0x00, 0x1F}, {0x00, 0x00}, {0x00, 0x80},
      {0x01, 0x3F}, {0x02, 0x48}, {0x03, 0x10}, {0x04, 0x10},
      {0x05, 0x00}, {0x06, 0x03}, {0x07, 0x00}, {0x08, 0xFF},
      {0x09, 0x0C}, {0x0A, 0x0C}, {0x0D, 0x01}, {0x0E, 0x02},
      {0x12, 0x00}, {0x13, 0x10}, {0x1C, 0x6A}, {0x31, 0x00},
      {0x32, 0x70}, {0x37, 0x08},
  };
  for (const auto &entry : setup) {
    if (!codecWrite(entry.reg, entry.value)) return false;
    if (entry.reg == 0x00 && entry.value == 0x1F) delay(20);
  }
  return true;
}

bool initI2s() {
  i2s_chan_config_t channelConfig = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  channelConfig.auto_clear = true;
  if (i2s_new_channel(&channelConfig, &txChannel, nullptr) != ESP_OK) return false;

  i2s_std_config_t config = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
      .gpio_cfg = {
          .mclk = I2S_MCLK,
          .bclk = I2S_BCLK,
          .ws = I2S_WS,
          .dout = I2S_DOUT,
          .din = I2S_GPIO_UNUSED,
          .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false},
      },
  };
  config.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;
  if (i2s_channel_init_std_mode(txChannel, &config) != ESP_OK || i2s_channel_enable(txChannel) != ESP_OK) {
    i2s_del_channel(txChannel);
    txChannel = nullptr;
    return false;
  }
  return true;
}
}  // namespace

bool cydPlayBootTone() {
  Serial.println("CYD: initializing ES8311 speaker boot tone");
  // The manufacturer examples explicitly pull AP_ENABLE (GPIO1) LOW before
  // initializing ES8311. Without this, I2S and the codec can work normally
  // while the board's amplified speaker/output path remains muted.
  pinMode(Cyd35Pins::AUDIO_AMP_ENABLE, OUTPUT);
  digitalWrite(Cyd35Pins::AUDIO_AMP_ENABLE, LOW);
  if (!initCodec()) {
    Serial.println("CYD: ES8311 codec was not detected at I2C 0x18");
    return false;
  }
  if (!initI2s()) {
    Serial.println("CYD: I2S speaker initialization failed");
    return false;
  }

  // 800 Hz triangle wave, 0.5 s, stereo 16-bit. It is generated locally so
  // boot validation does not depend on SD storage, CAN traffic, or Wi-Fi.
  int16_t frames[256 * 2];
  uint16_t phase = 0;
  constexpr uint16_t PHASE_STEP = 3277;  // 800 Hz at a 16 kHz sample rate.
  constexpr uint32_t TOTAL_FRAMES = SAMPLE_RATE / 2;
  for (uint32_t written = 0; written < TOTAL_FRAMES; written += 256) {
    const uint32_t count = min<uint32_t>(256, TOTAL_FRAMES - written);
    for (uint32_t i = 0; i < count; ++i) {
      const uint16_t ramp = (phase & 0x8000) ? static_cast<uint16_t>(0xFFFF - phase) : phase;
      const int16_t sample = static_cast<int16_t>((static_cast<int32_t>(ramp) - 16384) * 2);
      frames[i * 2] = sample;
      frames[i * 2 + 1] = sample;
      phase += PHASE_STEP;
    }
    size_t bytesWritten = 0;
    if (i2s_channel_write(txChannel, frames, count * sizeof(int16_t) * 2, &bytesWritten, 1000) != ESP_OK) {
      Serial.println("CYD: speaker tone write failed");
      i2s_channel_disable(txChannel);
      i2s_del_channel(txChannel);
      txChannel = nullptr;
      return false;
    }
  }
  i2s_channel_disable(txChannel);
  i2s_del_channel(txChannel);
  txChannel = nullptr;
  Serial.println("CYD: speaker boot tone completed");
  return true;
}
