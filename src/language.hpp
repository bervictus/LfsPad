#pragma once
#include <array>
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
} // namespace pad
