#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace pad {
enum class CheckStep : std::size_t { Up, Down, Left, Right, LT, RT, LowMotor, HighMotor, Done };
enum class CheckResult { Pending, Passed, Skipped, NotFelt, NoOutput };
enum class MotorPhase { Ready, Pulsing, Feedback };
enum class MotorReply { Accepted, Busy, Rejected };

struct ControllerCheck {
  static constexpr unsigned neutralTrigger = 30;
  static constexpr unsigned pressedTrigger = 200;
  static constexpr double pulseSeconds = .32;
  std::array<CheckResult, std::size_t(CheckStep::Done)> results{};
  CheckStep step = CheckStep::Up;
  MotorPhase motor = MotorPhase::Ready;
  bool interrupted = false;

  bool complete() const { return step == CheckStep::Done; }
  bool testingMotor() const { return step == CheckStep::LowMotor || step == CheckStep::HighMotor; }

  void interrupt() {
    interrupted = true;
    motor = MotorPhase::Ready;
    armed = accepted = false;
  }

  void update(std::uint16_t directions, unsigned lt, unsigned rt,
              bool confirm, bool skip, double now) {
    if (interrupted || complete())
      return;
    if (testingMotor()) {
      if (motor == MotorPhase::Ready) {
        if (skip)
          advance(CheckResult::Skipped);
        else if (confirm) {
          motor = MotorPhase::Pulsing;
          deadline = now + pulseSeconds;
        }
      } else if (motor == MotorPhase::Pulsing && now >= deadline) {
        if (accepted)
          motor = MotorPhase::Feedback;
        else
          advance(CheckResult::NoOutput);
      } else if (motor == MotorPhase::Feedback && confirm != skip) {
        advance(confirm ? CheckResult::Passed : CheckResult::NotFelt);
      }
      return;
    }
    if (skip) {
      advance(CheckResult::Skipped);
      return;
    }
    if (!directions && lt <= neutralTrigger && rt <= neutralTrigger)
      armed = true;
    if (!armed)
      return;
    if (step <= CheckStep::Right) {
      const auto expected = 1u << std::size_t(step);
      if ((directions & ~expected) || lt > neutralTrigger || rt > neutralTrigger)
        armed = false;
      else if (directions == expected)
        advance(CheckResult::Passed);
    } else {
      const bool left = step == CheckStep::LT;
      if (directions || (left ? rt : lt) > neutralTrigger)
        armed = false;
      else if ((left ? lt : rt) >= pressedTrigger)
        advance(CheckResult::Passed);
    }
  }

  void motorReply(MotorReply reply) {
    if (motor != MotorPhase::Pulsing)
      return;
    if (reply == MotorReply::Accepted)
      accepted = true;
    else if (reply == MotorReply::Rejected)
      advance(CheckResult::NoOutput);
  }

private:
  bool armed = false, accepted = false;
  double deadline = 0;

  void advance(CheckResult result) {
    const auto index = std::size_t(step);
    results[index] = result;
    step = CheckStep(index + 1);
    motor = MotorPhase::Ready;
    armed = accepted = false;
  }
};
} // namespace pad
