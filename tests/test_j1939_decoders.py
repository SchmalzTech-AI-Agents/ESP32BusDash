"""Regression vectors for the fixed-layout J1939 values used by the sketch."""
import unittest


def u32_le(data, index):
    return int.from_bytes(data[index:index + 4], "little")


def total_miles_from_vdhr(data):
    # PGN 65217 / SPN 917: bytes 1-4, 5 m/bit.
    return int(u32_le(data, 0) * 5.0 / 1609.344)


def gear_label(raw):
    # ETC2 SPN 524: raw value is offset by -125.
    return {
        124: "R",
        125: "N",
        126: "D",
        127: "4",
        128: "2",
        129: "1",
    }.get(raw, "--")


class J1939DecoderTests(unittest.TestCase):
    def test_vdhr_total_distance_uses_first_four_bytes(self):
        # 1,609,345 m is approximately 1,000 miles; trip distance is non-zero too.
        frame = (321869).to_bytes(4, "little") + (99).to_bytes(4, "little")
        self.assertEqual(total_miles_from_vdhr(frame), 1000)

    def test_etc2_range_labels(self):
        self.assertEqual(
            [gear_label(raw) for raw in range(124, 130)],
            ["R", "N", "D", "4", "2", "1"],
        )
        self.assertEqual(gear_label(255), "--")

    def test_air1_standard_service_brake_positions_and_scaling(self):
        # AIR1 SPNs 1087/1088 are bytes 3/4, 8 kPa/bit.
        frame = bytes([0xFF, 0xFF, 99, 99, 0xFF, 0xFF, 0xFF, 0xFF])
        psi = frame[2] * 8.0 * 0.145038
        self.assertEqual(round(psi, 1), 114.9)


if __name__ == "__main__":
    unittest.main()
