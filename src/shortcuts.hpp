#pragma once
#include "language.hpp"
#include "lfs_profile.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace pad {
enum class ShortcutScreen { Main, SinglePlayer, Garage, Options, Race, None };
enum class Favorite { None, Axes, Skins, Clutch };
inline constexpr LocalizedText axesLabel{"Eksenler / FF", "Axes / FF"};
inline constexpr LocalizedText skinListLabel{"Kaplama listesi", "Skin list"};
inline constexpr LocalizedText clutchLabel{"Otomatik debriyaj", "Auto clutch"};
struct ShortcutTarget {
  const wchar_t *id = nullptr;
  LocalizedText label;
  ProfileRva callback = nullptr;
  unsigned type = 0, value = 0;
  Favorite favorite = Favorite::None;
};
using ShortcutDirections = std::array<int, 4>;
inline constexpr ShortcutDirections defaultShortcutDirections{1, 2, 3, 4};
struct ShortcutMenu {
  const wchar_t *key = nullptr;
  LocalizedText label;
  int count = 0;
  // Index zero is disabled. The next four entries are the default directions.
  std::array<ShortcutTarget, 7> targets{};
  ProfileRva ownerRva = nullptr;
};
inline constexpr ShortcutTarget shortcutOff{L"off", {"Kapalı", "Off"}};
inline constexpr std::array<ShortcutMenu, std::size_t(ShortcutScreen::None)>
    shortcutMenus{{
        {L"MainMenu", {"Ana menü", "Main menu"}, 7,
         {shortcutOff, {L"solo", {"Tek oyunculu", "Single player"}, &LfsBuildProfile::mainMenuCallback, 9, 1},
          {L"options", {"Seçenekler", "Options"}, &LfsBuildProfile::mainMenuCallback, 9, 3},
          {L"replays", {"Tekrarlar", "Replays"}, &LfsBuildProfile::mainMenuCallback, 9, 4},
          {L"online", {"Çok oyunculu", "Multiplayer"}, &LfsBuildProfile::mainMenuCallback, 9, 2},
          {L"training", {"Eğitim", "Training"}, &LfsBuildProfile::mainMenuCallback, 9, 5},
          {L"axes-ff", axesLabel, nullptr, 0, 0, Favorite::Axes}}},
        {L"SinglePlayer", {"Tek oyunculu hazırlık", "Single-player setup"}, 5,
         {shortcutOff, {L"garage", {"Garaj", "Garage"}, &LfsBuildProfile::singlePlayerNavCallback, 4, 2},
          {L"go", {"Yürü", "Go"}, &LfsBuildProfile::singlePlayerGoCallback, 3, 0},
          {L"track", {"Pist", "Track"}, &LfsBuildProfile::singlePlayerNavCallback, 4, 3},
          {L"driver", {"Sürücü", "Driver"}, &LfsBuildProfile::singlePlayerNavCallback, 4, 4}}},
        {L"Garage", {"Garaj", "Garage"}, 7,
         {shortcutOff, {L"setups", {"Ayar listesi", "Setups"}},
          {L"ok", {"Tamam", "OK"}, &LfsBuildProfile::garageOkCallback, 3, 1},
          {L"colours", {"Renkler", "Colours"}, &LfsBuildProfile::garageTabCallback, 0, 1},
          {L"viewer", {"Görüntüleyici", "Viewer"}, &LfsBuildProfile::garageViewerCallback, 0, 1},
          {L"skins", {"Kaplama seç", "Select skin"}, &LfsBuildProfile::garageSkinsOpenCallback, 0, 0},
          {L"skin-list", skinListLabel, nullptr, 0, 0, Favorite::Skins}}},
        {L"Options", {"Seçenekler", "Options"}, 5,
         {shortcutOff, {L"controls", {"Kontroller", "Controls"}, &LfsBuildProfile::optionsNativeSelect, 4, 7},
          {L"ok", {"Tamam", "OK"}, &LfsBuildProfile::optionsNativeSelect, 3, 0},
          {L"display", {"Ekran", "Display"}, &LfsBuildProfile::optionsNativeSelect, 4, 2},
          {L"graphics", {"Grafik", "Graphics"}, &LfsBuildProfile::optionsNativeSelect, 4, 3}},
         &LfsBuildProfile::optionsWindow},
        {L"RaceMenu", {"Durdurma menüsü", "Pause menu"}, 6,
         {shortcutOff, {L"resume", {"Devam et", "Resume"}, &LfsBuildProfile::raceMenuCallback, 5, 0},
          {L"pit", {"Pit", "Pit"}, &LfsBuildProfile::raceMenuCallback, 5, 2},
          {L"options", {"Seçenekler", "Options"}, &LfsBuildProfile::raceMenuCallback, 5, 1},
          {L"replay", {"Tekrarlamayı göster", "Show replay"}, &LfsBuildProfile::raceMenuCallback, 5, 3},
          {L"axes-ff", axesLabel, nullptr, 0, 0, Favorite::Axes}}},
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
  Favorite favorite = Favorite::None;
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
      result[i] = {findItem(target), &target.label, target.favorite};
    }
  }
  return result;
}

struct ShortcutGesture {
  bool waiting = false;
  bool tap = false;
  bool dismissed = false;

  bool update(bool allowed, bool sequential, bool down, bool pressed,
              bool otherInput, bool cancel) {
    if (!allowed || cancel || (waiting && pressed)) {
      waiting = tap = false;
      dismissed = down;
      return false;
    }
    if (down) {
      if (pressed && sequential && !otherInput)
        tap = true;
      if (otherInput)
        tap = false;
      return !dismissed;
    }
    dismissed = false;
    if (tap && !otherInput)
      waiting = true;
    tap = false;
    return waiting;
  }
};

enum class FavoritePage { Other, Transition, Entry, Options, Controls, Axes, Garage, Colours, Skins, Driver };

inline FavoritePage nextFavoritePage(Favorite favorite, FavoritePage page) {
  if (favorite == Favorite::Axes) {
    if (page == FavoritePage::Entry) return FavoritePage::Options;
    if (page == FavoritePage::Options) return FavoritePage::Controls;
    if (page == FavoritePage::Controls) return FavoritePage::Axes;
  } else if (favorite == Favorite::Skins) {
    if (page == FavoritePage::Garage) return FavoritePage::Colours;
    if (page == FavoritePage::Colours) return FavoritePage::Skins;
  } else if (favorite == Favorite::Clutch) {
    if (page == FavoritePage::Entry) return FavoritePage::Options;
    if (page == FavoritePage::Options) return FavoritePage::Driver;
  }
  return FavoritePage::Other;
}

struct FavoriteRoute {
  Favorite favorite = Favorite::None;
  FavoritePage from = FavoritePage::Other, expected = FavoritePage::Other;
  double deadline = 0;

  void begin(Favorite target, FavoritePage page, double now) {
    favorite = target;
    from = expected = page;
    deadline = now + 1.;
  }
  void cancel() { *this = {}; }

  // Returns the page whose native button should be activated this frame.
  FavoritePage update(FavoritePage page, bool clickReady, double now, bool &failed) {
    failed = false;
    if (favorite == Favorite::None)
      return FavoritePage::Other;
    const bool reached = page == expected ||
        (favorite == Favorite::Axes && expected == FavoritePage::Options &&
         (page == FavoritePage::Controls || page == FavoritePage::Axes)) ||
        (favorite == Favorite::Axes && expected == FavoritePage::Controls &&
         page == FavoritePage::Axes) ||
        (favorite == Favorite::Clutch && expected == FavoritePage::Options &&
         page == FavoritePage::Driver);
    if (reached && ((favorite == Favorite::Axes && page == FavoritePage::Axes) ||
                    (favorite == Favorite::Skins && page == FavoritePage::Skins) ||
                    (favorite == Favorite::Clutch && page == FavoritePage::Driver))) {
      cancel();
    } else if (now >= deadline ||
               (!reached && page != from && page != FavoritePage::Transition)) {
      cancel();
      failed = true;
    } else if (reached && clickReady) {
      const auto destination = nextFavoritePage(favorite, page);
      if (destination != FavoritePage::Other) {
        from = page;
        expected = destination;
        deadline = now + 1.;
        return destination;
      }
    }
    return FavoritePage::Other;
  }
};
} // namespace pad
