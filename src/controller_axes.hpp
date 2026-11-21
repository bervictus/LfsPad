#pragma once
#include <cstdint>

namespace pad {
// SDL reports north as negative. XInput uses the full signed range with
// north positive.
inline std::int16_t controllerY(std::int16_t value) {
  return value == -32768 ? 32767 : std::int16_t(-value);
}
inline std::uint8_t controllerTrigger(std::int16_t value) {
  return value <= 0 ? 0 : std::uint8_t((int(value) * 255 + 16383) / 32767);
}
inline bool knownPlayStationButtonLayout(std::uint32_t vidpid) {
  if ((vidpid & 0xffff) != 0x054c)
    return false;
  switch (vidpid >> 16) {
  case 0x05c4: // DualShock 4
  case 0x05c5: // DualShock 4 Strikepad
  case 0x09cc: // DualShock 4 v2
  case 0x0ba0: // DualShock 4 wireless adapter
  case 0x0ce6: // DualSense
  case 0x0df2: // DualSense Edge
    return true;
  default:
    return false;
  }
}
inline int playStationButtonIndex(unsigned instance) {
  // Windows exposes Square, Cross, Circle, Triangle before the shoulders.
  constexpr int indices[]{2, 0, 1, 3, 4, 5, -1, -1, 6, 7, 8, 9};
  return instance < sizeof(indices) / sizeof(*indices) ? indices[instance] : -1;
}
} // namespace pad
