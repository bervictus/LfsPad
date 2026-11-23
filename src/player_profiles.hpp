#pragma once
#include "controller_axes.hpp"
#include "shortcuts.hpp"
#include <array>
#include <string>
#include <string_view>

namespace pad {
struct PlayerInputSettings {
  float deadzone = .24f, cursorSpeed = 900.f;
  std::array<int, 9> bindings{0, 1, 2, 3, 4, 5, 6, 7, 8};
  int quickButton = 1;
  bool inputMonitor = false, controllerIcons = true;
  int rumbleStrength = 50;
  SteeringCurve steeringCurve = SteeringCurve::Linear;
  SteeringHelp steeringHelp = SteeringHelp::Balanced;
  bool shortcutSequential = false;
  ShortcutMappings shortcuts = [] {
    ShortcutMappings mappings;
    mappings.fill(defaultShortcutDirections);
    return mappings;
  }();
};

struct PlayerProfile {
  PlayerInputSettings input;
  std::wstring name;
  bool used = false;
};

struct PlayerProfiles {
  static constexpr int count = 3;
  std::array<PlayerProfile, count> slots;
  int active = 0;
  bool enabled = false;

  PlayerProfile &current() { return slots[active]; }

  void select(int direction, PlayerInputSettings &input) {
    current().input = input;
    current().used = true;
    active = (active + count + direction) % count;
    if (!current().used) {
      current().input = input;
      current().used = true;
    }
    input = current().input;
    enabled = true;
  }
};

inline constexpr std::size_t profileNameLimit = 18;
inline bool profileNameCharacter(wchar_t c) {
  return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') ||
         (c >= L'0' && c <= L'9') ||
         (c && std::wstring_view(L" ÇĞİÖŞÜçğıöşü-_.").find(c) != std::wstring_view::npos);
}

inline std::wstring normalizeProfileName(std::wstring_view name) {
  const auto begin = name.find_first_not_of(L' ');
  if (begin == name.npos)
    return {};
  name = name.substr(begin, name.find_last_not_of(L' ') - begin + 1);
  if (name.size() > profileNameLimit)
    return {};
  for (auto c : name)
    if (!profileNameCharacter(c))
      return {};
  return std::wstring(name);
}

// Windows profile APIs may write an existing ANSI INI in its original code
// page. Hex keeps names portable without converting the rest of the file.
inline std::wstring encodeProfileName(std::wstring_view name) {
  constexpr wchar_t digits[] = L"0123456789ABCDEF";
  std::wstring encoded;
  for (unsigned c : name)
    for (int shift = 12; shift >= 0; shift -= 4)
      encoded += digits[(c >> shift) & 15];
  return encoded;
}

inline std::wstring decodeProfileName(std::wstring_view encoded) {
  if (encoded.size() % 4 || encoded.size() > profileNameLimit * 4)
    return {};
  constexpr std::wstring_view digits = L"0123456789ABCDEF";
  std::wstring name;
  for (std::size_t i = 0; i < encoded.size(); i += 4) {
    unsigned value = 0;
    for (unsigned j = 0; j < 4; ++j) {
      const auto digit = digits.find(encoded[i + j]);
      if (digit == digits.npos)
        return {};
      value = (value << 4) | unsigned(digit);
    }
    name += wchar_t(value);
  }
  return normalizeProfileName(name);
}
} // namespace pad
