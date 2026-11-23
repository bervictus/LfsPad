#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

namespace pad {
enum class Language { Turkish, English };

inline constexpr Language parseLanguage(std::wstring_view value) {
  return value == L"en" ? Language::English : Language::Turkish;
}

inline constexpr const wchar_t *languageKey(Language language) {
  return language == Language::English ? L"en" : L"tr";
}

struct LocalizedText {
  const char *turkish = nullptr;
  const char *english = nullptr;

  constexpr const char *get(Language language) const {
    return language == Language::English ? english : turkish;
  }
};

inline constexpr std::array<std::wstring_view, 4> keyboardRows(
    Language language, bool uppercase, bool symbols) {
  if (symbols)
    return {L"1234567890", LR"(!"#$%&'()*+,)", LR"(-./:;<=>?@[\)", L"]^_`{|}~€£"};
  if (language == Language::English)
    return uppercase
        ? std::array<std::wstring_view, 4>{L"1234567890", L"QWERTYUIOP", L"ASDFGHJKL", L"ZXCVBNM.,"}
        : std::array<std::wstring_view, 4>{L"1234567890", L"qwertyuiop", L"asdfghjkl", L"zxcvbnm.,"};
  return uppercase
      ? std::array<std::wstring_view, 4>{L"1234567890", L"QWERTYUIOPĞÜ", L"ASDFGHJKLŞİ", L"ZXCVBNMÖÇ.,"}
      : std::array<std::wstring_view, 4>{L"1234567890", L"qwertyuıopğü", L"asdfghjklşi", L"zxcvbnmöç.,"};
}

struct KeyboardKey {
  float x, width;
  float center() const { return x + width * .5f; }
};

inline KeyboardKey keyboardKey(int row, int column, int count) {
  // Units are standard key widths. The same geometry drives focus and drawing.
  if (row < 4)
    return {(12.f - count) * .5f + column, 1.f};
  constexpr KeyboardKey footer[]{{3.f, 4.25f}, {7.25f, 1.45f}, {1.5f, 1.5f},
                                 {0.f, 1.5f}, {8.7f, 1.65f}, {10.35f, 1.65f}};
  return footer[column];
}

inline constexpr int keyboardFooterOrder[]{3, 2, 0, 1, 4, 5};

inline int nearestKeyboardKey(int row, int count, float x) {
  int best = 0;
  for (int i = 1; i < count; ++i) {
    const auto distance = [row, count, x](int column) {
      const auto key = keyboardKey(row, column, count);
      return std::abs(x - std::clamp(x, key.x, key.x + key.width));
    };
    const float candidate = distance(i), current = distance(best);
    if (candidate < current || (candidate == current &&
        std::abs(x - keyboardKey(row, i, count).center()) <
        std::abs(x - keyboardKey(row, best, count).center())))
      best = i;
  }
  return best;
}
} // namespace pad
