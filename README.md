# ESP32BusDash
Instrument cluster web dashboard and CAN Sniffer code reader for IC Bus CE300 with ESP32

The dashboard decodes standard fixed-layout J1939 application PGNs. The primary gauge mappings are:

- PGN 61444 (EEC1): SPN 190 engine speed, bytes 4-5, 0.125 rpm/bit.
- PGN 65198: SPNs 1087/1088 service-brake air pressure, bytes 3/4, 8 kPa/bit.
- PGN 61445 (ETC2): SPN 524 selected gear, byte 1, offset -125.
- PGN 65248: SPN 245 total vehicle distance, bytes 5-8, 0.125 km/bit.
- PGN 65226 (DM1): diagnostic lamp status and active DTCs.

These PGNs are not DM15, DM13, DM16, or DM1 multi-SPN diagnostic containers. Their SPNs are defined at fixed bit/byte positions by the J1939 application-layer message definitions.

<img width="1344" height="2125" alt="Screenshot_20260628-153350" src="https://github.com/user-attachments/assets/b52e53eb-093d-49f8-bdcf-764403aecd6d" />
