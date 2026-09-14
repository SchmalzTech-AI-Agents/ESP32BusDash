# ESP32BusDash — ESP32-S3 3.5-inch CYD port

Read-only J1939 instrument dashboard for the ESP32-S3 3.5-inch CYD labeled **ST77922 / IPS / 320x480 / CTP**. The hardware profile selects a 480x320 landscape UI and keeps GPIO 45/46 free for CAN.

## Implemented

- J1939 dashboard via Wi-Fi at `http://192.168.4.1` (AP: `TruckDash`; change the default password before use).
- Automatic DM1 diagnostic-code logging to `/diagnostics.csv` on the SD card.
- In-dashboard toggle to capture every received CAN frame to an SD `candump`-compatible text log. Capture files appear in `/captures`.
- Packet count, SD state, and capture write-failure counter.
- ESP32 TWAI is locked to **listen-only** mode; it never transmits or acknowledges traffic.
- CAN pins: **GPIO45 = RX**, **GPIO46 = TX**.
- CYD electrical map in `include/cyd35_pins.h`: ST77922 QSPI display, CTP I2C, SD_MMC 1-bit card socket, and sound hardware pins kept untouched.

## Wiring and safety

The ESP32-S3 pins are logic-level signals, not CAN. Use an isolated or automotive-rated **3.3 V CAN transceiver** between the truck CAN-H/CAN-L wires and the CYD:

- transceiver RXD → CYD GPIO45
- transceiver TXD ← CYD GPIO46
- shared logic ground only as specified by the transceiver/vehicle interface

Do not connect vehicle CAN directly to GPIO45/46. Do not use a 5 V-only TJA1050 module without level shifting. The firmware is listener-only, but physical bus wiring still must follow the truck service documentation.

## Build

```sh
pio run -e esp32-s3-cyd35
pio run -e esp32-s3-cyd35 -t upload
pio device monitor -b 115200
```

The repository declares all dependencies in `platformio.ini`; no global Arduino library setup is required. Use a 16 MB ESP32-S3 with OPI PSRAM, which is typical for this CYD family.

## Display bring-up note

The ST77922/CTP pin profile is recorded and the project includes `ESP32_Display_Panel` + LVGL dependencies for the native landscape panel. The provided dashboard is currently browser-rendered while the panel UI integration is completed against the exact panel revision. Different CYD batches can ship a different touch-controller firmware or ST77922 initialization sequence; verify the display using the vendor test sketch before flashing into a vehicle.

## Logs

- `diagnostics.csv`: `uptime_s,event,spn,fmi,odometer_mi,source`
- `captures/can_N.log`: `uptime_s CAN_ID#DATA`; each received standard or extended frame is retained. Files use device uptime because no RTC/GNSS time source is assumed.

Remove the SD card only after stopping capture from the dashboard.