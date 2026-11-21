#pragma once
#include <cstdint>
#include <windows.h>
#include <xinput.h>

namespace pad {
using ControllerId = std::int64_t;
inline constexpr int playStationPreference = 4;
inline constexpr bool isPlayStationController(ControllerId controller) {
  return controller >= 4;
}
void updatePlayStationControllers(double time);
int playStationControllerCount();
ControllerId controllerForPreference(int preference, ControllerId previous);
DWORD readController(ControllerId controller, XINPUT_STATE &state);
DWORD readXInputController(int slot, XINPUT_STATE &state);
DWORD setControllerVibration(ControllerId controller, std::uint16_t left,
                             std::uint16_t right);
std::uint32_t controllerVidPid(ControllerId controller);
bool isMappedPlayStationDevice(std::uint32_t vidpid);
} // namespace pad
