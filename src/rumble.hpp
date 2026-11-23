#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace pad {
struct RumbleSample {
  std::uint32_t time = 0;
  std::array<float, 3> acceleration{}, velocity{};
  float throttle = 0, engineSpeed = 0;
  float heading = 0, yawRate = 0, steeringAngle = 0;
  unsigned char gear = 0;
  bool hasWheels = false, hasSteeringAngle = false;
  struct Wheel {
    float deflection = 0, tanSlipAngle = 0;
    unsigned char slip = 0;
    bool touching = false;
  };
  std::array<Wheel, 4> wheels{};
};

inline float telemetryFloat(const void *data) {
  float value;
  std::memcpy(&value, data, sizeof(value));
  return value;
}

inline bool decodeCarMotion(const char *data, RumbleSample &sample) {
  sample.heading = telemetryFloat(data + 12);
  sample.yawRate = telemetryFloat(data + 8);
  if (!std::isfinite(sample.heading) || !std::isfinite(sample.yawRate))
    return false;
  for (int axis = 0; axis < 3; ++axis) {
    sample.acceleration[axis] = telemetryFloat(data + 24 + axis * 4);
    sample.velocity[axis] = telemetryFloat(data + 36 + axis * 4);
    if (!std::isfinite(sample.acceleration[axis]) || !std::isfinite(sample.velocity[axis]))
      return false;
  }
  return true;
}

// OutSimPack2 with all options enabled (cfg.txt OutSim Opts 1ff).
inline bool decodeOutSim(const char *data, int size, RumbleSample &sample) {
  if (size != 280 || std::memcmp(data, "LFST", 4) != 0)
    return false;
  RumbleSample next;
  std::memcpy(&next.time, data + 8, sizeof(next.time));
  if (!decodeCarMotion(data + 12, next))
    return false;
  next.hasWheels = true;
  next.throttle = telemetryFloat(data + 72);
  next.steeringAngle = telemetryFloat(data + 80);
  next.hasSteeringAngle = std::isfinite(next.steeringAngle);
  next.gear = static_cast<unsigned char>(data[92]);
  next.engineSpeed = telemetryFloat(data + 96);
  if (!std::isfinite(next.throttle) || !std::isfinite(next.engineSpeed))
    return false;
  for (int wheel = 0; wheel < 4; ++wheel) {
    const auto offset = 112 + wheel * 40;
    auto &state = next.wheels[wheel];
    state.deflection = telemetryFloat(data + offset);
    state.slip = static_cast<unsigned char>(data[offset + 29]);
    state.touching = data[offset + 30] != 0;
    const float angle = telemetryFloat(data + offset + 36);
    state.tanSlipAngle = std::isfinite(angle) ? angle : 0.f;
    if (!std::isfinite(state.deflection))
      state.deflection = 0.f;
  }
  sample = next;
  return true;
}

inline constexpr unsigned char rumbleCarRequest = 251;

// SMALL_AII is a read-only query about one local car. Match the response to
// the request and driver before using it as a camera-independent source.
inline bool decodeInSimRumble(const unsigned char *packet, unsigned size,
                             unsigned player, std::uint32_t tick, RumbleSample &sample) {
  if (size != 96 || packet[0] != 24 || packet[1] != 69 ||
      packet[2] != rumbleCarRequest || !player || packet[3] != player)
    return false;
  RumbleSample next;
  const auto data = reinterpret_cast<const char *>(packet);
  if (!decodeCarMotion(data + 4, next))
    return false;
  const float rpm = telemetryFloat(data + 68);
  if (!std::isfinite(rpm))
    return false;
  next.time = tick;
  next.gear = packet[65];
  next.engineSpeed = (packet[64] & 1) ? rpm * .104719755f : 0.f;
  sample = next;
  return true;
}

struct MotorLevels {
  std::uint16_t left = 0, right = 0;
};

// Refresh steady commands as well as changing ones. Keep writes bounded
// while the render loop runs faster than the controller needs.
inline bool rumbleWriteDue(bool active, MotorLevels previous, MotorLevels next,
                           std::uint64_t elapsed) {
  return !active || elapsed >= 100 ||
         (elapsed >= 20 && (next.left != previous.left || next.right != previous.right));
}

inline MotorLevels motorLevels(float left, float right, int strength) {
  const double scale = 65535.0 * std::clamp(strength, 0, 100) / 100.0;
  return {std::uint16_t(std::lround((std::isfinite(left) ? std::clamp(left, 0.f, 1.f) : 0.f) * scale)),
          std::uint16_t(std::lround((std::isfinite(right) ? std::clamp(right, 0.f, 1.f) : 0.f) * scale))};
}

inline constexpr std::uint64_t rumbleTimeout = 160;

struct DrivingTelemetry {
  RumbleSample sample;
  std::uint64_t received = 0;

  bool fresh(std::uint64_t now, std::uint64_t timeout = rumbleTimeout) const {
    return received && now >= received && now - received < timeout;
  }
};

inline DrivingTelemetry selectDrivingTelemetry(const DrivingTelemetry &outSim,
                                               const DrivingTelemetry &inSim,
                                               std::uint64_t now) {
  // Use wheel data when it is current. InSim is already sampled in the
  // background of the current driving session when the camera changes.
  return outSim.fresh(now, 65) || !inSim.fresh(now) ? outSim : inSim;
}

struct DrivingRumble {
  RumbleSample previous{};
  std::uint64_t lastPacket = 0;
  float road = 0, impact = 0, shift = 0;

  MotorLevels update(const RumbleSample &sample, std::uint64_t received,
                     std::uint64_t now, int strength) {
    if (!strength || !received || now < received || now - received >= rumbleTimeout) {
      *this = {};
      return {};
    }
    const float speed = std::hypot(sample.velocity[0], sample.velocity[1]);
    const float load = std::clamp(sample.throttle, 0.f, 1.f);
    const float rpm = std::clamp((sample.engineSpeed - 75.f) / 675.f, 0.f, 1.f);
    const float engine = rpm * (sample.hasWheels ? .13f + .23f * load : .22f);
    float slip = 0;
    if (sample.hasWheels) {
      for (const auto &wheel : sample.wheels) {
        if (wheel.touching)
          slip = std::max(slip, std::clamp((wheel.slip - 38.f) / 217.f, 0.f, 1.f));
      }
    } else if (speed > 1.5f) {
      const float sideways = std::abs(sample.velocity[0] * std::cos(sample.heading) +
                                      sample.velocity[1] * std::sin(sample.heading));
      slip = std::clamp((sideways / speed - .08f) / .55f, 0.f, .65f);
    }
    slip *= std::clamp((speed - 1.5f) / 5.f, 0.f, 1.f);
    if (received != lastPacket) {
      const auto elapsed = sample.time - previous.time;
      if (lastPacket && sample.hasWheels == previous.hasWheels &&
          elapsed >= 4 && elapsed <= 150 &&
          received - lastPacket < rumbleTimeout) {
        const float dt = elapsed * .001f;
        float suspensionSpeed = 0;
        for (std::size_t i = 0; i < sample.wheels.size(); ++i) {
          if (sample.wheels[i].touching && previous.wheels[i].touching)
            suspensionSpeed = std::max(suspensionSpeed, std::abs(
                sample.wheels[i].deflection - previous.wheels[i].deflection) / dt);
        }
        const float bump = sample.hasWheels
            ? std::clamp((suspensionSpeed - .12f) * .48f, 0.f, .60f)
            : std::clamp((std::abs(sample.acceleration[2]) - 2.5f) / 22.f, 0.f, .55f);
        road = std::max(road - 6.f * dt, bump * std::clamp((speed - 2.f) / 6.f, 0.f, 1.f));
        const float acceleration = std::hypot(sample.acceleration[0], sample.acceleration[1]);
        impact = std::max(impact - 4.5f * dt,
                          std::clamp((acceleration - 24.f) / 45.f, 0.f, 1.f));
        shift = std::max(0.f, shift - 4.f * dt);
        if (speed > 3.f && sample.gear >= 2 && sample.gear != previous.gear)
          shift = .36f;
      } else {
        road = impact = shift = 0;
      }
      previous = sample;
      lastPacket = received;
    }
    return motorLevels(engine + .35f * road + impact + shift,
                       .25f * engine + road + .75f * slip + .45f * impact + .55f * shift,
                       strength);
  }
};
} // namespace pad
