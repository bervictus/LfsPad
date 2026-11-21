#include "runtime.hpp"
#include "controller_axes.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstring>
#include <map>
#include <mutex>
#include <utility>
#include <vector>

namespace pad {
struct PlayStationDevice {
  SDL_Gamepad *gamepad;
  std::uint32_t vidpid;
};
static std::mutex controllerMutex, nativeDevicesMutex;
static std::map<SDL_JoystickID, PlayStationDevice> playStations;
static std::vector<std::uint32_t> nativeDevices;

static HMODULE xinputModule() {
  static const HMODULE module = [] {
    wchar_t system[MAX_PATH]{};
    if (!GetSystemDirectoryW(system, MAX_PATH))
      return HMODULE{};
    for (const auto *dll : {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"}) {
      if (auto handle = LoadLibraryW((std::wstring(system) + L"\\" + dll).c_str()))
        return handle;
    }
    return HMODULE{};
  }();
  return module;
}

DWORD readXInputController(int slot, XINPUT_STATE &state) {
  state = {};
  static const auto poll = reinterpret_cast<decltype(&XInputGetState)>(
      GetProcAddress(xinputModule(), "XInputGetState"));
  return slot >= 0 && slot < 4 && poll ? poll(slot, &state) : ERROR_DEVICE_NOT_CONNECTED;
}

static SDL_JoystickID joystickId(ControllerId controller) {
  return SDL_JoystickID(controller - 4);
}

static std::uint32_t vidPid(SDL_JoystickID id) {
  return std::uint32_t(SDL_GetGamepadVendorForID(id)) |
         (std::uint32_t(SDL_GetGamepadProductForID(id)) << 16);
}

static bool isPlayStationOrAdapter(SDL_JoystickID id) {
  const auto type = SDL_GetRealGamepadTypeForID(id);
  if (type == SDL_GAMEPAD_TYPE_PS3 || type == SDL_GAMEPAD_TYPE_PS4 ||
      type == SDL_GAMEPAD_TYPE_PS5)
    return true;
  if (type != SDL_GAMEPAD_TYPE_STANDARD && type != SDL_GAMEPAD_TYPE_UNKNOWN)
    return false;
  const auto mappedType = SDL_GetGamepadTypeForID(id);
  if (mappedType == SDL_GAMEPAD_TYPE_XBOX360 || mappedType == SDL_GAMEPAD_TYPE_XBOXONE)
    return false;
  // Mapped USB adapters often have no manufacturer-specific type. XUSB
  // interfaces and Xbox emulators are already polled through XInput.
  const char *path = SDL_GetGamepadPathForID(id);
  return vidPid(id) != 0xc21f046d &&
         (!path || (!std::strstr(path, "IG_") && !std::strstr(path, "ig_")));
}

void updatePlayStationControllers(double time) {
  std::lock_guard<std::mutex> lock(controllerMutex);
  static const bool ready = [] {
    SDL_SetHint(SDL_HINT_XINPUT_ENABLED, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_GAMEINPUT, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_XBOX, "0");
    const bool sonyDriver = GetPrivateProfileIntW(L"Input", L"PlayStation3SonyDriver", 0,
                                                path(L"LfsPad.ini").c_str()) != 0;
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS3, sonyDriver ? "0" : "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS3_SIXAXIS_DRIVER, sonyDriver ? "1" : "0");
    // Enhanced Bluetooth reports stop native DirectInput reads in LFS.
    SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_ENHANCED_REPORTS, "0", SDL_HINT_OVERRIDE);
    // SDL owns the joystick poll, while LFS owns the Windows message pump.
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    const bool ok = SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    if (ok) {
      SDL_SetGamepadEventsEnabled(false);
      SDL_SetJoystickEventsEnabled(false);
    } else {
      log("PlayStation gamepad initialization failed: %s", SDL_GetError());
    }
    return ok;
  }();
  if (!ready)
    return;

  SDL_UpdateGamepads();
  static double nextScan = 0;
  if (time < nextScan)
    return;
  nextScan = time + .5;

  for (auto it = playStations.begin(); it != playStations.end();) {
    if (!SDL_GamepadConnected(it->second.gamepad)) {
      SDL_CloseGamepad(it->second.gamepad);
      it = playStations.erase(it);
    } else {
      ++it;
    }
  }
  int count = 0;
  SDL_JoystickID *ids = SDL_GetGamepads(&count);
  for (int i = 0; ids && i < count; ++i) {
    const auto id = ids[i];
    if (playStations.count(id) || !isPlayStationOrAdapter(id))
      continue;
    if (auto *gamepad = SDL_OpenGamepad(id)) {
      const auto product = vidPid(id);
      playStations.emplace(id, PlayStationDevice{gamepad, product});
      log("Native gamepad opened: VID=%04x PID=%04x", product & 0xffff, product >> 16);
    }
  }
  SDL_free(ids);

  std::lock_guard<std::mutex> nativeLock(nativeDevicesMutex);
  nativeDevices.clear();
  for (const auto &entry : playStations)
    nativeDevices.push_back(entry.second.vidpid);
}

int playStationControllerCount() {
  std::lock_guard<std::mutex> lock(controllerMutex);
  return int(playStations.size());
}

ControllerId controllerForPreference(int preference, ControllerId previous) {
  if (preference != playStationPreference)
    return preference;
  std::lock_guard<std::mutex> lock(controllerMutex);
  const auto found = isPlayStationController(previous)
                         ? playStations.find(joystickId(previous)) : playStations.end();
  if (found != playStations.end() && SDL_GamepadConnected(found->second.gamepad))
    return previous;
  for (const auto &entry : playStations)
    if (SDL_GamepadConnected(entry.second.gamepad))
      return ControllerId(entry.first) + 4;
  return -1;
}

DWORD readController(ControllerId controller, XINPUT_STATE &state) {
  if (!isPlayStationController(controller))
    return readXInputController(int(controller), state);
  state = {};
  std::lock_guard<std::mutex> lock(controllerMutex);
  const auto found = playStations.find(joystickId(controller));
  if (found == playStations.end() || !SDL_GamepadConnected(found->second.gamepad))
    return ERROR_DEVICE_NOT_CONNECTED;
  auto *gamepad = found->second.gamepad;
  const auto get = [=](SDL_GamepadAxis axis) { return SDL_GetGamepadAxis(gamepad, axis); };
  auto &pad = state.Gamepad;
  pad.sThumbLX = get(SDL_GAMEPAD_AXIS_LEFTX);
  pad.sThumbLY = controllerY(get(SDL_GAMEPAD_AXIS_LEFTY));
  pad.sThumbRX = get(SDL_GAMEPAD_AXIS_RIGHTX);
  pad.sThumbRY = controllerY(get(SDL_GAMEPAD_AXIS_RIGHTY));
  pad.bLeftTrigger = controllerTrigger(get(SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
  pad.bRightTrigger = controllerTrigger(get(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
  static constexpr std::pair<SDL_GamepadButton, WORD> buttons[] = {
      {SDL_GAMEPAD_BUTTON_SOUTH, XINPUT_GAMEPAD_A},
      {SDL_GAMEPAD_BUTTON_EAST, XINPUT_GAMEPAD_B},
      {SDL_GAMEPAD_BUTTON_WEST, XINPUT_GAMEPAD_X},
      {SDL_GAMEPAD_BUTTON_NORTH, XINPUT_GAMEPAD_Y},
      {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, XINPUT_GAMEPAD_LEFT_SHOULDER},
      {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER},
      {SDL_GAMEPAD_BUTTON_START, XINPUT_GAMEPAD_START},
      {SDL_GAMEPAD_BUTTON_BACK, XINPUT_GAMEPAD_BACK},
      {SDL_GAMEPAD_BUTTON_LEFT_STICK, XINPUT_GAMEPAD_LEFT_THUMB},
      {SDL_GAMEPAD_BUTTON_RIGHT_STICK, XINPUT_GAMEPAD_RIGHT_THUMB},
      {SDL_GAMEPAD_BUTTON_DPAD_UP, XINPUT_GAMEPAD_DPAD_UP},
      {SDL_GAMEPAD_BUTTON_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_DOWN},
      {SDL_GAMEPAD_BUTTON_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_LEFT},
      {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, XINPUT_GAMEPAD_DPAD_RIGHT},
  };
  for (const auto &entry : buttons)
    if (SDL_GetGamepadButton(gamepad, entry.first))
      pad.wButtons |= entry.second;
  return ERROR_SUCCESS;
}

DWORD setControllerVibration(ControllerId controller, std::uint16_t left,
                             std::uint16_t right) {
  if (isPlayStationController(controller)) {
    // The watchdog can run while the render thread is inside a device update.
    std::unique_lock<std::mutex> lock(controllerMutex, std::try_to_lock);
    if (!lock.owns_lock())
      return ERROR_BUSY;
    const auto found = playStations.find(joystickId(controller));
    if (found == playStations.end())
      return ERROR_DEVICE_NOT_CONNECTED;
    return SDL_RumbleGamepad(found->second.gamepad, left, right, 160)
               ? ERROR_SUCCESS : ERROR_NOT_SUPPORTED;
  }
  static const auto set = reinterpret_cast<decltype(&XInputSetState)>(
      GetProcAddress(xinputModule(), "XInputSetState"));
  XINPUT_VIBRATION vibration{left, right};
  return set && controller >= 0 ? set(DWORD(controller), &vibration) : ERROR_NOT_SUPPORTED;
}

std::uint32_t controllerVidPid(ControllerId controller) {
  if (!isPlayStationController(controller))
    return 0;
  std::lock_guard<std::mutex> lock(controllerMutex);
  auto found = playStations.find(joystickId(controller));
  return found != playStations.end() ? found->second.vidpid : 0;
}

bool isMappedPlayStationDevice(std::uint32_t vidpid) {
  if (!vidpid)
    return false;
  if (knownPlayStationButtonLayout(vidpid) || vidpid == 0x0268054c)
    return true;
  std::lock_guard<std::mutex> lock(nativeDevicesMutex);
  return std::find(nativeDevices.begin(), nativeDevices.end(), vidpid) != nativeDevices.end();
}
} // namespace pad
