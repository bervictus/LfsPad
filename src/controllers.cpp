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
struct NativeController {
  SDL_Gamepad *gamepad;
  SDL_Joystick *joystick;
  std::uint32_t vidpid;
  SDL_GamepadType type;
  SDL_Haptic *haptic = nullptr;
};
static std::mutex controllerMutex, nativeDevicesMutex;
static std::map<SDL_JoystickID, NativeController> controllers;
static std::vector<std::uint32_t> nativeDevices;

static HMODULE xinputModule() {
  static const HMODULE module = [] {
    for (const auto *dll : {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"}) {
      if (auto handle = loadSystemLibrary(dll))
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
  return std::uint32_t(SDL_GetJoystickVendorForID(id)) |
         (std::uint32_t(SDL_GetJoystickProductForID(id)) << 16);
}

static bool useNativeBackend(SDL_JoystickID id, bool xinputConnected) {
  const auto product = vidPid(id);
  const auto *path = SDL_GetJoystickPathForID(id);
  const auto type = SDL_GetRealGamepadTypeForID(id);
  // Avoid representing a single XUSB device twice. A SDL mapping that
  // merely uses Xbox-style button names does not make it an XUSB device.
  if (product == 0xc21f046d ||
      (path && (std::strstr(path, "IG_") || std::strstr(path, "ig_"))))
    return false;
  if (xinputConnected && (type == SDL_GAMEPAD_TYPE_XBOX360 ||
                          type == SDL_GAMEPAD_TYPE_XBOXONE))
    return false;
  return SDL_IsGamepad(id) || SDL_GetJoystickTypeForID(id) == SDL_JOYSTICK_TYPE_GAMEPAD;
}

void updateNativeControllers(double time) {
  std::lock_guard<std::mutex> lock(controllerMutex);
  static const bool ready = [] {
    // SDL's HID backend looks these up by basename. Load the system copies
    // before initializing it so the current directory cannot supply them.
    for (const auto *dll : {L"hid.dll", L"cfgmgr32.dll"}) {
      if (!loadSystemLibrary(dll)) {
        log("Windows HID libraries unavailable; native gamepad input disabled");
        return false;
      }
    }
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
      log("Native gamepad initialization failed: %s", SDL_GetError());
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

  for (auto it = controllers.begin(); it != controllers.end();) {
    const auto &device = it->second;
    if (SDL_JoystickConnected(device.joystick)) {
      ++it;
      continue;
    }
    if (device.haptic)
      SDL_CloseHaptic(device.haptic);
    if (device.gamepad)
      SDL_CloseGamepad(device.gamepad);
    else
      SDL_CloseJoystick(device.joystick);
    it = controllers.erase(it);
  }

  XINPUT_STATE state{};
  bool xinputConnected = false;
  for (int slot = 0; slot < 4 && !xinputConnected; ++slot)
    xinputConnected = readXInputController(slot, state) == ERROR_SUCCESS;
  int count = 0;
  SDL_JoystickID *ids = SDL_GetJoysticks(&count);
  for (int i = 0; ids && i < count; ++i) {
    const auto id = ids[i];
    if (controllers.count(id) || !useNativeBackend(id, xinputConnected))
      continue;
    SDL_Gamepad *gamepad = SDL_IsGamepad(id) ? SDL_OpenGamepad(id) : nullptr;
    SDL_Joystick *joystick = gamepad ? SDL_GetGamepadJoystick(gamepad)
                                    : SDL_OpenJoystick(id);
    if (!joystick)
      continue;
    const auto product = vidPid(id);
    controllers.emplace(id, NativeController{gamepad, joystick, product,
                         SDL_GetRealGamepadTypeForID(id)});
    log("Native gamepad: VID=%04x PID=%04x mapped=%d", product & 0xffff,
        product >> 16, gamepad != nullptr);
  }
  SDL_free(ids);

  std::lock_guard<std::mutex> nativeLock(nativeDevicesMutex);
  nativeDevices.clear();
  for (const auto &entry : controllers)
    if (entry.second.gamepad)
      nativeDevices.push_back(entry.second.vidpid);
}

int nativeControllerCount() {
  std::lock_guard<std::mutex> lock(controllerMutex);
  return int(std::count_if(controllers.begin(), controllers.end(),
                          [](const auto &entry) { return entry.second.gamepad != nullptr; }));
}

ControllerId soleUnmappedController() {
  std::lock_guard<std::mutex> lock(controllerMutex);
  if (controllers.size() != 1 || controllers.begin()->second.gamepad ||
      !SDL_JoystickConnected(controllers.begin()->second.joystick))
    return -1;
  return ControllerId(controllers.begin()->first) + 4;
}

bool controllerUsesPlayStationButtons(ControllerId controller) {
  if (!isNativeController(controller))
    return false;
  std::lock_guard<std::mutex> lock(controllerMutex);
  const auto found = controllers.find(joystickId(controller));
  if (found == controllers.end())
    return false;
  const auto type = found->second.type;
  return type == SDL_GAMEPAD_TYPE_PS3 || type == SDL_GAMEPAD_TYPE_PS4 ||
         type == SDL_GAMEPAD_TYPE_PS5;
}

ControllerId controllerForPreference(int preference, ControllerId previous) {
  if (preference != nativePreference)
    return preference;
  std::lock_guard<std::mutex> lock(controllerMutex);
  const auto found = isNativeController(previous)
                         ? controllers.find(joystickId(previous)) : controllers.end();
  if (found != controllers.end() && found->second.gamepad &&
      SDL_GamepadConnected(found->second.gamepad))
    return previous;
  for (const auto &entry : controllers)
    if (entry.second.gamepad && SDL_GamepadConnected(entry.second.gamepad))
      return ControllerId(entry.first) + 4;
  return -1;
}

DWORD readController(ControllerId controller, XINPUT_STATE &state) {
  if (!isNativeController(controller))
    return readXInputController(int(controller), state);
  state = {};
  std::lock_guard<std::mutex> lock(controllerMutex);
  const auto found = controllers.find(joystickId(controller));
  if (found == controllers.end() || !found->second.gamepad ||
      !SDL_GamepadConnected(found->second.gamepad))
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

ControllerId automaticRumbleController(ControllerId fallback,
                                       ControllerId previous, float deadzone) {
  const int threshold = int(deadzone * 32767);
  RumbleControllerChoice choice{previous};
  XINPUT_STATE state{};
  for (int i = 0; i < 4; ++i)
    if (readXInputController(i, state) == ERROR_SUCCESS)
      choice.observe(i, drivingInputActive(state.Gamepad.sThumbLX, threshold,
                                            state.Gamepad.bLeftTrigger, state.Gamepad.bRightTrigger));

  std::lock_guard<std::mutex> lock(controllerMutex);
  for (const auto &entry : controllers) {
    auto *gamepad = entry.second.gamepad;
    if (!gamepad || !SDL_GamepadConnected(gamepad))
      continue;
    choice.observe(ControllerId(entry.first) + 4, drivingInputActive(
        SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX), threshold,
        controllerTrigger(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER)),
        controllerTrigger(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER))));
  }
  return choice.select(fallback);
}

static bool rumbleNative(NativeController &device, std::uint16_t left, std::uint16_t right) {
  if (device.gamepad)
    return SDL_RumbleGamepad(device.gamepad, left, right, rumbleTimeout);
  if (!device.haptic) {
    if (SDL_RumbleJoystick(device.joystick, left, right, rumbleTimeout))
      return true;
    if (!left && !right)
      return false;
    // SDL's force feedback API applies to raw joysticks. The handle is
    // opened from the selected joystick, never from a system-wide index.
    if (!SDL_WasInit(SDL_INIT_HAPTIC) && !SDL_InitSubSystem(SDL_INIT_HAPTIC))
      return false;
    auto *haptic = SDL_OpenHapticFromJoystick(device.joystick);
    if (!haptic)
      return false;
    if (!SDL_InitHapticRumble(haptic)) {
      SDL_CloseHaptic(haptic);
      return false;
    }
    device.haptic = haptic;
  }
  if (!left && !right)
    return SDL_StopHapticRumble(device.haptic);
  return SDL_PlayHapticRumble(device.haptic, std::max(left, right) / 65535.f,
                              rumbleTimeout);
}

DWORD setControllerVibration(ControllerId controller, std::uint16_t left,
                             std::uint16_t right) {
  if (isNativeController(controller)) {
    // The watchdog can run while the render thread is inside a device update.
    std::unique_lock<std::mutex> lock(controllerMutex, std::try_to_lock);
    if (!lock.owns_lock())
      return ERROR_BUSY;
    const auto found = controllers.find(joystickId(controller));
    if (found == controllers.end() || !SDL_JoystickConnected(found->second.joystick))
      return ERROR_DEVICE_NOT_CONNECTED;
    return rumbleNative(found->second, left, right) ? ERROR_SUCCESS : ERROR_NOT_SUPPORTED;
  }
  static const auto set = reinterpret_cast<decltype(&XInputSetState)>(
      GetProcAddress(xinputModule(), "XInputSetState"));
  XINPUT_VIBRATION vibration{left, right};
  return set && controller >= 0 ? set(DWORD(controller), &vibration) : ERROR_NOT_SUPPORTED;
}

std::uint32_t controllerVidPid(ControllerId controller) {
  if (!isNativeController(controller))
    return 0;
  std::lock_guard<std::mutex> lock(controllerMutex);
  auto found = controllers.find(joystickId(controller));
  return found != controllers.end() ? found->second.vidpid : 0;
}

bool isMappedNativeDevice(std::uint32_t vidpid) {
  if (!vidpid)
    return false;
  if (knownPlayStationButtonLayout(vidpid) || vidpid == 0x0268054c)
    return true;
  std::lock_guard<std::mutex> lock(nativeDevicesMutex);
  return std::find(nativeDevices.begin(), nativeDevices.end(), vidpid) != nativeDevices.end();
}
} // namespace pad
