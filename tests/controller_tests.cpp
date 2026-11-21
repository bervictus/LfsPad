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

  updatePlayStationControllers(0.0);
  check(SDL_WasInit(SDL_INIT_GAMEPAD) != 0, "SDL gamepad subsystem started");
  const int existing = playStationControllerCount();
  const Uint16 products[]{0x0268, 0x05c4, 0x0ce6, 0x0df2};
  double time = 0;
  for (const Uint16 product : products) {
    MotorOutput motors;
    SDL_VirtualJoystickDesc descriptor{};
    SDL_INIT_INTERFACE(&descriptor);
    descriptor.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    descriptor.vendor_id = 0x054c;
    descriptor.product_id = product;
    descriptor.naxes = SDL_GAMEPAD_AXIS_COUNT;
    descriptor.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    descriptor.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
    descriptor.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
    descriptor.name = "LfsPad test pad";
    descriptor.userdata = &motors;
    descriptor.Rumble = rumble;

    const auto joystickId = SDL_AttachVirtualJoystick(&descriptor);
    check(joystickId != 0, "virtual Sony controller attached");
    auto *joystick = SDL_OpenJoystick(joystickId);
    check(joystick != nullptr, "virtual joystick state is writable");
    const ControllerId id = ControllerId(joystickId) + 4;
    updatePlayStationControllers(time += 1);
    check(playStationControllerCount() == existing + 1, "PlayStation controller discovered");
    check(controllerForPreference(playStationPreference, id) == id,
          "selection keeps a connected PlayStation controller");
    check(controllerVidPid(id) == (std::uint32_t(product) << 16 | 0x054c),
          "PlayStation can be matched to its native DirectInput device");

    check(SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_SOUTH, true) &&
              SDL_SetJoystickVirtualButton(joystick, SDL_GAMEPAD_BUTTON_DPAD_UP, true) &&
              SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFTY, -32768) &&
              SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 0) &&
              SDL_SetJoystickVirtualAxis(joystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767),
          "virtual pad sends buttons and distinct simultaneous triggers");
    updatePlayStationControllers(time += 1);
    XINPUT_STATE state{};
    const auto readResult = readController(id, state);
    if (readResult != ERROR_SUCCESS ||
        state.Gamepad.wButtons != (XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_DPAD_UP) ||
        state.Gamepad.sThumbLY != 32767 || state.Gamepad.bRightTrigger != 255 ||
        state.Gamepad.bLeftTrigger < 127 || state.Gamepad.bLeftTrigger > 128) {
      std::cerr << "Virtual pad " << std::hex << product << std::dec
                << " returned " << readResult << ", buttons=" << state.Gamepad.wButtons
                << ", Y=" << state.Gamepad.sThumbLY
                << ", RT=" << int(state.Gamepad.bRightTrigger)
                << ", LT=" << int(state.Gamepad.bLeftTrigger) << '\n';
      check(false, "normalized input reaches existing menu and pedal code");
    }
    check(setControllerVibration(id, 1234, 4321) == ERROR_SUCCESS &&
              motors.low == 1234 && motors.high == 4321,
          "rumble reaches the selected native controller");
    check(setControllerVibration(id, 0, 0) == ERROR_SUCCESS &&
              motors.low == 0 && motors.high == 0, "rumble stops");

    SDL_CloseJoystick(joystick);
    check(SDL_DetachVirtualJoystick(joystickId), "virtual controller disconnected");
    updatePlayStationControllers(time += 1);
    check(playStationControllerCount() == existing &&
              readController(id, state) == ERROR_DEVICE_NOT_CONNECTED &&
              state.Gamepad.wButtons == 0 && state.Gamepad.bRightTrigger == 0,
          "disconnect clears state and removes the device");
  }
  std::cout << "PlayStation backend passed virtual DS3, DS4, DualSense and Edge checks\n";
}
