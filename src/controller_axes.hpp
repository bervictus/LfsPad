#pragma once
#include "core.hpp"
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
inline bool drivingInputActive(int steering, int deadzone, int brake, int throttle) {
  return steering < -deadzone || steering > deadzone || brake > 30 || throttle > 30;
}

enum class SteeringCurve : unsigned { Linear, Soft, ExtraSoft };
enum class SteeringHelp : unsigned { Off, Balanced, Strong };

inline constexpr unsigned fullSteering = 1000;

struct SteeringResponse {
  SteeringCurve curve = SteeringCurve::Linear;
  unsigned limit = fullSteering;
  int correction = 0;
  unsigned softening = 0;

  bool active() const {
    return curve != SteeringCurve::Linear || limit != fullSteering || correction || softening;
  }
};

inline std::int32_t curvedSteering(std::int32_t value, std::int32_t minimum,
                                   std::int32_t maximum, SteeringResponse response) {
  if (!response.active() || minimum >= maximum || value < minimum || value > maximum)
    return value;
  const auto centre = axisCenter(minimum, maximum);
  const auto delta = std::int64_t(value) - centre;
  const auto span = delta < 0 ? std::int64_t(centre) - minimum : std::int64_t(maximum) - centre;
  if (!span)
    return value;
  const double position = double(delta) / double(span);
  const double stick = std::abs(position);
  const bool driverCorrecting = position * response.correction > 0.;
  const double restore = driverCorrecting
      ? std::clamp(std::abs(response.correction) / 150., 0., 1.) * std::clamp((stick - .25) / .55, 0., 1.)
      : 0.;
  const double curveSoftness = response.curve == SteeringCurve::Linear ? 0. :
                               response.curve == SteeringCurve::Soft ? .45 : .65;
  const double softness = curveSoftness + (1. - curveSoftness) * response.softening * (1. - restore) / fullSteering;
  double shaped = position * (1. - softness * (1. - position * position));
  const double baseLimit = double(response.limit) / fullSteering;
  const double limit = baseLimit + (1. - baseLimit) * restore;
  // Leave small inputs alone. Compress only the upper part of stick travel
  // and keep the slope continuous where the compression starts.
  constexpr double knee = .42;
  if (limit < 1. && std::abs(shaped) > knee) {
    const double progress = (std::abs(shaped) - knee) / (1. - knee);
    const double available = (limit - knee) / (1. - knee);
    shaped = std::copysign(knee + (1. - knee) * progress /
                           (1. + (1. / available - 1.) * progress), shaped);
  }
  const double driverPriority = std::clamp((.88 - stick) / .62, 0., 1.);
  double correction = response.correction * driverPriority / fullSteering;
  if (!driverCorrecting) {
    const double retainIntent = std::clamp((stick - .22) / .23, 0., 1.);
    const double maximum = std::abs(correction) * (1. - retainIntent) + std::abs(shaped) * retainIntent;
    correction = std::copysign(std::min(std::abs(correction), maximum), correction);
  }
  shaped = std::clamp(shaped + correction, -limit, limit);
  const auto outputSpan = shaped < 0 ? std::int64_t(centre) - minimum : std::int64_t(maximum) - centre;
  return std::int32_t(centre + std::llround(shaped * outputSpan));
}

// The render thread publishes the entire response and device together. A native
// device needs an exact VID/PID match, and zero is the disabled request.
inline std::uint64_t steeringRequest(SteeringResponse response, bool xinput, std::uint32_t vidpid) {
  if (!response.active() || (!xinput && !vidpid))
    return 0;
  return (std::uint64_t(response.softening) << 55) | (std::uint64_t(vidpid) << 23) |
         (std::uint64_t(response.correction + 512) << 13) |
         (std::uint64_t(response.limit) << 3) |
         (xinput ? 4u : 0u) | unsigned(response.curve);
}
inline SteeringResponse requestedSteering(std::uint64_t request, bool xinput,
                                          std::uint32_t vidpid) {
  if (!request || bool(request & 4) != xinput || (!xinput && std::uint32_t(request >> 23) != vidpid))
    return {};
  return {SteeringCurve(request & 3), unsigned((request >> 3) & 1023),
          int((request >> 13) & 1023) - 512, unsigned(request >> 55)};
}

struct RumbleControllerChoice {
  std::int64_t previous;
  std::int64_t active = -1;
  unsigned count = 0;
  bool previousPresent = false;

  void observe(std::int64_t id, bool driving) {
    previousPresent |= id == previous;
    if (!driving)
      return;
    active = id;
    ++count;
  }

  std::int64_t select(std::int64_t fallback) const {
    return count == 1 ? active : previousPresent ? previous : fallback;
  }
};

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
