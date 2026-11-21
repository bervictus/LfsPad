#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace pad {
// Assigning an occupied button swaps the two actions, preserving access to all
// actions while the user edits a mapping.
inline void rebind(std::array<int, 9> &bindings, int action, int button) {
  for (int i = 0; i < int(bindings.size()); ++i)
    if (i != action && bindings[i] == button)
      bindings[i] = bindings[action];
  bindings[action] = button;
}
struct BindingCapture {
  enum class Phase { Idle, Release, Ready };
  Phase phase = Phase::Idle;

  void begin() { phase = Phase::Release; }
  void cancel() { phase = Phase::Idle; }
  bool active() const { return phase != Phase::Idle; }

  template <std::size_t N>
  int update(std::uint16_t buttons,
             const std::array<std::uint16_t, N> &assignable) {
    if (!active())
      return -1;
    int button = -1;
    for (int i = 0; i < int(assignable.size()); ++i) {
      if (!(buttons & assignable[i]))
        continue;
      if (button >= 0) {
        phase = Phase::Release;
        return -1;
      }
      button = i;
    }
    if (button < 0) {
      phase = Phase::Ready;
      return -1;
    }
    if (phase != Phase::Ready)
      return -1;
    cancel();
    return button;
  }
};
struct NativeButtonAssignmentGate {
  bool armed = false;

  bool update(bool assigning, bool reset, std::uint16_t buttons) {
    if (!assigning || reset)
      armed = false;
    if (assigning && !buttons)
      armed = true;
    return armed;
  }
};

struct Rect {
  float l, t, r, b;
  float x() const { return (l + r) * .5f; }
  float y() const { return (t + b) * .5f; }
};
inline float colourSliderPosition(unsigned component, unsigned maximum,
                                  int direction) {
  if (!maximum)
    return .5f;
  const float value = float(component) / float(maximum);
  return std::clamp(value + direction * .025f, .001f, .999f);
}
inline constexpr unsigned rightStickMask = 1u << 16;
inline constexpr unsigned triggerAxisMask = 1u << 17;
inline constexpr unsigned leftStickMask = 1u << 18;
struct GarageViewInput {
  bool armed = false;
  unsigned heldAxes = 0;

  unsigned update(bool viewer, bool reset, bool stickMoved, bool triggerHeld) {
    if (reset || !viewer)
      armed = false;
    const unsigned moved = (stickMoved ? rightStickMask : 0) |
                           (triggerHeld ? triggerAxisMask : 0);
    if (viewer && !moved && !reset)
      armed = true;
    heldAxes = viewer ? moved : heldAxes & moved;
    return viewer ? rightStickMask | triggerAxisMask : heldAxes;
  }
};
struct Item {
  Rect rect{};
  int id = 0;
  std::uint32_t signature = 0;
  unsigned nativeType = 0;
  bool tab = false, active = false;
};
enum class Direction { Up, Down, Left, Right };
inline int menuShortcutDirection(unsigned heldDirections, unsigned pressedDirections) {
  constexpr unsigned directions = 0xf;
  heldDirections &= directions;
  if (!heldDirections || (heldDirections & (heldDirections - 1)) ||
      !(heldDirections & pressedDirections))
    return -1;
  for (int i = 0; i < 4; ++i)
    if (heldDirections == (1u << i))
      return i;
  return -1;
}
struct ShortcutInputDrain {
  bool stickHeld = false;
  std::uint16_t buttonsHeld = 0;

  void update(bool editor, bool panel, bool stickMoved, std::uint16_t editorButtons) {
    stickHeld = stickMoved && (stickHeld || editor || panel);
    buttonsHeld = editor ? editorButtons : buttonsHeld & editorButtons;
  }
};
inline std::int32_t axisCenter(std::int32_t minimum, std::int32_t maximum) {
  return std::int32_t(minimum + (std::int64_t(maximum) - minimum + 1) / 2);
}
inline std::int32_t pedalAxis(std::uint8_t trigger, std::int32_t minimum,
                            std::int32_t maximum, bool suppressed) {
  const auto span = std::int64_t(maximum) - minimum;
  return std::int32_t(minimum + (suppressed ? 0 : (span * trigger + 127) / 255));
}
inline float axisFraction(std::int32_t value, std::int32_t minimum,
                         std::int32_t maximum) {
  const auto center = axisCenter(minimum, maximum);
  const auto delta = std::int64_t(value) - center;
  const auto span = delta < 0 ? std::int64_t(center) - minimum
                              : std::int64_t(maximum) - center;
  return span > 0 ? std::clamp(float(delta) / float(span), -1.f, 1.f) : 0.f;
}
inline int nextTab(const std::vector<Item> &items, int direction) {
  std::vector<int> tabs;
  int active = 0;
  for (int i = 0; i < int(items.size()); ++i)
    if (items[i].tab) {
      if (items[i].active)
        active = int(tabs.size());
      tabs.push_back(i);
    }
  return tabs.size() < 2
             ? -1
             : tabs[(active + direction + tabs.size()) % tabs.size()];
}
inline int sliderArrow(const std::vector<Item> &items, int selected,
                       int direction) {
  if (selected < 0 || selected >= int(items.size()) ||
      items[selected].nativeType != 40)
    return -1;
  int id = items[selected].id + direction;
  for (int i = 0; i < int(items.size()); ++i)
    if (items[i].id == id && items[i].nativeType == (direction < 0 ? 44u : 43u))
      return i;
  return -1;
}
// Prefer aligned neighbours; never wrap into an unrelated control at an edge.
inline int neighbour(const std::vector<Item> &items, int selected,
                     Direction dir) {
  if (items.empty())
    return -1;
  if (selected < 0 || selected >= int(items.size()))
    return 0;
  auto a = items[selected].rect;
  float best = std::numeric_limits<float>::max();
  int result = selected;
  for (int i = 0; i < int(items.size()); ++i) {
    if (i == selected)
      continue;
    auto b = items[i].rect;
    float dx = b.x() - a.x(), dy = b.y() - a.y();
    bool vertical = dir == Direction::Up || dir == Direction::Down;
    float forward = vertical ? dy : dx,
          side = vertical ? std::abs(dx) : std::abs(dy);
    if (dir == Direction::Up || dir == Direction::Left)
      forward = -forward;
    if (forward < .002f)
      continue;
    bool overlap =
        vertical ? (b.r > a.l && b.l < a.r) : (b.b > a.t && b.t < a.b);
    float score = forward + side * 2.5f + (overlap ? 0.f : 2.f);
    if (score < best) {
      best = score;
      result = i;
    }
  }
  return result;
}
struct Repeat {
  bool was = false;
  double next = 0, start = 0;
  bool update(bool down, double now, bool accelerate = false) {
    if (!down) {
      was = false;
      return false;
    }
    if (!was) {
      was = true;
      start = now;
      next = now + .35;
      return true;
    }
    if (now >= next) {
      next = now + (accelerate && now - start >= .9 ? .045 : .095);
      return true;
    }
    return false;
  }
};
struct HoldConfirm {
  static constexpr double duration = .70;
  double began = -1;

  void cancel() { began = -1; }
  float progress(double now) const {
    return began < 0 ? 0.f
                     : float(std::clamp((now - began) / duration, 0., 1.));
  }
  bool update(bool pressed, bool held, bool eligible, double now) {
    if (!eligible || !held) {
      cancel();
      return false;
    }
    if (pressed)
      began = now;
    if (progress(now) < 1.f)
      return false;
    cancel();
    return true;
  }
};
inline int pageOffset(int first, int visible, int total, int direction) {
  return std::clamp(first + visible * direction, 0, std::max(0, total - visible));
}
inline std::uint32_t seekTime(std::uint32_t position, std::uint32_t duration,
                              int direction) {
  return std::uint32_t(std::clamp(std::int64_t(position) + direction * 5000,
                                  std::int64_t(0), std::int64_t(duration)));
}
enum class QuickAction { None, Camera, Pit, Mirror };
inline QuickAction quickDirection(float x, float y) {
  if (std::max(std::abs(x), std::abs(y)) < .55f)
    return QuickAction::None;
  if (std::abs(x) > std::abs(y))
    return x < 0 ? QuickAction::Mirror : QuickAction::Pit;
  return y > 0 ? QuickAction::Camera : QuickAction::None;
}
inline int triggerDirection(unsigned left, unsigned right) {
  return left >= 50 && right <= 30 ? -1 : right >= 50 && left <= 30 ? 1 : 0;
}
enum class RaceSession { Outside, Unknown, Offline, Multiplayer };
enum class RecoveryPhase { Idle, Waiting, Warning, Pausing, Paused, Resuming };
enum class RecoveryAction { None, Pause, Resume };

inline RaceSession raceSession(bool known, std::uint16_t flags,
                               bool replay, bool noReplay) {
  if (!known)
    return RaceSession::Unknown;
  if (!(flags & 1) || (flags & (2 | 256)) || replay)
    return RaceSession::Outside;
  if (flags & 512)
    return RaceSession::Multiplayer;
  return noReplay ? RaceSession::Offline : RaceSession::Unknown;
}

struct PadRecovery {
  RecoveryPhase phase = RecoveryPhase::Idle;
  double deadline = 0;

  bool blocksPad() const {
    return phase == RecoveryPhase::Pausing || phase == RecoveryPhase::Paused ||
           phase == RecoveryPhase::Resuming;
  }

  RecoveryAction update(RaceSession session, bool lost, bool connected,
                        bool paused, bool canToggle, bool confirm, double now) {
    if (session == RaceSession::Outside) {
      phase = RecoveryPhase::Idle;
      return RecoveryAction::None;
    }
    if (session == RaceSession::Multiplayer) {
      phase = !connected && (lost || phase != RecoveryPhase::Idle)
                  ? RecoveryPhase::Warning : RecoveryPhase::Idle;
      return RecoveryAction::None;
    }
    if (phase == RecoveryPhase::Idle && lost)
      phase = RecoveryPhase::Waiting;

    const auto cleared = connected ? RecoveryPhase::Idle : RecoveryPhase::Warning;
    const bool offline = session == RaceSession::Offline;
    switch (phase) {
    case RecoveryPhase::Waiting:
      if (offline && paused)
        phase = cleared;
      else if (offline && canToggle) {
        phase = RecoveryPhase::Pausing;
        deadline = now + 1.5;
        return RecoveryAction::Pause;
      } else if (connected && !lost)
        phase = RecoveryPhase::Idle;
      break;
    case RecoveryPhase::Warning:
      phase = cleared;
      break;
    case RecoveryPhase::Pausing:
      if (offline && paused)
        phase = RecoveryPhase::Paused;
      else if (now >= deadline)
        phase = cleared;
      break;
    case RecoveryPhase::Paused:
      if (offline && !paused)
        phase = cleared;
      else if (offline && canToggle && connected && confirm && !lost) {
        phase = RecoveryPhase::Resuming;
        deadline = now + 1.5;
        return RecoveryAction::Resume;
      }
      break;
    case RecoveryPhase::Resuming:
      if (offline && !paused)
        phase = connected ? RecoveryPhase::Idle : RecoveryPhase::Waiting;
      else if (offline && now >= deadline)
        phase = RecoveryPhase::Paused;
      break;
    case RecoveryPhase::Idle:
      break;
    }
    return RecoveryAction::None;
  }
};
inline float analog(short v, float deadzone) {
  float f = std::clamp(float(v) / 32767.f, -1.f, 1.f), m = std::abs(f);
  return m <= deadzone ? 0.f
                       : std::copysign((m - deadzone) / (1.f - deadzone), f);
}
// A transition, focus loss or reconnection requires all held buttons to be
// released.
struct ButtonGate {
  std::uint16_t previous = 0, blocked = 0xffff;
  std::uint16_t update(std::uint16_t buttons, bool reset) {
    if (reset) {
      blocked = 0xffff;
      previous = buttons;
      return 0;
    }
    blocked &= buttons;
    auto pressed = std::uint16_t(buttons & ~previous & ~blocked);
    previous = buttons;
    return pressed;
  }
  std::uint16_t held(std::uint16_t b) const { return b & ~blocked; }
};
// Both triggers must start released in an eligible menu. Once the chord is
// used, keep its axes suppressed until both triggers are released again.
struct SettingsTriggerChord {
  static constexpr unsigned neutral = 30, activation = 180;
  static constexpr double duration = .35;
  bool armed = false, draining = false;
  double began = -1;

  bool update(unsigned left, unsigned right, double now, bool eligible) {
    const bool released = left <= neutral && right <= neutral;
    if (released)
      draining = false;
    if (!eligible) {
      armed = false;
      began = -1;
      return false;
    }
    if (released)
      armed = true;
    if (!armed || left < activation || right < activation) {
      began = -1;
      return false;
    }
    if (began < 0)
      began = now;
    if (now - began < duration)
      return false;
    armed = false;
    draining = true;
    began = -1;
    return true;
  }
};
inline bool menuCapture(bool stateKnown, std::uint16_t flags, unsigned mode,
                        bool nativeMenu) {
  return nativeMenu ||
         (stateKnown &&
          (!(flags & 1) || (flags & (16 | 256 | 32768)) || mode != 0));
}
} // namespace pad
