#pragma once
#include "controller_axes.hpp"
#include "rumble.hpp"

namespace pad {
inline double steeringBlend(double value, double start, double end) {
  const double t = std::clamp((value - start) / (end - start), 0., 1.);
  return t * t * (3. - 2. * t);
}

struct SteeringHelpState {
  double limit = 1., correction = 0., softening = 0.;
  int polarity = 0;
  double turnTime = 0.;
  int turnEvidence = 0, learnedLeft = 0, learnedRight = 0;

  void suspend() {
    limit = 1.;
    correction = 0.;
    softening = 0.;
    turnTime = 0.;
    turnEvidence = 0;
  }

  void learnPolarity(double axis, double turn, double seconds) {
    if (polarity)
      return;
    if (std::abs(axis) < .10 || !turn) {
      turnTime = 0.;
      turnEvidence = 0;
      return;
    }
    // LFS reports steering angle and yaw positive to the left. Require two directions to
    // agree before adding any countersteer, even if the axis is inverted.
    const int direction = axis > 0. ? 1 : -1;
    const int candidate = turn * axis < 0. ? 1 : -1;
    const int evidence = direction == 1 ? candidate : 2 * candidate;
    if (turnEvidence != evidence) {
      turnEvidence = evidence;
      turnTime = 0.;
    }
    turnTime += seconds;
    if (turnTime >= .16) {
      (direction == 1 ? learnedRight : learnedLeft) = candidate;
      if (learnedLeft == learnedRight)
        polarity = candidate;
    }
  }

  SteeringResponse update(SteeringHelp mode, SteeringCurve curve, const RumbleSample &car,
                          bool fresh, double axis, double seconds) {
    if (mode == SteeringHelp::Off) {
      *this = {};
      return {curve};
    }
    seconds = std::clamp(seconds, 0., .05);
    const bool strong = mode == SteeringHelp::Strong;
    const double forward = car.velocity[1] * std::cos(car.heading) - car.velocity[0] * std::sin(car.heading);
    const double right = car.velocity[0] * std::cos(car.heading) + car.velocity[1] * std::sin(car.heading);
    const bool movingForward = fresh && forward > 5.;
    const double slide = movingForward ? std::atan2(right, forward) : 0.;
    if (fresh && car.hasSteeringAngle)
      learnPolarity(axis, std::abs(car.steeringAngle) >= .015 ? car.steeringAngle : 0., seconds);
    else
      learnPolarity(axis, movingForward && std::abs(slide) < .08 && std::abs(car.yawRate) >= .05
                             ? car.yawRate : 0., seconds);

    double targetLimit = 1., targetCorrection = 0., targetSoftening = 0.;
    if (movingForward) {
      const double speed = steeringBlend(forward, 6., 38.);
      const double cornerSpeed = steeringBlend(forward, 5., 14.);
      targetLimit -= speed * (strong ? .20 : .12);
      targetSoftening = steeringBlend(forward, 5., 23.) * (strong ? .34 : .24);
      double axleSlip[2]{};
      bool axleContact[2]{};
      if (car.hasWheels) {
        // OutSim wheel order is rear left/right followed by front left/right.
        for (unsigned wheel = 0; wheel < car.wheels.size(); ++wheel) {
          const auto &tyre = car.wheels[wheel];
          if (!tyre.touching)
            continue;
          const unsigned axle = wheel / 2;
          axleContact[axle] = true;
          const double turningSlip = steeringBlend(std::abs(tyre.tanSlipAngle), .09, .30);
          axleSlip[axle] = std::max(axleSlip[axle], turningSlip * steeringBlend(tyre.slip, 165., 250.));
        }
        targetLimit -= cornerSpeed * axleSlip[1] * (strong ? .22 : .18);
      }
      if (polarity && (!car.hasWheels || axleContact[0] || axleContact[1])) {
        const double angle = std::abs(slide);
        const double rearSlide = axleSlip[0] * steeringBlend(axleSlip[0] - axleSlip[1], .04, .30);
        const double yawIntoSlide = std::copysign(1., slide) * car.yawRate;
        // Keep steering toward the direction of travel while a slide remains.
        // Turn rate adds urgency as it grows and damps the correction as it recovers.
        const double predicted = angle + .10 * std::clamp(yawIntoSlide, -1.5, 1.5);
        const double threshold = .10 - .045 * rearSlide;
        const double amount = std::clamp((predicted - threshold) * (strong ? 1.95 : 1.55),
                                         0., strong ? .42 : .32);
        const double stableSlide = steeringBlend(angle, .045, .10) * (1. - steeringBlend(angle, .80, 1.25));
        const double recovery = 1. - .65 * steeringBlend(-yawIntoSlide, 0., .8);
        targetCorrection = polarity * std::copysign(amount * cornerSpeed * stableSlide * recovery, slide);
      }
    }
    if (fresh && !movingForward) {
      // Parking and reversing get full lock immediately.
      limit = 1.;
      correction = 0.;
      softening = 0.;
    } else {
      limit += std::clamp(targetLimit - limit, -2. * seconds, 1.25 * seconds);
      correction += std::clamp(targetCorrection - correction, -3. * seconds, 3. * seconds);
      softening += std::clamp(targetSoftening - softening, -1.25 * seconds, 1.25 * seconds);
    }
    return {curve, unsigned(std::lround(limit * fullSteering)),
            int(std::lround(correction * fullSteering)), unsigned(std::lround(softening * fullSteering))};
  }
};
} // namespace pad
