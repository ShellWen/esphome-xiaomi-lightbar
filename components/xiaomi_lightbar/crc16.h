#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome {
namespace xiaomi_lightbar {

// CRC-16 used by the Xiaomi light bar protocol.
//
// Equivalent to RobTillaart's CRC16(0x1021, 0xFFFE, 0x0000, false, false) that
// the original lightbar2mqtt firmware used:
//   - polynomial 0x1021
//   - initial value 0xFFFE
//   - final XOR 0x0000
//   - no input/output bit reflection (computed MSB-first)
//
// This replaces the external RobTillaart CRC library dependency.
inline uint16_t crc16_lightbar(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFE;
  for (size_t i = 0; i < len; i++) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (crc & 0x8000) {
        crc = static_cast<uint16_t>(crc << 1) ^ 0x1021;
      } else {
        crc = static_cast<uint16_t>(crc << 1);
      }
    }
  }
  return crc;
}

}  // namespace xiaomi_lightbar
}  // namespace esphome
