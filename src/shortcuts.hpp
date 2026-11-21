#pragma once
#include "language.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace pad {
enum class ShortcutScreen { Main, SinglePlayer, Garage, Options, Race, None };
struct ShortcutTarget {
  const wchar_t *id = nullptr;
  LocalizedText label;
  std::uintptr_t callback = 0;
  unsigned type = 0, value = 0;
};
using ShortcutDirections = std::array<int, 4>;
inline constexpr ShortcutDirections defaultShortcutDirections{1, 2, 3, 4};
struct ShortcutMenu {
  const wchar_t *key = nullptr;
  LocalizedText label;
  int count = 0;
  // Index zero is disabled. The next four entries are the default directions.
  std::array<ShortcutTarget, 6> targets{};
  std::uintptr_t ownerRva = 0;
};
inline constexpr ShortcutTarget shortcutOff{L"off", {"Kapalı", "Off"}};
inline constexpr std::array<ShortcutMenu, std::size_t(ShortcutScreen::None)>
    shortcutMenus{{
        {L"MainMenu", {"Ana menü", "Main menu"}, 6,
         {shortcutOff, {L"solo", {"Tek oyunculu", "Single player"}, 0x84750, 9, 1},
          {L"options", {"Seçenekler", "Options"}, 0x84750, 9, 3},
          {L"replays", {"Tekrarlar", "Replays"}, 0x84750, 9, 4},
          {L"online", {"Çok oyunculu", "Multiplayer"}, 0x84750, 9, 2},
          {L"training", {"Eğitim", "Training"}, 0x84750, 9, 5}}},
        {L"SinglePlayer", {"Tek oyunculu hazırlık", "Single-player setup"}, 5,
         {shortcutOff, {L"garage", {"Garaj", "Garage"}, 0xac200, 4, 2},
          {L"go", {"Yürü", "Go"}, 0xac0c0, 3, 0}, {L"track", {"Pist", "Track"}, 0xac200, 4, 3},
          {L"driver", {"Sürücü", "Driver"}, 0xac200, 4, 4}}},
        {L"Garage", {"Garaj", "Garage"}, 6,
         {shortcutOff, {L"setups", {"Ayar listesi", "Setups"}},
          {L"ok", {"Tamam", "OK"}, 0xb3e00, 3, 1}, {L"colours", {"Renkler", "Colours"}, 0x141220, 0, 1},
          {L"viewer", {"Görüntüleyici", "Viewer"}, 0xb1780, 0, 1},
          {L"skins", {"Kaplama seç", "Select skin"}, 0xb5740, 0, 0}}},
        {L"Options", {"Seçenekler", "Options"}, 5,
         {shortcutOff, {L"controls", {"Kontroller", "Controls"}, 0x111760, 4, 7},
          {L"ok", {"Tamam", "OK"}, 0x111760, 3, 0}, {L"display", {"Ekran", "Display"}, 0x111760, 4, 2},
          {L"graphics", {"Grafik", "Graphics"}, 0x111760, 4, 3}}, 0x49129c},
        {L"RaceMenu", {"Durdurma menüsü", "Pause menu"}, 5,
         {shortcutOff, {L"resume", {"Devam et", "Resume"}, 0x8e310, 5, 0},
          {L"pit", {"Pit", "Pit"}, 0x8e310, 5, 2}, {L"options", {"Seçenekler", "Options"}, 0x8e310, 5, 1},
          {L"replay", {"Tekrarlamayı göster", "Show replay"}, 0x8e310, 5, 3}}},
    }};
using ShortcutMappings = std::array<ShortcutDirections, shortcutMenus.size()>;

inline ShortcutDirections parseShortcutDirections(const ShortcutMenu &menu,
                                                  std::wstring_view value) {
  ShortcutDirections result{};
  unsigned assigned = 0;
  for (std::size_t direction = 0; direction < result.size(); ++direction) {
    const auto comma = value.find(L',');
    if ((comma == value.npos) != (direction + 1 == result.size()))
      return defaultShortcutDirections;
    const auto token = value.substr(0, comma);
    int target = 0;
    while (target < menu.count && token != menu.targets[target].id)
      ++target;
    if (target == menu.count || (target && (assigned & (1u << target))))
      return defaultShortcutDirections;
    result[direction] = target;
    if (target)
      assigned |= 1u << target;
    if (comma != value.npos)
      value.remove_prefix(comma + 1);
  }
  return result;
}
inline std::wstring formatShortcutDirections(const ShortcutMenu &menu,
                                             const ShortcutDirections &directions) {
  std::wstring result;
  for (int target : directions) {
    if (!result.empty())
      result += L',';
    result += menu.targets[target].id;
  }
  return result;
}
inline void assignShortcut(ShortcutDirections &directions, int direction, int target) {
  if (target)
    for (int i = 0; i < int(directions.size()); ++i)
      if (i != direction && directions[i] == target) {
        directions[i] = directions[direction];
        break;
      }
  directions[direction] = target;
}
struct ResolvedShortcut {
  int item = -1;
  const LocalizedText *label = nullptr;
};
using ResolvedShortcuts = std::array<ResolvedShortcut, 4>;
template <typename FindItem>
ResolvedShortcuts resolveShortcuts(const ShortcutMenu &menu,
                                  const ShortcutDirections &directions, FindItem findItem) {
  ResolvedShortcuts result{};
  for (std::size_t i = 0; i < result.size(); ++i) {
    const int index = directions[i];
    if (index) {
      const auto &target = menu.targets[index];
      result[i] = {findItem(target), &target.label};
    }
  }
  return result;
}
} // namespace pad
