#include "controller_axes.hpp"
#include "controllers.hpp"
#include <SDL3/SDL.h>
#include <cstdlib>
#include <iostream>
#include <string>

namespace pad {
// The backend normally logs through the DLL. The test runs with no LFS process.
std::wstring path(const wchar_t *name) { return name; }
void log(const char *, ...) {}
}

static void check(bool ok, const char *message) {
  if (!ok) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

struct MotorOutput {
  Uint16 low = 0;
  Uint16 high = 0;
};

static bool SDLCALL rumble(void *context, Uint16 low, Uint16 high) {
  auto &motors = *static_cast<MotorOutput *>(context);
  motors = {low, high};
  return true;
}

int main() {
  using namespace pad;
  check(controllerY(-32768) == 32767 && controllerY(32767) == -32767 &&
            controllerY(0) == 0, "SDL up becomes XInput up");
  check(controllerTrigger(-1) == 0 && controllerTrigger(0) == 0 &&
            controllerTrigger(16384) == 128 && controllerTrigger(32767) == 255,
        "SDL triggers use the whole XInput range");
  check(knownPlayStationButtonLayout(0x09cc054c) && knownPlayStationButtonLayout(0x0df2054c) &&
            !knownPlayStationButtonLayout(0x0268054c) &&
            playStationButtonIndex(0) == 2 && playStationButtonIndex(1) == 0 &&
            playStationButtonIndex(8) == 6 && playStationButtonIndex(11) == 9,
        "native Sony button order is applied only to the matching device layouts");

  updateNativeControllers(0.0);
  check(SDL_WasInit(SDL_INIT_GAMEPAD) != 0, "SDL gamepad subsystem started");
  const int existing = nativeControllerCount();
  struct Device {
    Uint16 vendor;
    Uint16 product;
    bool playStation;
    bool hasRumble;
  };
  const Device devices[]{
      {0x054c, 0x0268, true, true}, {0x054c, 0x05c4, true, true},
      {0x054c, 0x0ce6, true, true}, {0x054c, 0x0df2, true, true},
      {0x046d, 0xc219, false, true}, // F710 in DirectInput mode
      {0x057e, 0x2009, false, true}, // Switch Pro
      {0x2dc8, 0x6001, false, true}, // 8BitDo
      {0x2dc8, 0x6001, false, false}
  };
  check(!drivingInputActive(2000, 7500, 7, 0) && drivingInputActive(-8500, 7500, 0, 0) &&
        drivingInputActive(0, 7500, 0, 200), "selection ignores drift and reacts to steering and pedals");
  RumbleControllerChoice secondPad{0};
  secondPad.observe(0, false);
  secondPad.observe(1, true);
  check(secondPad.select(0) == 1, "second F710 can take over vibration in automatic mode");
  RumbleControllerChoice bothIdle{1};
  bothIdle.observe(0, false);
  bothIdle.observe(1, false);
  check(bothIdle.select(0) == 1, "vibration stays on the last driver when sticks are released");
  RumbleControllerChoice disconnected{1};
  disconnected.observe(0, false);
  check(disconnected.select(0) == 0, "vibration returns to a connected controller after disconnect");
  double time = 0;
  for (const auto &device : devices) {
    MotorOutput motors;
    SDL_VirtualJoystickDesc descriptor{};
    SDL_INIT_INTERFACE(&descriptor);
    descriptor.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    descriptor.vendor_id = device.vendor;
    descriptor.product_id = device.product;
    descriptor.naxes = SDL_GAMEPAD_AXIS_COUNT;
    descriptor.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    descriptor.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
    descriptor.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
    descriptor.name = "LfsPad test pad";
    descriptor.userdata = &motors;
    descriptor.Rumble = device.hasRumble ? rumble : nullptr;

    const auto joystickId = SDL_AttachVirtualJoystick(&descriptor);
    check(joystickId != 0, "virtual controller attached");
    auto *joystick = SDL_OpenJoystick(joystickId);
    check(joystick != nullptr, "virtual joystick state is writable");
    const ControllerId id = ControllerId(joystickId) + 4;
    updateNativeControllers(time += 1);
    check(nativeControllerCount() == existing + 1, "native controller discovered");
    check(controllerForPreference(nativePreference, id) == id,
          "selection keeps a connected native controller");
    check(controllerVidPid(id) == (std::uint32_t(device.product) << 16 | device.vendor),
          "gamepad is matched to its native DirectInput device");
    check(controllerUsesPlayStationButtons(id) == device.playStation,
          "Sony button captions are limited to the PlayStation layout");

    check(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, true) &&
              SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_DPAD_UP, true) &&
              SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTY, -32768) &&
              SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 0) &&
              SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767),
          "virtual pad sends buttons and distinct simultaneous triggers");
    updateNativeControllers(time += 1);
    XINPUT_STATE state{};
    const auto readResult = readController(id, state);
    if (readResult != ERROR_SUCCESS ||
        state.Gamepad.wButtons != (XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_DPAD_UP) ||
        state.Gamepad.sThumbLY != 32767 || state.Gamepad.bRightTrigger != 255 ||
        state.Gamepad.bLeftTrigger < 127 || state.Gamepad.bLeftTrigger > 128) {
      std::cerr << "Virtual pad " << std::hex << device.product << std::dec
                << " returned " << readResult << ", buttons=" << state.Gamepad.wButtons
                << ", Y=" << state.Gamepad.sThumbLY
                << ", RT=" << int(state.Gamepad.bRightTrigger)
                << ", LT=" << int(state.Gamepad.bLeftTrigger) << '\n';
      check(false, "normalized input reaches existing menu and pedal code");
    }
    const auto result = setControllerVibration(id, 1234, 4321);
    if (device.hasRumble) {
      check(result == ERROR_SUCCESS && motors.low == 1234 && motors.high == 4321,
            "rumble reaches the selected native controller");
      check(setControllerVibration(id, 0, 0) == ERROR_SUCCESS &&
                motors.low == 0 && motors.high == 0, "rumble stops");
    } else {
      check(result == ERROR_NOT_SUPPORTED, "controller rejects unavailable vibration");
    }

    SDL_CloseJoystick(joystick);
    check(SDL_DetachVirtualJoystick(joystickId), "virtual controller disconnected");
    updateNativeControllers(time += 1);
    check(nativeControllerCount() == existing &&
              readController(id, state) == ERROR_DEVICE_NOT_CONNECTED &&
              state.Gamepad.wButtons == 0 && state.Gamepad.bRightTrigger == 0,
          "disconnect clears state and removes the device");
  }
  std::cout << "Virtual PlayStation, Logitech, Nintendo and third-party gamepad checks passed\n";
}
