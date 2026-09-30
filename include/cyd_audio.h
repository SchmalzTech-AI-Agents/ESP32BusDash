#pragma once

// Initializes the manufacturer-defined ES8311/I2S audio path and plays one
// short boot test tone. Returns false if the codec or I2S transmitter fails.
bool cydPlayBootTone();
