#include "runtime.hpp"
#include "core.hpp"
#include "controller_axes.hpp"
#include "shortcuts.hpp"
#include <cstdio>
#include <cstring>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <xinput.h>

namespace pad {
#pragma pack(push, 1)
// Observed in 0.8C38: rect, value/text/extra/style, type/codepage, colour,
// enabled owner and callback. Only read after the executable hash is verified.
struct NativeItem {
  Rect rect;
  unsigned value, text, extra, style;
  unsigned char type, pad[3], codepage, pad2[3];
  unsigned colour, owner, callback;
};
#pragma pack(pop)
static_assert(sizeof(NativeItem) == 52);
static unsigned char *imageBase = (unsigned char *)GetModuleHandleW(nullptr);
enum class ListKind { None, Replays, Servers, Mods, Setups, Colours, Skins };
struct ListItem {
  int item;
  unsigned value;
  uintptr_t text;
};
struct ListView {
  ListKind kind = ListKind::None;
  std::vector<ListItem> entries;
  int first = 0, visible = 0, total = 0, columns = 1;
  Rect scrollUp{}, scrollDown{};
};
static ListView list;
static std::array<ListView, 3> garageLists;
static ListView &garageList(ListKind kind) {
  return garageLists[size_t(kind) - size_t(ListKind::Setups)];
}
static ListKind scrollKind = ListKind::None;
static int garageViewerItem = -1;
static bool garageViewer = false, garageRoot = false, garageSkins = false,
            garageModal = false;
static Rect viewerArea{};
static bool viewerDrag = false;
static bool colourDragging = false, colourMoved = false, editingGarageColour = false;
static POINT colourDestination{};
static double colourDragTime = 0;
static unsigned garageReturnCallback = 0, garageReturnValue = 0;
static int garageReturnType = -1;
static uintptr_t garageSkinPrefix = 0, garageSkinName = 0;
static std::string lastGarageSkin;
struct ListSelection {
  unsigned value = 0, labelHash = 0;
  int first = 0;
};
static std::array<ListSelection, 4> listSelections;
static ListKind previousList = ListKind::None;
static int scrollTarget = -1, scrollSlot = 0, sentOffset = -1;
static double scrollSentAt = 0;
static bool restoreAfterScroll = false;
static HWND window = nullptr;
static WNDPROC previousWndProc = nullptr;
static std::atomic<unsigned> diagnosticKeys{0};
static std::atomic<bool> assignmentFocusLost{false};
static bool diagnosticsEnabled = false;
static HoldConfirm finishHold;
static double diagnosticFinishUntil = 0;
static void cancelFinishHold() {
  finishHold.cancel();
  diagnosticFinishUntil = 0;
}
static void releaseKeys(bool all = false);
static void releaseMouse();
static LRESULT CALLBACK windowProc(HWND h, UINT message, WPARAM w, LPARAM l) {
  int diagnosticIndex = w >= '1' && w <= '9' ? int(w - '1')
                        : w == 'M'           ? 9
                        : w == 'H'           ? 10
                        : w == 'N'           ? 11
                        : w == 'I'           ? 12
                        : w == 'U'           ? 13
                        : w == 'J'           ? 14
                        : w == 'K'           ? 15
                        : w == 'Q'           ? 16
                        : w == 'T'           ? 17
                        : w == 'G'           ? 18
                                             : -1;
  if (diagnosticsEnabled &&
      (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) &&
      diagnosticIndex >= 0 && (GetKeyState(VK_CONTROL) & 0x8000) &&
      (GetKeyState(VK_SHIFT) & 0x8000) && !(l & (1L << 30))) {
    diagnosticKeys.fetch_or(1u << diagnosticIndex);
    return 0;
  }
  // LFS asks for confirmation on WM_CLOSE/SC_CLOSE. Cancelling that dialog
  // must leave the mod running; only destruction actually ends the session.
  if (message == WM_DESTROY || message == WM_NCDESTROY) {
    stopping.store(true);
    stopMotors();
    cancelFinishHold();
    releaseKeys(true);
    releaseMouse();
    captureButtons.store(false);
    captureRaceAxes.store(false);
    diagnosticDrivingPov.store(0xffffffff);
    captureQuickInput.store(0);
    monitorInput.store(false);
    pedalSlot.store(-1);
    nativePedals.store(0);
  }
  if (message == WM_ACTIVATEAPP && !w) {
    assignmentFocusLost.store(true);
    stopMotors();
    cancelFinishHold();
    diagnosticKeys.store(0);
    releaseKeys(true);
    releaseMouse();
    captureButtons.store(false);
    captureRaceAxes.store(false);
    diagnosticDrivingPov.store(0xffffffff);
    captureQuickInput.store(0);
    monitorInput.store(false);
    pedalSlot.store(-1);
    nativePedals.store(0);
  }
  return CallWindowProcA(previousWndProc, h, message, w, l);
}
static IDXGISwapChain *activeSwap = nullptr;
static ID3D11Device *device = nullptr;
static ID3D11DeviceContext *context = nullptr;
static ID3D11RenderTargetView *target = nullptr;
static bool initialized = false, cursorMode = false, settings = false,
            keyboard = false, manualKeyboard = false, upper = false,
            symbols = false;
static bool adjustingSlider = false;
static int selected = -1, keyRow = 0, keyCol = 0, settingRow = 0;
static ControllerId activeSlot = -1;
static bool playStationCaptions = false;
static DWORD vibrationPreviewResult = ERROR_CANCELLED;
static std::vector<Item> items;
static int mainMenuInitial = -1, raceFinishItem = -1, raceResumeItem = -1;
static int optionsInitial = -1;
static bool pedalCalibration = false;
static bool nativeButtonAssignment = false;
static unsigned mainMenuSelection = 5;
static std::uint32_t screenStamp = 0;
static ButtonGate gate;
static BindingCapture bindingCapture;
static Repeat repeat[4];
static double previousTime = 0, mouseRelease = 0;
static bool mouseDown = false, cursorDragging = false, wasMenu = false,
            wasForeground = false, wasConnected = false, stickArmed = false;
static unsigned ticks = 0;
static int inputSlot = -1;
static bool missingSlot = false;
static bool shortcutEditor = false, settingsSaveFailed = false;
static int shortcutEditMenu = 0, shortcutEditRow = 0;
static ShortcutMappings shortcutEditDraft, savedShortcuts;
static ShortcutDirections directionsBeforeEdit;
struct Config {
  Language language = Language::Turkish;
  int slot = -1;
  float deadzone = .24f, cursorSpeed = 900.f;
  std::array<int, 9> bindings{0, 1, 2, 3, 4, 5, 6, 7, 8};
  bool diagnostics = false;
  int quickButton = 1;
  bool inputMonitor = false;
  bool controllerIcons = true;
  int rumbleStrength = 50;
  ShortcutMappings shortcuts;
} cfg;
static const char *text(const char *turkish, const char *english) {
  return LocalizedText{turkish, english}.get(cfg.language);
}
enum Action {
  Confirm,
  Back,
  Secondary,
  Keyboard,
  PreviousTab,
  NextTab,
  Menu,
  SettingsModifier,
  CursorMode
};
enum PadButton { PadA, PadB, PadX, PadY, PadLB, PadRB, PadStart, PadBack, PadR3,
                 PadL3, PadUp, PadDown, PadLeft, PadRight, PadLT, PadRT, PadDirections, PadLeftStick, PadRightStick };
static constexpr std::array<std::uint16_t, PadRight + 1> physical = {
    XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y,
    XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER,
    XINPUT_GAMEPAD_START, XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_RIGHT_THUMB,
    XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_DOWN,
    XINPUT_GAMEPAD_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_RIGHT};
static constexpr std::uint16_t dpad = XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN |
                                      XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT;
static const char *buttonNames[] = {"A",  "B",     "X",    "Y",  "LB",
                                    "RB", "Start", "Back", "R3", "L3"};
static const char *buttonCaption(int index) {
  if (index >= PadUp) {
    const char *directions[] = {text("D-pad ↑", "D-pad ↑"), text("D-pad ↓", "D-pad ↓"),
                                text("D-pad ←", "D-pad ←"), text("D-pad →", "D-pad →")};
    return directions[index - PadUp];
  }
  const char *playStationButtons[] = {text("Çarpı", "Cross"), text("Daire", "Circle"), text("Kare", "Square"), text("Üçgen", "Triangle"), "L1",
                                           "R1", "Options", "Select", "R3", "L3"};
  return playStationCaptions ? playStationButtons[index] : buttonNames[index];
}
static const wchar_t *bindingKeys[] = {
    L"Confirm", L"Back", L"Secondary",        L"Keyboard",  L"PreviousTab",
    L"NextTab", L"Menu", L"SettingsModifier", L"CursorMode"};
static std::uint16_t binding(Action a) { return physical[cfg.bindings[a]]; }
static const char *bindingName(Action a) {
  return buttonCaption(cfg.bindings[a]);
}
enum SettingRow {
  controllerSetting, languageSetting, controllerIconsSetting, deadzoneSetting, cursorSpeedSetting,
  firstBindingSetting, quickMenuSetting = firstBindingSetting + 9,
  monitorSetting, shortcutSetting, rumbleSetting, settingCount
};
static bool isBindingSetting() {
  return settingRow >= firstBindingSetting && settingRow < quickMenuSetting;
}
static const char *quickButtonCaption() {
  const char *quickButtons[] = {text("Kapalı", "Off"), "Back", "L3", "R3"};
  return cfg.quickButton == 1 ? buttonCaption(7) : quickButtons[cfg.quickButton];
}
static const std::uint16_t quickMasks[] = {
    0, XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB};
static std::wstring ini;
static constexpr wchar_t shortcutSection[] = L"MenuShortcuts";
static void loadConfig() {
  missingSlot = false;
  shortcutEditor = settingsSaveFailed = false;
  ini = path(L"LfsPad.ini");
  wchar_t language[16]{};
  GetPrivateProfileStringW(L"General", L"Language", L"tr", language,
                           _countof(language), ini.c_str());
  cfg.language = parseLanguage(language);
  cfg.slot = std::clamp(
      (int)GetPrivateProfileIntW(L"Input", L"Controller", 0, ini.c_str()) - 1,
      -1, nativePreference);
  cfg.deadzone = std::clamp((int)GetPrivateProfileIntW(
                                L"Input", L"DeadzonePercent", 24, ini.c_str()),
                            10, 45) /
                 100.f;
  cfg.cursorSpeed = (float)std::clamp(
      (int)GetPrivateProfileIntW(L"Input", L"CursorSpeed", 900, ini.c_str()),
      200, 2400);
  cfg.bindings = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  // Swaps also repair duplicate or malformed hand-edited mappings.
  for (int i = 0; i < 9; ++i) {
    int b = std::clamp(
        (int)GetPrivateProfileIntW(L"Input", bindingKeys[i], i, ini.c_str()), 0,
        int(physical.size()) - 1);
    rebind(cfg.bindings, i, b);
  }
  cfg.diagnostics =
      GetPrivateProfileIntW(L"General", L"Diagnostics", 0, ini.c_str()) != 0;
  diagnosticsEnabled = cfg.diagnostics;
  cfg.quickButton = std::clamp(int(GetPrivateProfileIntW(
      L"Input", L"QuickMenu", 1, ini.c_str())), 0, 3);
  cfg.controllerIcons =
      GetPrivateProfileIntW(L"Input", L"ControllerIcons", 1, ini.c_str()) != 0;
  cfg.inputMonitor =
      GetPrivateProfileIntW(L"Input", L"InputMonitor", 0, ini.c_str()) != 0;
  cfg.rumbleStrength = std::clamp(int(GetPrivateProfileIntW(
      L"Input", L"RumbleStrength", 50, ini.c_str())), 0, 100);
  for (std::size_t i = 0; i < shortcutMenus.size(); ++i) {
    wchar_t value[128]{};
    auto length = GetPrivateProfileStringW(shortcutSection, shortcutMenus[i].key,
                                           L"", value, 128, ini.c_str());
    cfg.shortcuts[i] = length < 127
                          ? parseShortcutDirections(shortcutMenus[i], value)
                          : defaultShortcutDirections;
  }
  savedShortcuts = cfg.shortcuts;
  inputSlot = cfg.slot;
}
static bool saveConfig() {
  if (cfg.shortcuts != savedShortcuts) {
    std::wstring records;
    for (std::size_t i = 0; i < shortcutMenus.size(); ++i) {
      records += shortcutMenus[i].key;
      records += L'=';
      records += formatShortcutDirections(shortcutMenus[i], cfg.shortcuts[i]);
      records += wchar_t{};
    }
    // The implicit string terminator is the second NUL after the last entry.
    if (!WritePrivateProfileSectionW(shortcutSection, records.c_str(), ini.c_str()))
      return false;
    savedShortcuts = cfg.shortcuts;
  }
  bool inputSaved = true;
  auto put = [&](const wchar_t *k, int v) {
    if (!WritePrivateProfileStringW(L"Input", k, std::to_wstring(v).c_str(), ini.c_str()))
      inputSaved = false;
  };
  put(L"Controller", cfg.slot + 1);
  put(L"DeadzonePercent", int(cfg.deadzone * 100 + .5f));
  put(L"CursorSpeed", int(cfg.cursorSpeed));
  put(L"QuickMenu", cfg.quickButton);
  put(L"InputMonitor", cfg.inputMonitor);
  put(L"ControllerIcons", cfg.controllerIcons);
  put(L"RumbleStrength", cfg.rumbleStrength);
  for (int i = 0; i < 9; ++i)
    put(bindingKeys[i], cfg.bindings[i]);
  return inputSaved && WritePrivateProfileStringW(
      L"General", L"Language", languageKey(cfg.language), ini.c_str());
}
static bool validMenuRect(const Rect &r) {
  return std::isfinite(r.l) && std::isfinite(r.t) && std::isfinite(r.r) &&
         std::isfinite(r.b) && r.r - r.l >= .001f && r.b - r.t >= .001f &&
         r.l >= -1.01f && r.r <= 1.01f && r.t >= -1.01f && r.b <= 1.01f;
}
static bool gather(uintptr_t modal, bool inGarageMode) {
  const bool garageContext = garageRoot || garageViewer || garageSkins || garageModal;
  items.clear();
  list.kind = ListKind::None;
  list.entries.clear();
  list.first = list.visible = list.total = 0;
  list.columns = 1;
  for (auto &view : garageLists) {
    view.entries.clear();
    view.kind = ListKind::None;
    view.first = view.visible = view.total = 0;
    view.scrollUp = view.scrollDown = {};
  }
  garageViewer = garageRoot = garageSkins = garageModal = false;
  garageViewerItem = -1;
  viewerArea = {-1, -1, 1, 1};
  garageSkinPrefix = garageSkinName = 0;
  mainMenuInitial = -1;
  optionsInitial = -1;
  pedalCalibration = nativeButtonAssignment = false;
  raceFinishItem = -1;
  raceResumeItem = -1;
  bool raceMenu = false;
  const int count = *reinterpret_cast<const int *>(imageBase + 0x28c994);
  if (count < 0 || count > 1024)
    return false;
  auto native = reinterpret_cast<const NativeItem *>(imageBase + 0x28ca20);
  const auto raceMenuCallback = uintptr_t(imageBase + 0x8e310);
  bool viewerLight = false, viewerLod = false;
  for (int i = 0; modal && i < count; ++i) {
    const auto &n = native[i];
    if (n.owner != modal)
      continue;
    const auto callback = n.callback - uintptr_t(imageBase);
    garageRoot |= n.type == 0 && callback == 0xb1780 && n.value == 1;
    garageSkins |= n.type == 0 && callback == 0x13dba0;
    viewerLight |= n.type == 0 && callback == 0xb2dd0;
    viewerLod |= n.type == 0 && callback == 0xb2de0;
  }
  garageViewer = viewerLight && viewerLod && !garageRoot;
  garageModal = modal && inGarageMode && garageContext &&
                !garageRoot && !garageViewer && !garageSkins;
  unsigned listOwner = 0;
  for (int i = 0; i < count; ++i) {
    auto &n = native[i];
    auto r = n.rect;
    if (!n.owner || !n.callback || !validMenuRect(r))
      continue;
    if ((garageRoot || garageViewer || garageSkins || garageModal) && n.owner != modal)
      continue;
    auto sig = n.callback ^ (n.owner * 16777619u) ^ (unsigned(n.type) << 24) ^
               unsigned(i * 31);
    if (n.owner == modal && n.callback == uintptr_t(imageBase + 0x84750) &&
        n.type == 9 && (n.value == mainMenuSelection ||
                       (n.value == 5 && mainMenuInitial < 0)))
      mainMenuInitial = int(items.size());
    // LFS leaves ISS_GAME set and ISS_DIALOG clear in the race menu. Match
    // visible controls owned by this modal, not merely any driving HUD item.
    if (n.owner == modal && n.callback == raceMenuCallback) {
      raceMenu = true;
      if (n.type == 3 && n.value == 5)
        raceFinishItem = int(items.size());
      if (n.type == 5 && n.value == 0)
        raceResumeItem = int(items.size());
    }
    ListKind kind = ListKind::None;
    const auto callback = n.callback - uintptr_t(imageBase);
    const bool active = n.colour == uintptr_t(imageBase) + 0x43b330;
    const bool optionsTab = n.owner == uintptr_t(imageBase) + 0x49129c &&
                            callback == 0x111760 && n.type == 4 &&
                            n.value >= 1 && n.value <= 9;
    if (optionsTab && (active || optionsInitial < 0))
      optionsInitial = int(items.size());
    // LT+RT is a real pedal input while the native Axes / FF page is active.
    if (active && n.owner == modal && callback == 0x10aba0 && n.type == 0 &&
        n.value == 5)
      pedalCalibration = true;
    if (active && n.owner == modal && n.type == 0 && callback == 0x10abc0)
      nativeButtonAssignment = true;
    if (garageRoot && n.type == 0 && callback == 0xb1780 && n.value == 1)
      garageViewerItem = int(items.size());
    if (garageRoot && n.type == 0 && callback == 0x141340)
      garageSkinPrefix = n.text;
    if (garageRoot && n.type == 0 && callback == 0xb5740 && n.value == 0)
      garageSkinName = n.text;
    if (garageViewer)
      viewerArea.b = std::min(viewerArea.b, r.t);
    ListKind garageKind = ListKind::None;
    if (garageRoot && n.type == 0) {
      if (callback == 0xb4420 || callback == 0xb4440)
        garageKind = ListKind::Setups;
      else if (callback == 0xb4650)
        garageKind = ListKind::Colours;
    } else if (garageSkins && n.type == 0 && callback == 0x13dbe0) {
      garageKind = ListKind::Skins;
    }
    if (garageKind != ListKind::None) {
      auto &view = garageList(garageKind);
      view.kind = garageKind;
      view.entries.push_back({int(items.size()), n.value, n.text});
    }
    // A preset is drawn as a marker and a swatch with the same action.
    bool selectedPreset = false;
    if (garageRoot && n.type == 2 && callback == 0xb4640) {
      if (n.style != 1 || i == 0)
        continue;
      const auto &marker = native[i - 1];
      if (marker.callback != n.callback || marker.owner != n.owner ||
          marker.value != n.value || marker.style != 0)
        continue;
      r.l = marker.rect.l >= -1.01f && marker.rect.l < r.l ? marker.rect.l : r.l;
      selectedPreset = marker.colour == uintptr_t(imageBase + 0x43b330);
    }
    if (callback == 0x1372d0 && n.type == 0 && r.r - r.l > .2f)
      kind = ListKind::Replays; // Use the name column, which shares a callback.
    if (callback == 0xa6d60 && n.type == 0)
      kind = ListKind::Servers;
    if (callback == 0xf3f60 && n.type == 0)
      kind = ListKind::Mods;
    if (kind != ListKind::None &&
        (listOwner == 0 || (listOwner == n.owner && list.kind == kind))) {
      list.kind = kind;
      listOwner = n.owner;
      list.entries.push_back({int(items.size()), n.value, n.text});
    }
    items.push_back(
        {r, i, sig, n.type,
         optionsTab || (callback == 0x141220 && n.type == 0),
         selectedPreset || active});
  }
  for (auto &view : garageLists) {
    if (view.entries.empty())
      continue;
    const Rect &first = items[view.entries.front().item].rect;
    const Rect &last = items[view.entries.back().item].rect;
    for (int i = 1; i + 1 < count; ++i) {
      const auto &bar = native[i], &up = native[i - 1], &down = native[i + 1];
      if (bar.type != 42 || bar.owner != modal || bar.callback ||
          bar.text >= bar.extra || bar.extra > bar.style || bar.style > 100000 ||
          bar.extra - bar.text != view.entries.size() ||
          up.type != 49 || down.type != 50 || up.owner != modal ||
          down.owner != modal || up.value != bar.value || down.value != bar.value ||
          !validMenuRect(up.rect) || !validMenuRect(down.rect) ||
          up.rect.r <= last.r || up.rect.l - last.r > (view.kind == ListKind::Skins ? .3f : .007f) ||
          std::abs(up.rect.t - first.t) > .003f ||
          std::abs(down.rect.b - last.b) > .003f)
        continue;
      view.first = int(bar.text);
      view.visible = int(bar.extra - bar.text);
      view.total = int(bar.style);
      view.scrollUp = up.rect;
      view.scrollDown = down.rect;
      break;
    }
  }
  if (listOwner) {
    for (int i = 0; i < count; ++i) {
      const auto &n = native[i];
      if (n.type == 42 && n.owner == listOwner && !n.callback &&
          n.text < n.extra && n.extra <= n.style && n.style <= 100000 &&
          n.rect.r > n.rect.l &&
          n.rect.b > n.rect.t && n.rect.l >= -1.01f && n.rect.r <= 1.01f &&
          n.rect.t >= -1.01f && n.rect.b <= 1.01f) {
        list.first = int(n.text);
        list.visible = int(n.extra - n.text);
        list.total = int(n.style);
        break;
      }
    }
    if (list.kind == ListKind::Mods) {
      const auto top = items[list.entries.front().item].rect.y();
      list.columns = int(std::count_if(list.entries.begin(), list.entries.end(),
                                      [&](const ListItem &entry) {
        return std::abs(items[entry.item].rect.y() - top) < .002f;
      }));
    }
  }
  return raceMenu;
}
static void releaseMouse() {
  if (mouseDown) {
    INPUT i{};
    i.type = INPUT_MOUSE;
    i.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &i, sizeof(i));
    mouseDown = false;
  }
  cursorDragging = false;
  viewerDrag = false;
  colourDragging = false;
}
static void clickAt(float x, float y, bool right = false, bool hold = false) {
  if (GetForegroundWindow() != window)
    return;
  RECT r;
  GetClientRect(window, &r);
  POINT p{LONG(std::clamp(x, 0.f, float(r.right - 1))),
          LONG(std::clamp(y, 0.f, float(r.bottom - 1)))};
  ClientToScreen(window, &p);
  SetCursorPos(p.x, p.y);
  INPUT i{};
  i.type = INPUT_MOUSE;
  i.mi.dwFlags = right ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_LEFTDOWN;
  SendInput(1, &i, sizeof(i));
  if (right) {
    i.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
    SendInput(1, &i, sizeof(i));
  } else {
    mouseDown = true;
    cursorDragging = hold;
    mouseRelease = ImGui::GetTime() + .075;
  }
}
static void activate(const Item &item, float width, float height) {
  clickAt((item.rect.x() + 1) * .5f * width,
          (item.rect.y() + 1) * .5f * height);
  gate.blocked = 0xffff;
  stickArmed = false;
  log("Activate native item %d (%.3f, %.3f)", item.id, item.rect.x(),
      item.rect.y());
}
static void wheel(int amount) {
  INPUT i{};
  i.type = INPUT_MOUSE;
  i.mi.dwFlags = MOUSEEVENTF_WHEEL;
  i.mi.mouseData = (DWORD)amount;
  SendInput(1, &i, sizeof(i));
}
static int listSlot(int item, const ListView &view = list) {
  const auto it = std::find_if(view.entries.begin(), view.entries.end(),
                               [&](const ListItem &entry) { return entry.item == item; });
  return it == view.entries.end() ? -1 : int(it - view.entries.begin());
}
static std::string nativeLabel(uintptr_t addressValue) {
  // Only the verified list label descriptors carry plain text here.
  MEMORY_BASIC_INFORMATION region{};
  auto address = reinterpret_cast<const char *>(addressValue);
  if (!address || !VirtualQuery(address, &region, sizeof(region)) ||
      region.State != MEM_COMMIT || (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
    return {};
  auto room = reinterpret_cast<uintptr_t>(region.BaseAddress) + region.RegionSize - addressValue;
  char name[128]{};
  auto size = std::min(sizeof(name) - 1, size_t(room));
  if (!ReadProcessMemory(GetCurrentProcess(), address, name, size, nullptr))
    return {};
  return name;
}
static unsigned listLabel(const ListItem &entry) {
  auto name = nativeLabel(entry.text);
  if (name.empty())
    return 0;
  unsigned hash = 2166136261u;
  for (unsigned char c : name)
    hash = (hash ^ c) * 16777619u;
  return hash;
}
static void rememberList(int slot) {
  const auto &entry = list.entries[slot];
  auto &saved = listSelections[size_t(list.kind)];
  saved = {entry.value, listLabel(entry), list.first};
}
static int restoreList() {
  const auto &saved = listSelections[size_t(list.kind)];
  if (!saved.labelHash)
    return -1;
  for (int slot = 0; slot < int(list.entries.size()); ++slot)
    if (list.entries[slot].value == saved.value &&
        listLabel(list.entries[slot]) == saved.labelHash)
      return slot;
  int match = -1;
  for (int slot = 0; slot < int(list.entries.size()); ++slot)
    if (listLabel(list.entries[slot]) == saved.labelHash) {
      if (match >= 0)
        return -1;
      match = slot;
    }
  return match;
}
static void beginListScroll(int target, int slot, ListKind kind) {
  scrollTarget = target;
  scrollSlot = slot;
  scrollKind = kind;
  sentOffset = -1;
}
static void positionListPointer(float width, float height) {
  const auto &first = items[list.entries.front().item].rect;
  const auto &last = items[list.entries.back().item].rect;
  POINT point{LONG((first.x() + 1) * width * .5f),
              LONG((first.y() + last.y() + 2) * height * .25f)};
  ClientToScreen(window, &point);
  const int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
  const int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
  const int spanX = std::max(1, GetSystemMetrics(SM_CXVIRTUALSCREEN));
  const int spanY = std::max(1, GetSystemMetrics(SM_CYVIRTUALSCREEN));
  INPUT move{};
  move.type = INPUT_MOUSE;
  move.mi.dx = LONG((double(point.x - left) + .5) * 65536 / spanX);
  move.mi.dy = LONG((double(point.y - top) + .5) * 65536 / spanY);
  move.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
  SendInput(1, &move, sizeof(move));
}
static void updateListScroll(float width, float height, double time) {
  if (scrollTarget < 0)
    return;
  const bool garage = scrollKind >= ListKind::Setups;
  const auto &view = garage ? garageList(scrollKind) : list;
  if (view.kind != scrollKind || view.entries.empty()) {
    restoreAfterScroll = false;
    scrollTarget = -1;
    return;
  }
  if (!view.total || view.first == scrollTarget ||
      (view.first == sentOffset && time - scrollSentAt >= .26)) {
    const int restored = restoreAfterScroll ? restoreList() : -1;
    int slot = restored;
    if (slot < 0)
      slot = std::min(scrollSlot, int(view.entries.size()) - 1);
    selected = view.entries[slot].item;
    if (!garage && (!restoreAfterScroll || restored >= 0))
      rememberList(slot);
    restoreAfterScroll = false;
    scrollTarget = -1;
    return;
  }
  if (view.first == sentOffset || time - scrollSentAt < .016)
    return;
  if (garage) {
    if (mouseDown)
      return;
    const auto &arrow = view.first < scrollTarget ? view.scrollDown : view.scrollUp;
    clickAt((arrow.x() + 1) * width * .5f, (arrow.y() + 1) * height * .5f);
    sentOffset = view.first;
    scrollSentAt = time;
    return;
  }
  if (sentOffset == -1) {
    // The mods grid routes the wheel by the last processed mouse position.
    // Move inside it and let the window process that move before scrolling.
    positionListPointer(width, height);
    sentOffset = -2;
    scrollSentAt = time;
    return;
  }
  wheel(list.first < scrollTarget ? -WHEEL_DELTA : WHEEL_DELTA);
  sentOffset = list.first;
  scrollSentAt = time;
}
static const ListView &focusedList() {
  for (const auto &view : garageLists)
    if (listSlot(selected, view) >= 0)
      return view;
  return list;
}
static bool moveList(Direction direction) {
  const auto &view = focusedList();
  const int slot = listSlot(selected, view);
  if (slot < 0)
    return false;
  if (direction == Direction::Left || direction == Direction::Right) {
    const int next = slot + (direction == Direction::Left ? -1 : 1);
    if (view.columns <= 1 || next < 0 || next >= int(view.entries.size()) ||
        next / view.columns != slot / view.columns)
      return false;
    selected = view.entries[next].item;
    return true;
  }
  const int step = direction == Direction::Up ? -1 : 1;
  const int row = slot / view.columns;
  int next = (row + step) * view.columns;
  if (next >= 0 && next < int(view.entries.size())) {
    next = std::min(next + slot % view.columns, int(view.entries.size()) - 1);
    selected = view.entries[next].item;
    return true;
  }
  if (!view.total || view.first + step < 0 ||
      view.first + step > view.total - view.visible)
    return false;
  beginListScroll(view.first + step, slot, view.kind);
  return true;
}
static bool isGarageColourSlider(const Item &item) {
  if ((!garageRoot && !garageViewer) ||
      (item.nativeType != 38 && item.nativeType != 39))
    return false;
  const auto *native = reinterpret_cast<const NativeItem *>(imageBase + 0x28ca20);
  const auto callback = native[item.id].callback - uintptr_t(imageBase);
  return callback == 0xb4870 || callback == 0xb4920 || callback == 0xb2e00;
}
static int garageEntryFocus() {
  if (garageSkins) {
    const auto &view = garageList(ListKind::Skins);
    for (const auto &row : view.entries)
      if (!lastGarageSkin.empty() &&
          _stricmp(nativeLabel(row.text).c_str(), lastGarageSkin.c_str()) == 0)
        return row.item;
    if (!view.entries.empty())
      return view.entries.front().item;
    return -1;
  }
  const auto *native = reinterpret_cast<const NativeItem *>(imageBase + 0x28ca20);
  for (int i = 0; i < int(items.size()); ++i) {
    const auto &n = native[items[i].id];
    if (n.callback == garageReturnCallback && n.type == garageReturnType &&
        n.value == garageReturnValue)
      return i;
  }
  for (const auto &row : garageList(ListKind::Setups).entries)
    if (items[row.item].active)
      return row.item;
  return -1;
}
static int findShortcutItem(const ShortcutTarget &target, uintptr_t owner) {
  if (!target.callback) {
    const auto &setups = garageList(ListKind::Setups).entries;
    const auto active = std::find_if(setups.begin(), setups.end(),
                                    [](const ListItem &row) { return items[row.item].active; });
    return active != setups.end() ? active->item
           : setups.empty() ? -1 : setups.front().item;
  }
  const auto *native = reinterpret_cast<const NativeItem *>(imageBase + 0x28ca20);
  for (int i = 0; i < int(items.size()); ++i) {
    const auto &n = native[items[i].id];
    if (n.owner == owner && n.callback == uintptr_t(imageBase) + target.callback &&
        n.type == target.type && n.value == target.value)
      return i;
  }
  return -1;
}
static ShortcutScreen findShortcutScreen(uintptr_t modal, bool raceMenu) {
  if (!modal)
    return ShortcutScreen::None;
  if (mainMenuInitial >= 0)
    return ShortcutScreen::Main;
  if (raceMenu)
    return ShortcutScreen::Race;
  if (optionsInitial >= 0)
    return ShortcutScreen::Options;
  if (garageRoot)
    return ShortcutScreen::Garage;
  const auto &garage = shortcutMenus[std::size_t(ShortcutScreen::SinglePlayer)].targets[1];
  return findShortcutItem(garage, modal) >= 0 ? ShortcutScreen::SinglePlayer
                                              : ShortcutScreen::None;
}
static ResolvedShortcuts findMenuShortcuts(ShortcutScreen screen, uintptr_t modal) {
  if (screen == ShortcutScreen::None)
    return {};
  const auto index = std::size_t(screen);
  const auto &menu = shortcutMenus[index];
  const auto owner = menu.ownerRva ? uintptr_t(imageBase) + menu.ownerRva : modal;
  return resolveShortcuts(menu, cfg.shortcuts[index], [&](const ShortcutTarget &target) {
    return findShortcutItem(target, owner);
  });
}
static void adjustGarageColour(const Item &item, int direction, float width,
                               float height) {
  const auto *native = reinterpret_cast<const NativeItem *>(imageBase + 0x28ca20);
  const auto &control = native[item.id];
  if (!control.extra || control.extra > 255)
    return;
  unsigned char rgb[3]{};
  if (!ReadProcessMemory(GetCurrentProcess(),
                         reinterpret_cast<const void *>(control.value), rgb,
                         item.nativeType == 38 ? 1 : 3, nullptr))
    return;
  const unsigned current = std::max({rgb[0], rgb[1], rgb[2]});
  if (current > control.extra ||
      (direction < 0 && !current) ||
      (direction > 0 && current == control.extra))
    return;
  const auto r = item.rect;
  const auto t = colourSliderPosition(current, control.extra, direction);
  // The colored bar has a small inset for the native thumb.
  const float left = (r.l + 1) * width * .5f;
  const float right = (r.r + 1) * width * .5f;
  const float inset = std::min((r.b - r.t) * height * .25f, (right - left) * .1f);
  const float span = right - left - 2 * inset;
  const float y = (r.y() + 1) * height * .5f;
  clickAt(left + inset + span * (float(current) / control.extra), y, false, true);
  if (cursorDragging) {
    colourDestination = {std::lround(left + inset + span * t), std::lround(y)};
    ClientToScreen(window, &colourDestination);
    colourDragging = true;
    colourMoved = false;
    colourDragTime = ImGui::GetTime() + .025;
  }
}
static void moveGarageViewer(float x, float y, int zoom, float width,
                             float height, float dt, double time) {
  static double lastZoom = -1;
  static int lastDirection = 0;
  static float dragX = 0, dragY = 0;
  const float middleX = width * .5f, middleY = (viewerArea.y() + 1) * height * .5f;
  const float bottom = (viewerArea.b + 1) * height * .5f;
  if (bottom < 64 || GetForegroundWindow() != window)
    return;
  if (x || y) {
    if (!viewerDrag && !mouseDown) {
      dragX = middleX;
      dragY = middleY;
      clickAt(dragX, dragY, false, true);
      viewerDrag = cursorDragging;
    } else if (viewerDrag) {
      dragX += x * width * .28f * dt;
      dragY -= y * height * .28f * dt;
      if (dragX <= 8 || dragX >= width - 8 || dragY <= 8 || dragY >= bottom - 8) {
        // Restart the drag in the free viewing area on the next frame.
        releaseMouse();
      } else {
        POINT target{std::lround(dragX), std::lround(dragY)};
        ClientToScreen(window, &target);
        SetCursorPos(target.x, target.y);
      }
    }
  } else if (viewerDrag) {
    releaseMouse();
  }
  if (zoom && (zoom != lastDirection || time >= lastZoom + .14)) {
    if (!mouseDown) {
      POINT target{std::lround(middleX), std::lround(middleY)};
      ClientToScreen(window, &target);
      SetCursorPos(target.x, target.y);
    }
    wheel(zoom * WHEEL_DELTA);
    lastZoom = time;
  }
  lastDirection = zoom;
}
static void character(wchar_t ch) {
  INPUT i[2]{};
  for (auto &e : i) {
    e.type = INPUT_KEYBOARD;
    e.ki.wScan = ch;
    e.ki.dwFlags = KEYEVENTF_UNICODE;
  }
  i[1].ki.dwFlags |= KEYEVENTF_KEYUP;
  SendInput(2, i, sizeof(INPUT));
}
struct PendingKey {
  WORD vk;
  ULONGLONG until;
};
static std::vector<PendingKey> pendingKeys;
static void releaseKeys(bool all) {
  auto now = GetTickCount64();
  for (auto it = pendingKeys.begin(); it != pendingKeys.end();) {
    if (all || now >= it->until) {
      INPUT i{};
      i.type = INPUT_KEYBOARD;
      i.ki.wVk = it->vk;
      i.ki.dwFlags = KEYEVENTF_KEYUP;
      SendInput(1, &i, sizeof(i));
      it = pendingKeys.erase(it);
    } else
      ++it;
  }
}
void localKey(WORD vk) {
  if (GetForegroundWindow() != window)
    return;
  for (auto &key : pendingKeys)
    if (key.vk == vk)
      return;
  INPUT i{};
  i.type = INPUT_KEYBOARD;
  i.ki.wVk = vk;
  if (SendInput(1, &i, sizeof(i)))
    pendingKeys.push_back({vk, GetTickCount64() + 75});
}
static ImVec2 pixel(float x, float y) {
  auto s = ImGui::GetIO().DisplaySize;
  return {(x + 1) * s.x * .5f, (y + 1) * s.y * .5f};
}
static void label(float x, float y, const char *s,
                  ImU32 col = IM_COL32(245, 245, 245, 255)) {
  ImGui::GetForegroundDrawList()->AddText({x, y}, col, s);
}
static void hudLabel(float x, float y, const char *value,
                     ImU32 colour = IM_COL32(245, 245, 245, 255)) {
  label(x + 1, y + 1, value, IM_COL32(0, 0, 0, 240));
  label(x - 1, y + 1, value, IM_COL32(0, 0, 0, 185));
  label(x, y, value, colour);
}

static float glyphWidth(int button) {
  return (button >= PadLB && button <= PadL3) || button == PadLT ||
         button == PadRT ? 32.f : 24.f;
}

static void drawButtonGlyph(float x, float y, int button,
                            ImU32 colour = IM_COL32(245, 245, 245, 255)) {
  auto *draw = ImGui::GetForegroundDrawList();
  const float w = glyphWidth(button);
  const ImVec2 c{x + w * .5f, y + 11};
  constexpr ImU32 shadow = IM_COL32(0, 0, 0, 225);
  const auto line = [&](ImVec2 a, ImVec2 b) {
    draw->AddLine(a, b, shadow, 4);
    draw->AddLine(a, b, colour, 1.5f);
  };
  const auto circle = [&](float radius) {
    draw->AddCircle(c, radius, shadow, 24, 4);
    draw->AddCircle(c, radius, colour, 24, 1.5f);
  };
  const auto caption = [&](const char *value) {
    const auto size = ImGui::CalcTextSize(value);
    hudLabel(c.x - size.x * .5f, c.y - size.y * .5f, value, colour);
  };
  if (button <= PadY) {
    if (!playStationCaptions) {
      circle(9);
      caption(buttonNames[button]);
    } else if (button == PadA) {
      line({c.x - 6, c.y - 6}, {c.x + 6, c.y + 6});
      line({c.x + 6, c.y - 6}, {c.x - 6, c.y + 6});
    } else if (button == PadB) {
      circle(7);
    } else if (button == PadX) {
      draw->AddRect({c.x - 6, c.y - 6}, {c.x + 6, c.y + 6}, shadow, 0, 0, 4);
      draw->AddRect({c.x - 6, c.y - 6}, {c.x + 6, c.y + 6}, colour, 0, 0, 1.5f);
    } else {
      const ImVec2 a{c.x, c.y - 7}, b{c.x + 7, c.y + 6}, d{c.x - 7, c.y + 6};
      draw->AddTriangle(a, b, d, shadow, 4);
      draw->AddTriangle(a, b, d, colour, 1.5f);
    }
  } else if (button == PadDirections || (button >= PadUp && button <= PadRight)) {
    const ImVec2 cross[]{{c.x-4,c.y-9},{c.x+4,c.y-9},{c.x+4,c.y-4},
        {c.x+9,c.y-4},{c.x+9,c.y+4},{c.x+4,c.y+4},{c.x+4,c.y+9},
        {c.x-4,c.y+9},{c.x-4,c.y+4},{c.x-9,c.y+4},{c.x-9,c.y-4},{c.x-4,c.y-4}};
    draw->AddPolyline(cross, 12, shadow, ImDrawFlags_Closed, 4);
    draw->AddPolyline(cross, 12, colour, ImDrawFlags_Closed, 1.4f);
    if (button != PadDirections) {
      static constexpr ImVec2 tips[]{{0,-6},{0,6},{-6,0},{6,0}};
      const auto tip = tips[button - PadUp];
      draw->AddCircleFilled({c.x + tip.x, c.y + tip.y}, 2, colour, 10);
    }
  } else if (button == PadLeftStick || button == PadRightStick) {
    circle(9);
    caption(button == PadLeftStick ? "L" : "R");
  } else {
    const ImVec2 a{x + 2, y + 2}, b{x + w - 2, y + 20};
    draw->AddRect(a, b, shadow, 5, 0, 4);
    draw->AddRect(a, b, colour, 5, 0, 1.3f);
    if (button == PadStart) {
      for (int i = -1; i <= 1; ++i)
        line({c.x - 5, c.y + i * 3}, {c.x + 5, c.y + i * 3});
    } else if (button == PadBack) {
      draw->AddRect({c.x - 6, c.y - 2}, {c.x + 2, c.y + 4}, colour);
      draw->AddRect({c.x - 2, c.y - 4}, {c.x + 6, c.y + 2}, colour);
    } else if (button == PadLT || button == PadRT) {
      caption(button == PadLT ? (playStationCaptions ? "L2" : "LT")
                             : (playStationCaptions ? "R2" : "RT"));
    } else {
      caption(buttonCaption(button));
    }
  }
}

// On-demand instructions still identify the necessary button in plain text
// when the player hides the routine icon overlay.
static const char *hudButtonCaption(int button) {
  switch (button) {
  case PadLT: return playStationCaptions ? "L2" : "LT";
  case PadRT: return playStationCaptions ? "R2" : "RT";
  case PadDirections: return "D-pad";
  case PadLeftStick: return text("Sol analog", "Left stick");
  case PadRightStick: return text("Sağ analog", "Right stick");
  default: return buttonCaption(button);
  }
}

static float hudButtonWidth(int button) {
  return cfg.controllerIcons ? glyphWidth(button)
                             : ImGui::CalcTextSize(hudButtonCaption(button)).x;
}

static void drawHudButton(float x, float y, int button, ImU32 colour) {
  if (cfg.controllerIcons)
    drawButtonGlyph(x, y, button, colour);
  else
    hudLabel(x, y + (22 - ImGui::GetFontSize()) * .5f, hudButtonCaption(button), colour);
}

struct HudHint {
  int button;
  const char *action;
  int second = -1;
  ImU32 colour = IM_COL32(245, 245, 245, 255);
};

static void drawCornerHints(float width, float height, bool right,
                            std::initializer_list<HudHint> hints) {
  constexpr float gap = 13, margin = 19, lineHeight = 27;
  const float budget = (width - 3 * margin) * .5f;
  if (budget < 115)
    return;
  std::array<float, 8> widths{}, offsets{};
  std::array<int, 8> rows{};
  IM_ASSERT(hints.size() <= rows.size());
  int row = 0, i = 0;
  for (const auto &hint : hints) {
    if (!hint.action)
      continue;
    float w = ImGui::CalcTextSize(hint.action).x;
    if (hint.button >= 0)
      w += hudButtonWidth(hint.button) + 4;
    if (hint.second >= 0)
      w += hudButtonWidth(hint.second) + 4;
    if (widths[row] > 0 && widths[row] + gap + w > budget)
      ++row;
    offsets[i] = widths[row] > 0 ? widths[row] + gap : 0;
    rows[i] = row;
    widths[row] = offsets[i] + w;
    ++i;
  }

  // Keep each group pinned to its side, raising it only if it covers an
  // actionable game button near the bottom edge.
  const float groupHeight = (row + 1) * lineHeight;
  float y = height - margin - groupHeight;
  float best = std::numeric_limits<float>::infinity(), chosen = y;
  for (int step = 0; step < 4 && y >= margin; ++step, y -= 40) {
    float overlap = 0;
    for (const auto &item : items) {
      auto a = pixel(item.rect.l, item.rect.t);
      auto b = pixel(item.rect.r, item.rect.b);
      for (int r = 0; r <= row; ++r) {
        const float left = right ? width - margin - widths[r] : margin;
        const float top = y + r * lineHeight;
        overlap += std::max(0.f, std::min(left + widths[r], b.x) - std::max(left, a.x)) *
                   std::max(0.f, std::min(top + 22, b.y) - std::max(top, a.y));
      }
    }
    if (overlap < best) {
      chosen = y;
      best = overlap;
    }
    if (overlap == 0)
      break;
  }
  i = 0;
  auto *draw = ImGui::GetForegroundDrawList();
  draw->PushClipRect({right ? width * .5f : 0, 0},
                     {right ? width : width * .5f, height}, true);
  for (const auto &hint : hints) {
    if (!hint.action)
      continue;
    const auto r = rows[i];
    float x = (right ? std::max(width - margin - widths[r], width * .5f + 2) : margin) + offsets[i++];
    const float top = chosen + r * lineHeight;
    for (int button : {hint.button, hint.second}) {
      if (button >= 0) {
        drawHudButton(x, top, button, hint.colour);
        x += hudButtonWidth(button) + 4;
      }
    }
    hudLabel(x, top + (22 - ImGui::GetFontSize()) * .5f, hint.action, hint.colour);
  }
  draw->PopClipRect();
}

static void panel(float l, float t, float r, float b) {
  auto d = ImGui::GetForegroundDrawList();
  d->AddRectFilled({l, t}, {r, b}, IM_COL32(12, 19, 28, 242), 12);
  d->AddRect({l, t}, {r, b}, IM_COL32(78, 223, 186, 240), 12, 0, 2);
}
static void drawMenuShortcuts(float width, float height, const ResolvedShortcuts &shortcuts,
                              int selection) {
  const auto hint = [&](int direction) {
    const auto &entry = shortcuts[direction];
    const ImU32 colour = entry.item < 0 ? IM_COL32(151, 160, 171, 240)
        : entry.item == selection ? IM_COL32(105, 243, 211, 255)
                                  : IM_COL32(245, 245, 245, 255);
    return HudHint{PadUp + direction, entry.label ? entry.label->get(cfg.language) : nullptr,
                   -1, colour};
  };
  drawCornerHints(width, height, false, {
      hint(0), hint(1), {cfg.bindings[SettingsModifier], text("Bırak", "Release")}});
  drawCornerHints(width, height, true, {
      hint(2), hint(3), {cfg.bindings[Confirm], text("Aç", "Open")}});

}
static std::string utf8(const std::wstring &s) {
  int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0,
                              nullptr, nullptr);
  std::string out(n, 0);
  WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n,
                      nullptr, nullptr);
  return out;
}
static void keyAction() {
  auto rows = keyboardRows(cfg.language, upper, symbols);
  if (keyRow < 4) {
    character(rows[keyRow][keyCol]);
    return;
  }
  switch (keyCol) {
  case 0:
    character(L' ');
    break;
  case 1:
    localKey(VK_BACK);
    break;
  case 2:
    upper = !upper;
    break;
  case 3:
    symbols = !symbols;
    break;
  case 4:
    simKey("enter");
    keyboard = manualKeyboard = false;
    break;
  case 5:
    simKey("esc");
    keyboard = manualKeyboard = false;
    break;
  }
}
static void drawKeyboard(float width, float height) {
  float cw = std::min(width * .052f, 64.f), ch = std::min(height * .065f, 54.f),
        w = cw * 12 + 40, l = (width - w) / 2,
        t = std::max(70.f, height - ch * 5 - 110);
  panel(l, t, l + w, t + ch * 5 + 90);
  char hint[160];
  std::snprintf(hint, sizeof(hint),
                text("TÜRKÇE Q   |   %s: Sil   %s: Büyük/küçük   %s: Tamam", "ENGLISH QWERTY   |   %s: Delete   %s: Change case   %s: OK"),
                bindingName(Secondary), bindingName(Keyboard),
                bindingName(Menu));
  label(l + 20, t + 12, hint);
  auto rows = keyboardRows(cfg.language, upper, symbols);
  auto d = ImGui::GetForegroundDrawList();
  for (int row = 0; row < 5; ++row) {
    int count = row == 4 ? 6 : int(rows[row].size());
    float cell = row == 4 ? cw * 2 : cw;
    for (int col = 0; col < count; ++col) {
      float x = l + 20 + col * cell, y = t + 48 + row * ch;
      bool sel = row == keyRow && col == keyCol;
      d->AddRectFilled(
          {x + 2, y + 2}, {x + cell - 2, y + ch - 2},
          sel ? IM_COL32(40, 170, 133, 255) : IM_COL32(40, 50, 65, 255), 5);
      const char *special[] = {text("Boşluk", "Space"), text("Sil", "Delete"),   "Aa",
                                      "#+=",    text("Tamam", "OK"), text("İptal", "Cancel")};
      auto caption = row == 4 ? std::string(special[col])
                           : utf8(std::wstring(1, rows[row][col]));
      label(x + 10, y + ch * .28f, caption.c_str());
    }
  }
  // No mirror of the field contents: LFS retains password masking and text
  // limits.
}
static void adjustSetting(int direction) {
  switch (settingRow) {
  case controllerSetting:
    cfg.slot = (cfg.slot + 1 + direction + 6) % 6 - 1;
    missingSlot = false;
    break;
  case languageSetting:
    cfg.language = cfg.language == Language::English ? Language::Turkish : Language::English;
    break;
  case controllerIconsSetting:
    cfg.controllerIcons = !cfg.controllerIcons;
    break;
  case deadzoneSetting:
    cfg.deadzone = std::clamp(cfg.deadzone + direction * .01f, .1f, .45f);
    break;
  case cursorSpeedSetting:
    cfg.cursorSpeed =
        std::clamp(cfg.cursorSpeed + direction * 50.f, 200.f, 2400.f);
    break;
  case quickMenuSetting:
    cfg.quickButton = (cfg.quickButton + direction + 4) % 4;
    break;
  case monitorSetting:
    cfg.inputMonitor = !cfg.inputMonitor;
    break;
  case shortcutSetting:
    break;
  case rumbleSetting:
    cfg.rumbleStrength = std::clamp(cfg.rumbleStrength + direction * 10, 0, 100);
    break;
  default:
    int action = settingRow - firstBindingSetting;
    const int count = int(physical.size());
    rebind(cfg.bindings, action, (cfg.bindings[action] + direction + count) % count);
    break;
  }
}
static constexpr float settingsWidth = 620.f, settingsHeight = 420.f;
static constexpr float meterColumn = 178.f, meterHeight = 160.f, meterMargin = 19.f;
static void blockMenuInput() {
  gate.blocked = 0xffff;
  stickArmed = false;
  for (auto &directionRepeat : repeat)
    directionRepeat = {};
}
static void adjustShortcut(int step) {
  if (shortcutEditRow == 0) {
    const int count = int(shortcutMenus.size());
    shortcutEditMenu = (shortcutEditMenu + count + step) % count;
  } else {
    const auto &menu = shortcutMenus[shortcutEditMenu];
    auto &directions = shortcutEditDraft[shortcutEditMenu];
    const int direction = shortcutEditRow - 1;
    const int target = (directions[direction] + menu.count + step) % menu.count;
    // Cycling past a target only previews that swap. Keep the final swap.
    directions = directionsBeforeEdit;
    assignShortcut(directions, direction, target);
  }
}
static void drawShortcutEditor(float l, float t, float w) {
  panel(l, t, l + w, t + settingsHeight);
  label(l + 22, t + 16, text("KİŞİSEL MENÜ KISAYOLLARI", "CUSTOM MENU SHORTCUTS"));
  const auto &menu = shortcutMenus[shortcutEditMenu];
  const auto &directions = shortcutEditDraft[shortcutEditMenu];
  const char *rows[] = {text("Menü", "Menu"), text("Yukarı", "Up"), text("Aşağı", "Down"), text("Sol", "Left"), text("Sağ", "Right")};
  auto *draw = ImGui::GetForegroundDrawList();
  for (int row = 0; row < 5; ++row) {
    const float y = t + 58 + row * 49.f;
    if (row == shortcutEditRow)
      draw->AddRectFilled({l + 14, y}, {l + w - 14, y + 43},
                          IM_COL32(35, 110, 94, 255), 4);
    label(l + 25, y + 11, rows[row]);
    label(l + w * .36f, y + 11,
          (row == 0 ? menu.label : menu.targets[directions[row - 1]].label).get(cfg.language));
  }
  char hint[128];
  std::snprintf(hint, sizeof(hint), text("D-pad: seç / değiştir   %s: varsayılanlara dön", "D-pad: select / change   %s: restore defaults"),
                buttonCaption(2));
  label(l + 22, t + 342, hint);
  std::snprintf(hint, sizeof(hint), text("%s: değişiklikleri al   %s: iptal", "%s: apply changes   %s: cancel"),
                buttonCaption(0), buttonCaption(1));
  label(l + 22, t + 374, hint);
}
static void drawSettings(float l, float t, float w) {
  panel(l, t, l + w, t + settingsHeight);
  label(l + 22, t + 16, text("LfsPad ayarları", "LfsPad settings"));
  const char *actionNames[] = {text("Onay", "Confirm"),
                                    text("Geri", "Back"),
                                    text("Sağ tıklama / sil", "Right click / delete"),
                                    text("Klavye / büyük harf", "Keyboard / change case"),
                                    text("Önceki sekme", "Previous tab"),
                                    text("Sonraki sekme", "Next tab"),
                                    text("Menü / metni onayla", "Menu / submit text"),
                                    text("Menü kısayolları için basılı tut", "Hold for menu shortcuts"),
                                    text("İmleç modu", "Cursor mode")};
  char values[settingCount][100];
  if (cfg.slot == nativePreference)
    std::snprintf(values[controllerSetting], 80, "%s", text("Kontrolcü: Yerel gamepad / USB adaptör", "Controller: Native gamepad / USB adapter"));
  else if (cfg.slot >= 0)
    std::snprintf(values[controllerSetting], 80, text("Kontrolcü: %d", "Controller: %d"), cfg.slot + 1);
  else
    std::snprintf(values[controllerSetting], 80, "%s", text("Kontrolcü: Otomatik", "Controller: Auto"));
  std::snprintf(values[languageSetting], 100, "%s", text("Dil: Türkçe", "Language: English"));
  std::snprintf(values[controllerIconsSetting], 100, text("Kontrolcü simgeleri: %s", "Controller icons: %s"),
                cfg.controllerIcons ? text("Açık", "On") : text("Kapalı", "Off"));
  std::snprintf(values[deadzoneSetting], 80, text("Analog ölü bölgesi: %d%%", "Stick deadzone: %d%%"),
                int(cfg.deadzone * 100 + .5f));
  std::snprintf(values[cursorSpeedSetting], 80, text("İmleç hızı: %.0f", "Cursor speed: %.0f"), cfg.cursorSpeed);
  for (int i = 0; i < 9; ++i)
    std::snprintf(values[i + firstBindingSetting], 100, "%s: %s", actionNames[i],
                  buttonCaption(cfg.bindings[i]));
  std::snprintf(values[quickMenuSetting], 100, text("Hızlı işlem menüsü: %s", "Quick actions: %s"),
                quickButtonCaption());
  std::snprintf(values[monitorSetting], 100, text("Canlı giriş göstergesi: %s", "Live input display: %s"),
                cfg.inputMonitor ? text("Açık", "On") : text("Kapalı", "Off"));
  std::snprintf(values[shortcutSetting], 100, "%s", text("Kişisel menü kısayolları", "Custom menu shortcuts"));
  std::snprintf(values[rumbleSetting], 100, text("Sürüş titreşimi: %d%%", "Driving vibration: %d%%"),
                cfg.rumbleStrength);
  int first = std::clamp(settingRow - 3, 0, settingCount - 6);
  for (int row = 0; row < 6; ++row) {
    int i = first + row;
    if (i == settingRow)
      ImGui::GetForegroundDrawList()->AddRectFilled(
          {l + 14, t + 58 + row * 45.f}, {l + w - 14, t + 98 + row * 45.f},
          bindingCapture.active() ? IM_COL32(126, 84, 30, 255)
                                  : IM_COL32(35, 110, 94, 255), 4);
    label(l + 25, t + 68 + row * 45.f, values[i]);
  }
  if (bindingCapture.active()) {
    label(l + 22, t + 342,
          bindingCapture.phase == BindingCapture::Phase::Release
              ? text("Önce basılı tuşları bırak.", "Release the buttons first.")
              : text("Kullanmak istediğin tuşa bas.", "Press the button you want to assign."),
          IM_COL32(255, 210, 90, 255));
    label(l + 22, t + 374, playStationCaptions
        ? text("L2 + R2 ile iptal et", "Press L2 + R2 to cancel")
        : text("LT + RT ile iptal et", "Press LT + RT to cancel"));
  } else if (settingRow == shortcutSetting) {
    char hint[128];
    std::snprintf(hint, sizeof(hint), text("D-pad: seç   %s: kısayolları düzenle   %s: iptal", "D-pad: select   %s: edit shortcuts   %s: cancel"),
                  buttonCaption(0), buttonCaption(1));
    label(l + 22, t + 342, hint);
  } else {
    char hint[128];
    std::snprintf(hint, sizeof(hint), text("D-pad: seç / değiştir   %s: kaydet   %s: iptal", "D-pad: select / change   %s: save   %s: cancel"),
                  buttonCaption(0), buttonCaption(1));
    label(l + 22, t + 342, hint);
    if (isBindingSetting())
      std::snprintf(hint, sizeof(hint), text("%s: tuş ata   Tuş doluysa iki görev yer değiştirir.", "%s: assign button   Buttons already in use are swapped."),
                    buttonCaption(2));
    else if (settingRow == rumbleSetting)
      std::snprintf(hint, sizeof(hint), text("%s: titreşimi dene    %%0: kapalı", "%s: test vibration   0%%: off"), buttonCaption(2));
    else
      hint[0] = 0;
    label(l + 22, t + 374, hint);
  }
  if (missingSlot)
    label(l + 22, t + 320, text("Seçtiğin kontrolcüyü bağla.", "Connect the selected controller."),
          IM_COL32(255, 210, 90, 255));
  else if (settingsSaveFailed)
    label(l + 22, t + 320, text("Ayarlar kaydedilemedi. A ile tekrar dene.", "Couldn't save settings. Press A to try again."),
          IM_COL32(255, 210, 90, 255));
  else if (settingRow == rumbleSetting && vibrationPreviewResult != ERROR_CANCELLED) {
    const char *message = vibrationPreviewResult == ERROR_SUCCESS
        ? text("Sürücü titreşim komutunu kabul etti.", "The driver accepted the vibration command.")
        : text("Bu bağlantıda titreşim komutu çalışmadı.", "Vibration failed on this connection.");
    label(l + 22, t + 320, message);
  }
}
static constexpr ImU32 meterActive = IM_COL32(78, 223, 186, 255),
                       meterMuted = IM_COL32(166, 185, 202, 255),
                       meterWarning = IM_COL32(255, 210, 90, 255);
// XUSB DirectInput button instances, indexed into the physical button table.
static constexpr int meterButtonOrder[] = {0, 1, 2, 3, 4, 5, 7, 6, 9, 8};
static void drawMeterAxis(float x, float y, float width, const char *name,
                          float value, bool available, bool centered) {
  hudLabel(x, y, name, meterMuted);
  char percent[16] = "-";
  if (available)
    std::snprintf(percent, sizeof(percent), "%ld%%", std::lround(value * 100.f));
  hudLabel(x + width - ImGui::CalcTextSize(percent).x, y, percent);
  auto draw = ImGui::GetForegroundDrawList();
  draw->AddLine({x, y + 18}, {x + width, y + 18}, IM_COL32(0, 0, 0, 210), 4);
  draw->AddLine({x, y + 18}, {x + width, y + 18}, meterMuted, 1);
  const float neutral = centered ? width * .5f : 0;
  const float amount = value * (centered ? width * .5f : width);
  if (available && amount != 0)
    draw->AddRectFilled({x + neutral + std::min(0.f, amount), y + 17},
                        {x + neutral + std::max(0.f, amount), y + 20},
                        meterActive, 1);
  if (centered)
    draw->AddLine({x + neutral, y + 15}, {x + neutral, y + 21}, meterMuted);
}
static void drawMeterButtons(float x, float y, float width,
                             std::uint16_t available, std::uint16_t down,
                             std::uint16_t suppressed) {
  const float cell = width / 5;
  for (int i = 0; i < 10; ++i) {
    const auto mask = 1u << i;
    if (!(available & mask))
      continue;
    const auto button = meterButtonOrder[i];
    const float left = x + (i % 5) * cell + (cell - glyphWidth(button)) * .5f;
    const float top = y + (i / 5) * 23.f;
    drawButtonGlyph(left, top, button,
                    (suppressed & mask) ? meterWarning
                    : (down & mask) ? meterActive : meterMuted);
  }
}
static void drawInputMeter(float width, float y, const XINPUT_GAMEPAD &gamepad,
                           bool connected, ControllerId slot, bool singlePad,
                           const DrivingInputSample &sample) {
  constexpr float column = meterColumn;
  const float left = meterMargin, right = width - meterMargin - column;
  char slotText[48];
  std::snprintf(slotText, sizeof(slotText), "%s", text("Kontrolcü bekleniyor", "Waiting for controller"));
  if (connected) {
    if (isNativeController(slot))
      std::snprintf(slotText, sizeof(slotText), "%s", text("Yerel gamepad / adaptör", "Native gamepad / adapter"));
    else
      std::snprintf(slotText, sizeof(slotText), text("Kontrolcü %d", "Controller %d"), int(slot) + 1);
  }
  hudLabel(left, y, slotText, meterActive);
  const char *status = !connected ? text("Kontrolcü bağlı değil", "Controller disconnected")
                       : !singlePad || sample.ambiguous ? text("Birden çok cihaz", "Multiple devices")
                       : !sample.tick ? text("LFS’den veri bekleniyor", "Waiting for LFS input")
                       : sample.axesFiltered ? text("Eksenler sıfırda", "Axes at neutral")
                       : sample.buttonsFiltered ? text("Tuş filtresi açık", "Buttons filtered")
                       : text("Oyunun gördüğü", "Seen by LFS");
  hudLabel(right, y, status, sample.tick ? meterActive : meterWarning);
  drawMeterAxis(left, y + 22, column, text("Direksiyon", "Steering"),
                axisFraction(gamepad.sThumbLX, -32768, 32767), connected, true);
  drawMeterAxis(left, y + 46, column, text("Gaz (RT)", "Throttle (RT)"),
                gamepad.bRightTrigger / 255.f, connected, false);
  drawMeterAxis(left, y + 70, column, text("Fren (LT)", "Brake (LT)"),
                gamepad.bLeftTrigger / 255.f, connected, false);
  drawMeterAxis(right, y + 22, column, text("Sol X", "Left X"), sample.steering,
                sample.steeringAvailable, true);
  if (sample.splitPedals) {
    drawMeterAxis(right, y + 46, column, text("Gaz (RT)", "Throttle (RT)"), sample.throttle, true, false);
    drawMeterAxis(right, y + 70, column, text("Fren (LT)", "Brake (LT)"), sample.brake, true, false);
  } else {
    drawMeterAxis(right, y + 46, column, text("Ortak Z (LT/RT)", "Combined Z (LT/RT)"), sample.triggers,
                  sample.triggersAvailable, true);
  }

  const auto raw = gamepad.wButtons;
  char direction[40];
  std::snprintf(direction, sizeof(direction), "D-pad  %s%s%s%s%s",
                raw & XINPUT_GAMEPAD_DPAD_UP ? "↑" : "",
                raw & XINPUT_GAMEPAD_DPAD_DOWN ? "↓" : "",
                raw & XINPUT_GAMEPAD_DPAD_LEFT ? "←" : "",
                raw & XINPUT_GAMEPAD_DPAD_RIGHT ? "→" : "",
                (raw & 0xf) ? "" : connected ? text("Nötr", "Neutral") : "-");
  hudLabel(left, y + 94, direction, (raw & 0xf) ? meterActive : meterMuted);
  static const char *povNames[] = {"↑", "↑→", "→", "↓→", "↓", "↓←", "←", "↑←"};
  const bool povMoved = sample.povAvailable && sample.pov < 36000;
  const char *pov = !sample.povAvailable ? "-" : !povMoved ? text("Nötr", "Neutral")
                         : povNames[((sample.pov + 2250) / 4500) % 8];
  std::snprintf(direction, sizeof(direction), "D-pad  %s", pov);
  hudLabel(right, y + 94, direction, povMoved ? meterActive : meterMuted);

  if (cfg.controllerIcons) {
    std::uint16_t rawDown = 0;
    for (int i = 0; i < 10; ++i)
      if (raw & physical[meterButtonOrder[i]])
        rawDown |= 1u << i;
    drawMeterButtons(left, y + 114, column, connected ? 0x3ff : 0, rawDown, 0);
    drawMeterButtons(right, y + 114, column, sample.buttonsAvailable,
                     sample.buttonsDown, sample.buttonsSuppressed);
  }
}
static void drawQuickMenu(float width, float height, QuickAction selection) {
  const float w = std::min(390.f, width - 24), l = (width - w) * .5f;
  const float t = height * .48f;
  panel(l, t, l + w, t + 126);
  auto option = [&](float x, float y, const char *name, QuickAction action) {
    label(x - ImGui::CalcTextSize(name).x * .5f, y, name,
          action == selection ? IM_COL32(70, 255, 205, 255)
                              : IM_COL32(230, 239, 248, 255));
  };
  option(width * .5f, t + 12, text("↑ Kamera", "↑ Camera"), QuickAction::Camera);
  option(l + w * .23f, t + 50, text("← Ayna", "← Mirrors"), QuickAction::Mirror);
  option(l + w * .77f, t + 50, text("Pit bilgisi →", "Pit info →"), QuickAction::Pit);
  char hint[96];
  std::snprintf(hint, sizeof(hint), text("%s tuşunu bırak: uygula   |   Ortada: iptal", "Release %s to apply   |   Center stick to cancel"),
                quickButtonCaption());
  label(width * .5f - ImGui::CalcTextSize(hint).x * .5f, t + 91, hint);
}
static void applyQuickAction(QuickAction action) {
  switch (action) {
  case QuickAction::Camera:
    simCommand("/press v");
    break;
  case QuickAction::Pit:
    simCommand("/press f12");
    break;
  case QuickAction::Mirror:
    simCommand("/shift m");
    break;
  case QuickAction::None:
    return;
  }
  log("Quick action: %u", unsigned(action));
}
static void drawReplay(float width, float height, const ReplayState &replay) {
  char clock[64];
  const unsigned now = replay.position / 1000, total = replay.duration / 1000;
  if (replay.seeking)
    std::snprintf(clock, sizeof(clock), "%s", text("Tekrar sarılıyor...", "Seeking..."));
  else
    std::snprintf(clock, sizeof(clock), "%u:%02u / %u:%02u",
                  now / 60, now % 60, total / 60, total % 60);
  if (!cfg.controllerIcons) {
    drawCornerHints(width, height, true, {{-1, clock}});
    return;
  }
  drawCornerHints(width, height, false, {
      {cfg.bindings[Confirm], replay.paused ? text("Oynat", "Play") : text("Duraklat", "Pause")},
      {cfg.bindings[Back], text("Menü", "Menu")}});
  drawCornerHints(width, height, true, {{-1, clock}, {PadLT, "-5 s"}, {PadRT, "+5 s"}});
}
static void drawRecovery(float width, float height, RecoveryPhase phase,
                         RaceSession session, bool connected, bool canResume) {
  if (width < 64)
    return;
  const char *title = text("Kontrolcü bağlantısı kesildi", "Controller disconnected");
  const char *detail = text("Kontrolcüyü yeniden bağla", "Reconnect your controller");
  char hint[96];
  switch (phase) {
  case RecoveryPhase::Pausing:
    detail = text("Oyun duraklatılıyor...", "Pausing the game...");
    break;
  case RecoveryPhase::Paused:
    if (connected) {
      title = text("Kontrolcü yeniden bağlandı", "Controller reconnected");
      if (session == RaceSession::Unknown)
        detail = text("Oyun durumu doğrulanıyor...", "Checking game state...");
      else if (canResume) {
        std::snprintf(hint, sizeof(hint), text("%s: Devam et", "%s: Resume"), bindingName(Confirm));
        detail = hint;
      } else
        detail = text("Devam etmek için açık menüyü kapat", "Close the menu to resume");
    } else
      detail = text("Oyun duraklatıldı. Kontrolcüyü yeniden bağla.", "Game paused. Reconnect your controller.");
    break;
  case RecoveryPhase::Resuming:
    title = text("Kontrolcü yeniden bağlandı", "Controller reconnected");
    detail = text("Sürüşe dönülüyor...", "Resuming the race...");
    break;
  default:
    if (session == RaceSession::Multiplayer)
      detail = text("Çevrimiçi yarış sürüyor", "Online race still running");
    break;
  }
  const float w = std::min(width - 20, std::max(ImGui::CalcTextSize(title).x,
                                               ImGui::CalcTextSize(detail).x) + 32);
  const float l = (width - w) * .5f, t = height * .28f;
  panel(l, t, l + w, t + 72);
  auto d = ImGui::GetForegroundDrawList();
  d->PushClipRect({l, t}, {l + w, t + 72}, true);
  label(l + 16, t + 12, title, IM_COL32(255, 210, 90, 255));
  label(l + 16, t + 42, detail);
  d->PopClipRect();
}
static void drawFinishHold(const Item &item, float width, double now) {
  if (width < 64)
    return;
  auto button = pixel(item.rect.l, item.rect.t);
  const auto *hint = text("Yarışı bitirmek için basılı tut", "Hold to end the race");
  const float buttonWidth = hudButtonWidth(cfg.bindings[Confirm]);
  const float w = std::min(buttonWidth + ImGui::CalcTextSize(hint).x + 30, width - 20);
  const float left = std::clamp(button.x, 10.f, std::max(10.f, width - w - 10));
  const float top = std::max(10.f, button.y - 54);
  auto draw = ImGui::GetForegroundDrawList();
  draw->PushClipRect({left, top}, {left + w, top + 44}, true);
  drawHudButton(left + 12, top + 4, cfg.bindings[Confirm], IM_COL32(245, 245, 245, 255));
  hudLabel(left + 16 + buttonWidth, top + 6, hint);
  const float right = left + w - 12;
  draw->AddRectFilled({left + 12, top + 34}, {right, top + 38},
                      IM_COL32(75, 87, 100, 255), 2);
  const float progress = finishHold.progress(now);
  if (progress > 0)
    draw->AddRectFilled({left + 12, top + 34},
                        {left + 12 + (right - left - 12) * progress, top + 38},
                        IM_COL32(65, 255, 196, 255), 2);
  draw->PopClipRect();
}
void releaseTarget() {
  stopMotors();
  cancelFinishHold();
  if (viewerDrag || colourDragging)
    releaseMouse();
  if (target) {
    target->Release();
    target = nullptr;
  }
}
static bool initialize(IDXGISwapChain *swap) {
  DXGI_SWAP_CHAIN_DESC desc{};
  if (FAILED(swap->GetDesc(&desc)))
    return false;
  DWORD pid = 0;
  GetWindowThreadProcessId(desc.OutputWindow, &pid);
  if (pid != GetCurrentProcessId() || !IsWindowVisible(desc.OutputWindow))
    return false;
  if (FAILED(swap->GetDevice(__uuidof(ID3D11Device), (void **)&device)))
    return false;
  device->GetImmediateContext(&context);
  window = desc.OutputWindow;
  ImGui::CreateContext();
  auto &io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  static const ImWchar ranges[] = {0x20, 0x17f, 0x20ac, 0x20ac,
                                    0x2190, 0x2193, 0};
  io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 20.f, nullptr,
                               ranges);
  if (io.Fonts->Fonts.empty())
    io.Fonts->AddFontDefault();
  if (!ImGui_ImplDX11_Init(device, context)) {
    ImGui::DestroyContext();
    context->Release();
    device->Release();
    return false;
  }
  previousWndProc = reinterpret_cast<WNDPROC>(
      SetWindowLongPtrA(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(windowProc)));
  if (!previousWndProc) {
    ImGui_ImplDX11_Shutdown();
    ImGui::DestroyContext();
    context->Release();
    device->Release();
    return false;
  }
  activeSwap = swap;
  loadConfig();
  log("LfsPad " LFSPAD_VERSION " active: PID=%lu HWND=%p Direct3D11, native table "
      "RVA=28ca20",
      GetCurrentProcessId(), window);
  return true;
}
void frame(IDXGISwapChain *swap) {
  if (!initialized) {
    if (!initialize(swap))
      return;
    initialized = true;
  }
  if (swap != activeSwap)
    return;
  if (!target) {
    ID3D11Texture2D *back = nullptr;
    if (FAILED(swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&back)))
      return;
    device->CreateRenderTargetView(back, nullptr, &target);
    back->Release();
    if (!target)
      return;
  }
  RECT client{};
  GetClientRect(window, &client);
  float width = float(client.right), height = float(client.bottom);
  if (width <= 0 || height <= 0)
    return;
  auto &io = ImGui::GetIO();
  double now = GetTickCount64() * .001;
  float dt = previousTime ? float(std::min(now - previousTime, .1)) : 1.f / 60;
  previousTime = now;
  io.DeltaTime = std::max(.001f, dt);
  io.DisplaySize = {width, height};
  ImGui_ImplDX11_NewFrame();
  ImGui::NewFrame();
  double time = ImGui::GetTime();
  if (mouseDown && !cursorDragging && time >= mouseRelease)
    releaseMouse();
  auto sim = pollSim();
  const auto replay = replayState();
  const auto modal = *reinterpret_cast<const uintptr_t *>(imageBase + 0x49c298);
  const bool nativeText = modal == uintptr_t(imageBase + 0x95ec20);
  const bool wasGarageRoot = garageRoot, wasGarageViewer = garageViewer;
  const bool recoveringGarageFocus = garageSkins || garageModal || wasGarageRoot;
  const bool raceMenu = gather(modal, sim.known && sim.mode == 3);
  if (garageRoot && !recoveringGarageFocus && !wasGarageViewer)
    garageReturnCallback = 0;
  const bool playingReplay = (replay.active || (sim.flags & 2)) &&
                             sim.mode == 0 && !nativeText && !raceMenu &&
                             !(sim.flags & (8 | 16 | 256 | 32768));
  const bool menu = !playingReplay &&
                    menuCapture(sim.known, sim.flags, sim.mode,
                                nativeText || raceMenu || garageRoot || garageViewer || garageSkins);
  const bool shortcutEditorWasOpen = shortcutEditor;
  if (!menu && settings) {
    loadConfig();
    settings = false;
  }
  bool foreground = GetForegroundWindow() == window;
  // Native menus also poll keyboard state. A down/up in the same call can be
  // missed between frames, so hold generated control keys briefly.
  releaseKeys(!foreground);
  updateNativeControllers(time);
  XINPUT_STATE state{};
  static int xinputCount = 0;
  static double checkPads = 0;
  const ControllerId oldSlot = activeSlot;
  if (inputSlot >= 0 &&
      (inputSlot != nativePreference || !isNativeController(activeSlot)))
    activeSlot = controllerForPreference(inputSlot, activeSlot);
  const auto controllerStatus = readController(activeSlot, state);
  bool connected = controllerStatus == ERROR_SUCCESS;
  const bool lostPad = wasConnected && oldSlot == activeSlot &&
                       controllerStatus == ERROR_DEVICE_NOT_CONNECTED;
  if (!connected && inputSlot == nativePreference) {
    activeSlot = controllerForPreference(inputSlot, activeSlot);
    connected = readController(activeSlot, state) == ERROR_SUCCESS;
  }
  if (!connected && inputSlot < 0) {
    static double nextScan = 0;
    if (time >= nextScan) {
      nextScan = time + 1;
      for (int i = 0; i < 4; ++i)
        if (readXInputController(i, state) == ERROR_SUCCESS) {
          activeSlot = i;
          connected = true;
          break;
        }
      if (!connected) {
        activeSlot = controllerForPreference(nativePreference, activeSlot);
        connected = readController(activeSlot, state) == ERROR_SUCCESS;
      }
    }
  }
  if (time >= checkPads || connected != wasConnected || oldSlot != activeSlot) {
    checkPads = time + 1;
    xinputCount = 0;
    for (int slot = 0; slot < 4; ++slot) {
      XINPUT_STATE other{};
      if (readXInputController(slot, other) == ERROR_SUCCESS)
        ++xinputCount;
    }
  }
  const int padCount = xinputCount + nativeControllerCount();
  const bool singlePad = connected && padCount == 1;
  const bool native = isNativeController(activeSlot);
  playStationCaptions = native && controllerUsesPlayStationButtons(activeSlot);
  const auto nativeVidPid = native ? controllerVidPid(activeSlot) : 0;
  if (connected != wasConnected || oldSlot != activeSlot)
    log("%s connected=%d device=%lld", native ? "Native gamepad" : "XInput",
        connected, static_cast<long long>(activeSlot));
  std::uint32_t stamp = 2166136261;
  for (auto &item : items) {
    stamp = (stamp ^ item.signature) * 16777619;
    unsigned f[4];
    std::memcpy(f, &item.rect, 16);
    for (auto x : f)
      stamp = (stamp ^ x) * 16777619;
  }
  const bool layoutChanged = stamp != screenStamp;
  const bool newList = list.kind != previousList;
  const bool inList = list.kind != ListKind::None;
  const bool garageScrolling = scrollTarget >= 0 && scrollKind >= ListKind::Setups &&
                               (garageRoot || garageSkins);
  static bool wasRaceMenu = false;
  const bool enteredRaceMenu = raceMenu && !wasRaceMenu;
  const bool screenChanged = newList || wasGarageViewer != garageViewer ||
      (layoutChanged && !inList && !garageScrolling);
  screenStamp = stamp;
  if (screenChanged || (menu && !wasMenu)) {
    const bool keepColour = editingGarageColour && wasGarageRoot && garageRoot;
    adjustingSlider = false;
    editingGarageColour = false;
    selected = items.empty() ? -1 : 0;
    // The entry screen has news links before its menu buttons. Recognise its
    // actual callback/value, rather than applying a geometry guess to lists.
    if (mainMenuInitial >= 0)
      selected = mainMenuInitial;
    if (optionsInitial >= 0)
      selected = optionsInitial;
    if (enteredRaceMenu && raceResumeItem >= 0)
      selected = raceResumeItem;
    if (garageRoot || garageSkins) {
      const int entry = garageRoot && wasGarageViewer && garageViewerItem >= 0
                            ? garageViewerItem : garageEntryFocus();
      if (entry >= 0) {
        selected = entry;
        adjustingSlider = editingGarageColour =
            keepColour && isGarageColourSlider(items[entry]);
      }
    }
    for (auto &r : repeat)
      r = {};
  }
  if (newList || !foreground || !menu) {
    scrollTarget = -1;
    restoreAfterScroll = false;
  }
  if (menu && inList && scrollTarget < 0 && (newList || layoutChanged)) {
    const int restored = restoreList();
    if (restored >= 0)
      selected = list.entries[restored].item;
    else if (newList) {
      selected = list.entries.front().item;
      const auto &saved = listSelections[size_t(list.kind)];
      if (foreground && saved.labelHash && list.total &&
          saved.first != list.first && saved.first <= list.total - list.visible) {
        beginListScroll(saved.first, 0, list.kind);
        restoreAfterScroll = true;
      }
    }
  }
  if (selected >= int(items.size()))
    selected = items.empty() ? -1 : 0;
  static bool wasReplay = false;
  const bool contextChanged =
      menu != wasMenu || playingReplay != wasReplay ||
      foreground != wasForeground || connected != wasConnected ||
      oldSlot != activeSlot;
  const bool reset = screenChanged || contextChanged;
  auto buttons = state.Gamepad.wButtons;
  auto pressed = gate.update(buttons, reset || !foreground || !connected);
  if (reset)
    stickArmed = false;
  if (reset && cursorDragging)
    releaseMouse();
  float lx = analog(state.Gamepad.sThumbLX, cfg.deadzone),
        ly = analog(state.Gamepad.sThumbLY, cfg.deadzone);
  if (lx == 0 && ly == 0)
    stickArmed = true;
  auto held = gate.held(buttons);
  bool confirm =
           (pressed & (settings ? XINPUT_GAMEPAD_A : binding(Confirm))) != 0,
       back = (pressed & (settings ? XINPUT_GAMEPAD_B : binding(Back))) != 0;
  // A D-pad direction assigned to an action must not move focus as well.
  const auto reservedDirections = settings ? 0u
      : binding(Confirm) | binding(Back) | binding(Menu) | binding(Keyboard) |
            (keyboard ? binding(Secondary)
             : binding(CursorMode) | (cursorMode ? binding(Secondary)
                                               : binding(PreviousTab) | binding(NextTab)));
  const auto navButtons = held & ~reservedDirections;
  bool directions[] = {
      bool(navButtons & XINPUT_GAMEPAD_DPAD_UP) || (stickArmed && ly > .55f),
      bool(navButtons & XINPUT_GAMEPAD_DPAD_DOWN) || (stickArmed && ly < -.55f),
      bool(navButtons & XINPUT_GAMEPAD_DPAD_LEFT) || (stickArmed && lx < -.55f),
      bool(navButtons & XINPUT_GAMEPAD_DPAD_RIGHT) || (stickArmed && lx > .55f)};
  bool nav[4]{};
  for (int i = 0; i < 4; ++i)
    nav[i] = repeat[i].update(directions[i] && foreground && connected, time,
                              focusedList().kind != ListKind::None &&
                                  !cursorMode && !settings && !keyboard && i < 2);
  // Execute after the diagnostic chord is released. Otherwise generated Esc
  // would inherit Ctrl+Shift and invoke an OS shortcut instead of LFS Back.
  unsigned diagnostic = (GetAsyncKeyState(VK_CONTROL) & 0x8000) ||
                                (GetAsyncKeyState(VK_SHIFT) & 0x8000) ||
                                (GetAsyncKeyState(VK_MENU) & 0x8000)
                            ? 0
                            : diagnosticKeys.exchange(0);
  if (!cfg.diagnostics || !foreground)
    diagnostic = 0;
  const bool lostAssignmentFocus = assignmentFocusLost.exchange(false);
  static bool pedalsArmed = false;
  const bool pedalSource = foreground && connected && singlePad;
  if (!pedalSource || lostAssignmentFocus || activeSlot != oldSlot)
    pedalsArmed = false;
  if (pedalSource && !state.Gamepad.bLeftTrigger && !state.Gamepad.bRightTrigger)
    pedalsArmed = true;
  pedalSlot.store(pedalSource && pedalsArmed && !native ? int(activeSlot) : -1);
  const auto pedals = (std::uint64_t(state.Gamepad.bRightTrigger) << 40) |
                      (std::uint64_t(state.Gamepad.bLeftTrigger) << 32) |
                      nativeVidPid;
  nativePedals.store(pedalSource && pedalsArmed && nativeVidPid ? pedals : 0);
  const bool diagnosticAssign = (diagnostic & (1u << 16)) != 0;
  if (diagnostic)
    log("Diagnostic action mask=%03x", diagnostic);
  for (int i = 0; i < 4; ++i)
    nav[i] |= (diagnostic & (1u << i)) != 0;
  bool diagnosticNext = (diagnostic & (1 << 5)) != 0,
       diagnosticConfirm = (diagnostic & (1 << 6)) != 0,
       diagnosticKeyboard = (diagnostic & (1 << 7)) != 0,
       diagnosticSettings = (diagnostic & (1 << 4)) != 0;
  confirm |= diagnosticConfirm;
  back |= (diagnostic & (1 << 8)) != 0;
  const bool menuPressed =
      (pressed & (settings ? XINPUT_GAMEPAD_START : binding(Menu))) ||
      (diagnostic & (1 << 9));
  const bool closingSettingsWithStart = settings && menuPressed;
  static SettingsTriggerChord settingsChord;
  static double diagnosticTriggersUntil = 0;
  const bool settingsChordAllowed = menu && foreground && !contextChanged &&
      !lostAssignmentFocus && (connected || cfg.diagnostics) && !nativeText && !keyboard &&
      !garageViewer && !adjustingSlider && !cursorDragging && !bindingCapture.active() &&
      !nativeButtonAssignment && (!pedalCalibration || settings);
  if (!settingsChordAllowed)
    diagnosticTriggersUntil = 0;
  else if (diagnosticSettings)
    diagnosticTriggersUntil = time + SettingsTriggerChord::duration + .18;
  const auto &pad = state.Gamepad;
  const bool diagnosticTriggers = time < diagnosticTriggersUntil;
  const bool toggleSettings = settingsChord.update(
      diagnosticTriggers ? 255 : pad.bLeftTrigger,
      diagnosticTriggers ? 255 : pad.bRightTrigger, time, settingsChordAllowed);
  const auto shortcutScreen = menu && !nativeText ? findShortcutScreen(modal, raceMenu)
                                             : ShortcutScreen::None;
  const auto menuShortcuts = findMenuShortcuts(shortcutScreen, modal);
  const bool hasMenuShortcuts =
      std::any_of(menuShortcuts.begin(), menuShortcuts.end(),
                  [](const ResolvedShortcut &shortcut) { return shortcut.label != nullptr; });
  const bool canUseMenuShortcuts = foreground && (connected || cfg.diagnostics) &&
                                  hasMenuShortcuts && !settings && !keyboard &&
                                  !cursorMode && !adjustingSlider && !menuPressed &&
                                  !toggleSettings && !diagnosticKeyboard &&
                                  !(pressed & (binding(CursorMode) | binding(Keyboard)));
  static bool diagnosticMenuShortcuts = false;
  if (!canUseMenuShortcuts || reset || back)
    diagnosticMenuShortcuts = false;
  else if (diagnostic & (1u << 18))
    diagnosticMenuShortcuts = !diagnosticMenuShortcuts;
  const bool menuShortcutsOpen = canUseMenuShortcuts &&
      ((held & binding(SettingsModifier)) || diagnosticMenuShortcuts);
  static ShortcutInputDrain shortcutInput;
  shortcutInput.update(shortcutEditorWasOpen, menuShortcutsOpen, lx != 0.f || ly != 0.f,
                       buttons & (XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_B |
                                  XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_START));
  bool finishHint = false;
  const auto session = raceSession(sim.known, sim.flags, replay.active,
                                   replay.confirmedAbsent);
  const bool onTrack = sim.known && sim.mode == 0 &&
                       session != RaceSession::Outside && (sim.flags & 1);
  const bool canTogglePause = onTrack && foreground && !menu &&
                             !(sim.flags & 8) && !menuPressed && !back;
  static PadRecovery recovery;
  static bool testDisconnect = false, simulateDisconnected = false;
  bool simulatedLoss = false;
  if (!cfg.diagnostics || session == RaceSession::Outside) {
    testDisconnect = simulateDisconnected = false;
  } else if (onTrack && !menu && diagnosticNext) {
    testDisconnect = true;
    simulateDisconnected = !simulateDisconnected;
    simulatedLoss = simulateDisconnected;
    log("Diagnostic controller disconnect=%d", simulateDisconnected);
  }
  const bool recoveryConnected = testDisconnect ? !simulateDisconnected : connected;
  const bool recoveryLost = onTrack && (testDisconnect ? simulatedLoss : lostPad);
  const auto previousRecovery = recovery.phase;
  const auto action = recovery.update(session, recoveryLost, recoveryConnected,
                                      (sim.flags & 4) != 0, canTogglePause,
                                      confirm, now);
  if (action != RecoveryAction::None) {
    // P is an LFS pause toggle. Request it once and wait for the game to
    // report the resulting state before accepting another transition.
    if (!simCommand("/press p"))
      recovery.phase = action == RecoveryAction::Pause
                           ? RecoveryPhase::Warning : RecoveryPhase::Paused;
  }
  if (previousRecovery != recovery.phase)
    log("Controller recovery: phase %u -> %u (session=%u connected=%d)",
        unsigned(previousRecovery), unsigned(recovery.phase), unsigned(session),
        recoveryConnected);
  // Include the previous phase on the frame that resumes driving so the
  // confirmation button cannot also activate a regular menu or race action.
  static bool wasRecoveryCapture = false;
  const bool recoveryCapture = session == RaceSession::Offline && recovery.blocksPad();
  const bool recoveryConsumesConfirm = !menu && (recoveryCapture || wasRecoveryCapture);
  static bool triggerArmed = false;
  if (contextChanged || !foreground || !playingReplay)
    triggerArmed = false;
  if (foreground && playingReplay) {
    if ((confirm || back) && !menuPressed && !replay.seeking) {
      if (back)
        simKey("esc");
      else if (simCommand("/press space"))
        log("Replay play/pause");
    }
    const auto &pad = state.Gamepad;
    if (pad.bLeftTrigger <= 30 && pad.bRightTrigger <= 30)
      triggerArmed = true;
    int seek = triggerDirection(pad.bLeftTrigger, pad.bRightTrigger);
    if (diagnostic & (1 << 12))
      seek = 1;
    if (diagnostic & (1 << 13))
      seek = -1;
    if (triggerArmed && seek) {
      triggerArmed = false;
      if (!confirm && !back && !menuPressed)
        seekReplay(seek);
    }
  }

  // Keep the steering stick, throttle/brake axis and other driving buttons
  // available while the quick menu is open.
  static double quickHeldAt = 0;
  static bool quickOpen = false, quickStickArmed = false, quickDrain = false;
  static bool diagnosticQuick = false;
  static QuickAction quickAction = QuickAction::None;
  const bool quickAllowed = foreground && !menu && !playingReplay && !recoveryConsumesConfirm &&
      sim.known && !replay.active && (sim.flags & 1) &&
      !(sim.flags & (8 | 16 | 256 | 32768)) && sim.mode == 0 && cfg.quickButton &&
      (!native || knownPlayStationButtonLayout(nativeVidPid)) &&
      ((connected && singlePad) || cfg.diagnostics);
  const auto quickMask = quickMasks[cfg.quickButton];
  const float quickX = analog(state.Gamepad.sThumbRX, cfg.deadzone);
  const float quickY = analog(state.Gamepad.sThumbRY, cfg.deadzone);
  if (!quickAllowed || contextChanged || menuPressed) {
    quickHeldAt = 0;
    quickOpen = diagnosticQuick = quickStickArmed = false;
  } else if (diagnostic & (1 << 14)) {
    quickOpen = diagnosticQuick = !diagnosticQuick;
    quickAction = QuickAction::None;
    quickStickArmed = true;
  } else if (diagnosticQuick) {
    if (nav[0]) quickAction = QuickAction::Camera;
    if (nav[2]) quickAction = QuickAction::Mirror;
    if (nav[3]) quickAction = QuickAction::Pit;
    if (confirm || back) {
      if (confirm && !back)
        applyQuickAction(quickAction);
      quickOpen = diagnosticQuick = false;
    }
  } else if ((quickHeldAt != 0 ? buttons : held) & quickMask) {
    if (quickHeldAt == 0)
      quickHeldAt = time;
    if (!quickOpen && time - quickHeldAt >= .28) {
      quickOpen = true;
      quickDrain = connected;
      quickAction = QuickAction::None;
    }
    if (quickX == 0 && quickY == 0)
      quickStickArmed = true;
    if (quickOpen && quickStickArmed)
      quickAction = quickDirection(quickX, quickY);
  } else {
    if (quickOpen)
      applyQuickAction(quickAction);
    quickHeldAt = 0;
    quickOpen = quickStickArmed = false;
  }
  if (!foreground || !connected || oldSlot != activeSlot ||
      (!quickOpen && quickX == 0 && quickY == 0))
    quickDrain = false;
  static GarageViewInput garageViewInput;
  if (!connected || oldSlot != activeSlot)
    garageViewInput = {};
  const bool viewerReady = garageViewer && foreground && connected && singlePad &&
                           !settings && !keyboard && !cursorMode;
  const auto viewerMask = garageViewInput.update(
      viewerReady, contextChanged || lostAssignmentFocus,
      quickX != 0 || quickY != 0, pad.bLeftTrigger > 30 || pad.bRightTrigger > 30);
  constexpr unsigned quickButtonBits[]{0, 1u << 6, 1u << 8, 1u << 9};
  const auto quickFilter = (quickAllowed ? quickButtonBits[cfg.quickButton] : 0u) |
                           (quickDrain ? rightStickMask : 0u) |
                           (settingsChord.draining ? triggerAxisMask : 0u);
  captureQuickInput.store(connected && singlePad && foreground
                             ? quickFilter | viewerMask |
                                   (shortcutInput.stickHeld ? leftStickMask : 0u)
                             : 0u);
  static double diagnosticOrbitUntil = 0;
  static double rumblePreviewUntil = 0;
  static ControllerId rumblePreviewSlot = -1;
  if (!viewerReady || contextChanged || lostAssignmentFocus)
    diagnosticOrbitUntil = 0;
  else if (diagnostic & (1u << 17))
    diagnosticOrbitUntil = time + .40;
  const bool assigningDrivingButton = foreground && nativeButtonAssignment && !settings;
  static NativeButtonAssignmentGate drivingAssignment;
  static double drivingTestUntil = 0;
  static unsigned drivingTestPov = 0xffffffff;
  const bool drivingAssignmentArmed = drivingAssignment.update(assigningDrivingButton, reset, buttons);
  if (drivingAssignmentArmed && singlePad && (diagnostic & dpad)) {
    const unsigned angles[]{0, 18000, 27000, 9000};
    for (int i = 0; i < 4; ++i)
      if (diagnostic & (1u << i)) {
        drivingTestPov = angles[i];
        drivingTestUntil = time + .15;
        break;
      }
  }
  diagnosticDrivingPov.store(assigningDrivingButton && time < drivingTestUntil
                                 ? drivingTestPov : 0xffffffff);
  if (!assigningDrivingButton)
    drivingTestUntil = 0;
  if (assigningDrivingButton) {
    // LFS is listening for a driving button. Do not interpret that same
    // press as a menu action, and wait for the opening press to clear.
    blockMenuInput();
  } else if (bindingCapture.active()) {
    if (!settings || !foreground || lostAssignmentFocus || !connected ||
        activeSlot != oldSlot || (pad.bLeftTrigger > 160 && pad.bRightTrigger > 160) ||
        diagnosticSettings) {
      bindingCapture.cancel();
    } else {
      const auto candidate = std::uint16_t(
          buttons | (diagnosticConfirm ? XINPUT_GAMEPAD_A : 0) |
          ((diagnostic & (1u << 8)) ? XINPUT_GAMEPAD_B : 0) |
          ((diagnostic & (1u << 9)) ? XINPUT_GAMEPAD_START : 0) |
          (diagnosticAssign ? XINPUT_GAMEPAD_X : 0) | (diagnostic & dpad));
      const int button = bindingCapture.update(candidate, physical);
      if (button >= 0) {
        rebind(cfg.bindings, settingRow - firstBindingSetting, button);
        if (cfg.diagnostics)
          log("Assigned physical button %d to action %d", button, settingRow - firstBindingSetting);
      }
    }
    if (!bindingCapture.active())
      blockMenuInput();
  } else if (foreground && !recoveryConsumesConfirm) {
    if (toggleSettings) {
      if (settings)
        loadConfig();
      settings = !settings;
      blockMenuInput();
      if (cfg.diagnostics)
        log("Settings trigger chord: %s", settings ? "open" : "closed");
    } else if (menuPressed) {
      if (keyboard) {
        simKey("enter");
        keyboard = manualKeyboard = false;
      } else if (settings) {
        loadConfig();
        settings = false;
        blockMenuInput();
        confirm = back = false;
        std::fill(std::begin(nav), std::end(nav), false);
      } else
        simKey("esc");
    }
    if (menu && !toggleSettings && ((pressed & binding(Keyboard)) || diagnosticKeyboard) &&
        !settings) {
      if (keyboard && !diagnosticKeyboard)
        upper = !upper;
      else {
        keyboard = !keyboard;
        manualKeyboard = keyboard;
        gate.blocked = 0xffff;
      }
    }
    if (nativeText && !manualKeyboard && !settings)
      keyboard = true;
    if (!nativeText && !manualKeyboard)
      keyboard = false;
    if (!menu) {
      settings = keyboard = manualKeyboard = false;
      cursorMode = false;
    }
    if (menu && !toggleSettings && !closingSettingsWithStart) {
      if (keyboard) {
        auto rows = keyboardRows(cfg.language, upper, symbols);
        if (nav[0])
          keyRow = (keyRow + 4) % 5;
        if (nav[1])
          keyRow = (keyRow + 1) % 5;
        int cols = keyRow == 4 ? 6 : int(rows[keyRow].size());
        keyCol = std::min(keyCol, cols - 1);
        if (nav[2])
          keyCol = (keyCol + cols - 1) % cols;
        if (nav[3] || diagnosticNext)
          keyCol = (keyCol + 1) % cols;
        if (confirm)
          keyAction();
        if (pressed & binding(Secondary))
          localKey(VK_BACK);
        if (back) {
          keyboard = manualKeyboard = false;
          simKey("esc");
        }
      } else if (settings && shortcutEditor) {
        if (back || confirm) {
          if (!back) {
            cfg.shortcuts = shortcutEditDraft;
            settingRow = 0;
            vibrationPreviewResult = ERROR_CANCELLED;
          }
          shortcutEditor = false;
          blockMenuInput();
        } else if ((pressed & XINPUT_GAMEPAD_X) || diagnosticAssign) {
          directionsBeforeEdit = shortcutEditDraft[shortcutEditMenu] =
              defaultShortcutDirections;
        } else {
          if (nav[0] != (nav[1] || diagnosticNext)) {
            shortcutEditRow = (shortcutEditRow + (nav[0] ? 4 : 1)) % 5;
            directionsBeforeEdit = shortcutEditDraft[shortcutEditMenu];
          }
          if (nav[2] != nav[3])
            adjustShortcut(nav[2] ? -1 : 1);
        }
      } else if (settings) {
        if (nav[0])
          settingRow = (settingRow + settingCount - 1) % settingCount;
        if (nav[1] || diagnosticNext)
          settingRow = (settingRow + 1) % settingCount;
        if (nav[2])
          adjustSetting(-1);
        if (nav[3])
          adjustSetting(1);
        if (back) {
          loadConfig();
          settings = false;
          blockMenuInput();
        } else if (settingRow == shortcutSetting && confirm) {
          shortcutEditDraft = cfg.shortcuts;
          shortcutEditMenu = shortcutScreen == ShortcutScreen::None ? 0 : int(shortcutScreen);
          shortcutEditRow = 0;
          shortcutEditor = true;
          blockMenuInput();
        } else if (isBindingSetting() &&
                   ((pressed & XINPUT_GAMEPAD_X) || diagnosticAssign)) {
          bindingCapture.begin();
        } else if (settingRow == rumbleSetting &&
                   ((pressed & XINPUT_GAMEPAD_X) || diagnosticAssign)) {
          const auto slot = cfg.slot >= 0 ? controllerForPreference(cfg.slot, activeSlot) : activeSlot;
          XINPUT_STATE candidate{};
          missingSlot = readController(slot, candidate) != ERROR_SUCCESS;
          vibrationPreviewResult = ERROR_CANCELLED;
          if (!missingSlot && cfg.rumbleStrength) {
            rumblePreviewSlot = slot;
            rumblePreviewUntil = time + .42;
          }
        } else if (confirm) {
          XINPUT_STATE candidate{};
          missingSlot = cfg.slot >= 0 &&
                        readController(controllerForPreference(cfg.slot, activeSlot),
                                       candidate) != ERROR_SUCCESS;
          if (!missingSlot) {
            settingsSaveFailed = !saveConfig();
            if (!settingsSaveFailed) {
              inputSlot = cfg.slot;
              activeSlot = -1;
              settings = false;
              blockMenuInput();
            }
          }
        }
      } else if (menuShortcutsOpen) {
        const auto modifier = binding(SettingsModifier);
        auto directionalPress = pressed & dpad & ~modifier;
        auto directionalHeld = buttons & dpad & ~modifier;
        if (diagnosticMenuShortcuts) {
          directionalPress |= diagnostic & dpad;
          directionalHeld |= diagnostic & dpad;
        }
        if (modifier & dpad) {
          // A direction used as the modifier cannot also be pressed in the
          // chord. The left stick still reaches all four shortcut targets.
          const bool stickDirections[]{ly > .55f, ly < -.55f, lx < -.55f, lx > .55f};
          for (int i = 0; stickArmed && i < 4; ++i)
            if (stickDirections[i]) {
              directionalPress |= physical[PadUp + i];
              directionalHeld |= physical[PadUp + i];
            }
        }
        const int direction = menuShortcutDirection(directionalHeld, directionalPress);
        if (direction >= 0 && menuShortcuts[direction].item >= 0) {
          selected = menuShortcuts[direction].item;
          scrollTarget = -1;
          restoreAfterScroll = false;
        }
        // A direction or Confirm held at the end of the chord must not
        // navigate or activate a second target when the modifier is released.
        gate.blocked |= buttons & (dpad | binding(Confirm));
        stickArmed = false;
        for (auto &directionRepeat : repeat)
          directionRepeat = {};
      } else {
        const bool cursorChanged = (pressed & binding(CursorMode)) != 0;
        if (cursorChanged) {
          cursorMode = !cursorMode;
          scrollTarget = -1;
          restoreAfterScroll = false;
          releaseMouse();
        }
        const bool viewerMotion = viewerReady && garageViewInput.armed &&
                                  !cursorChanged && !back && !menuPressed &&
                                  !adjustingSlider && !confirm && !nav[0] &&
                                  !nav[1] && !nav[2] && !nav[3];
        if (viewerMotion)
          moveGarageViewer(time < diagnosticOrbitUntil ? .65f : quickX, quickY,
                           (diagnostic & (1u << 12)) ? 1
                           : (diagnostic & (1u << 13)) ? -1
                           : triggerDirection(pad.bLeftTrigger, pad.bRightTrigger),
                           width, height, dt, time);
        else if (viewerDrag)
          releaseMouse();
        auto finishConfirmed = [&](bool onFinish) {
          finishHint = onFinish;
          if (!onFinish || reset || back || menuPressed || cursorChanged ||
              mouseDown || diagnosticNext) {
            cancelFinishHold();
            return false;
          }
          // Test the held path through the same timer as the XInput button.
          const bool testPress = (diagnostic & (1u << 15)) != 0;
          if (testPress)
            diagnosticFinishUntil = time + HoldConfirm::duration + .20;
          const bool completed = finishHold.update(
              confirm || testPress,
              (held & binding(Confirm)) || time < diagnosticFinishUntil,
              true, time);
          if (completed)
            diagnosticFinishUntil = 0;
          return completed;
        };
        if (cursorMode) {
          float rx = analog(state.Gamepad.sThumbRX, cfg.deadzone),
                ry = analog(state.Gamepad.sThumbRY, cfg.deadzone);
          POINT p;
          GetCursorPos(&p);
          ScreenToClient(window, &p);
          p.x =
              LONG(std::clamp(p.x + rx * cfg.cursorSpeed * dt, 0.f, width - 1));
          p.y = LONG(
              std::clamp(p.y - ry * cfg.cursorSpeed * dt, 0.f, height - 1));
          auto d = ImGui::GetForegroundDrawList();
          d->AddCircle({float(p.x), float(p.y)}, 8, IM_COL32(70, 255, 205, 255),
                       0, 2);
          if (rx || ry) {
            POINT screen = p;
            ClientToScreen(window, &screen);
            SetCursorPos(screen.x, screen.y);
          }
          bool onFinish = false;
          if (raceFinishItem >= 0) {
            const auto &r = items[raceFinishItem].rect;
            const auto a = pixel(r.l, r.t), b = pixel(r.r, r.b);
            onFinish = p.x >= a.x && p.x < b.x && p.y >= a.y && p.y < b.y;
          }
          if (finishConfirmed(onFinish))
            activate(items[raceFinishItem], width, height);
          else if (confirm && !onFinish && !mouseDown)
            clickAt(float(p.x), float(p.y), false, !raceMenu);
          if (cursorDragging && !(buttons & binding(Confirm)))
            releaseMouse();
          if ((pressed & binding(Secondary)) && !onFinish)
            clickAt(float(p.x), float(p.y), true);
          if (nav[0])
            wheel(WHEEL_DELTA);
          if (nav[1])
            wheel(-WHEEL_DELTA);
        } else {
          const bool scrolling = scrollTarget >= 0;
          if (scrolling)
            updateListScroll(width, height, time);
          if (adjustingSlider) {
            if (colourDragging && time >= colourDragTime &&
                GetForegroundWindow() == window && !confirm && !back) {
              if (colourMoved) {
                releaseMouse();
              } else {
                SetCursorPos(colourDestination.x, colourDestination.y);
                colourMoved = true;
                colourDragTime = time + .025;
              }
            }
            for (int i = 2; i < 4; ++i)
              if (nav[i] && !mouseDown && selected >= 0) {
                if (isGarageColourSlider(items[selected])) {
                  adjustGarageColour(items[selected], i == 2 ? -1 : 1,
                                     width, height);
                  continue;
                }
                int arrow = sliderArrow(items, selected, i == 2 ? -1 : 1);
                if (arrow >= 0) {
                  auto r = items[arrow].rect;
                  clickAt((r.x() + 1) * width * .5f,
                          (r.y() + 1) * height * .5f);
                }
              }
            if (confirm || back || nav[0] || nav[1]) {
              adjustingSlider = editingGarageColour = false;
              confirm = back = false;
            }
          } else if (!scrolling) {
            const int oldSelection = selected;
            for (int i = 0; i < 4; ++i)
              if (nav[i] && !moveList(Direction(i)))
                selected = neighbour(items, selected, (Direction)i);
            const int slot = listSlot(selected);
            if (slot >= 0 && (selected != oldSelection || confirm))
              rememberList(slot);
            const bool onFinish = raceFinishItem >= 0 && selected == raceFinishItem;
            if (finishConfirmed(onFinish))
              activate(items[raceFinishItem], width, height);
            else if (confirm && !onFinish && selected >= 0 && !mouseDown &&
                     scrollTarget < 0) {
              editingGarageColour = isGarageColourSlider(items[selected]);
              if (items[selected].nativeType == 40 || editingGarageColour)
                adjustingSlider = true;
              else
                activate(items[selected], width, height);
            }
          }
          if (diagnosticNext && !items.empty() && scrollTarget < 0)
            selected = (selected + 1) % int(items.size());
        }
        if (back)
          simKey("esc");
        for (int d : {-1, 1})
          if ((pressed & binding(d < 0 ? PreviousTab : NextTab)) ||
              (diagnostic & (1u << (d < 0 ? 10 : 11)))) {
            int tab = nextTab(items, d);
            if (tab >= 0) {
              selected = tab;
              activate(items[tab], width, height);
            } else if (inList && !cursorMode && !back) {
              if (scrollTarget >= 0 || mouseDown)
                continue;
              const int slot = std::max(0, listSlot(selected));
              const int target = pageOffset(list.first, list.visible, list.total, d);
              if (target != list.first)
                beginListScroll(target, slot, list.kind);
              else {
                const int edge = d < 0 ? 0 : int(list.entries.size()) - 1;
                selected = list.entries[edge].item;
                rememberList(edge);
              }
            } else
              localKey(d < 0 ? VK_PRIOR : VK_NEXT);
          }
      }
    }
  } else
    releaseMouse();
  if (colourDragging && (settings || keyboard || cursorMode || !adjustingSlider))
    releaseMouse();
  if (mainMenuInitial >= 0 && selected >= 0 && selected < int(items.size())) {
    const auto *native = reinterpret_cast<const NativeItem *>(imageBase + 0x28ca20);
    const auto &entry = native[items[selected].id];
    if (entry.owner == modal && entry.type == 9 &&
        entry.callback == uintptr_t(imageBase) + 0x84750)
      mainMenuSelection = entry.value;
  }
  if (garageRoot && selected >= 0 && selected < int(items.size())) {
    const auto *native = reinterpret_cast<const NativeItem *>(imageBase + 0x28ca20);
    const auto &n = native[items[selected].id];
    garageReturnCallback = n.callback;
    garageReturnValue = n.value;
    garageReturnType = n.type;
    if (garageSkinName && garageSkinPrefix && (layoutChanged || !wasGarageRoot ||
                                              confirm || lastGarageSkin.empty())) {
      const auto prefix = nativeLabel(garageSkinPrefix), name = nativeLabel(garageSkinName);
      lastGarageSkin = prefix.empty() || name.empty() ? "" : prefix + "_" + name;
    }
  } else if (!menu) {
    garageReturnCallback = 0;
    lastGarageSkin.clear();
  }
  if (!finishHint)
    cancelFinishHold();
  // Continue suppressing a held menu button until release after returning to
  // driving.
  auto consumed = dpad;
  for (int b : cfg.bindings)
    consumed |= physical[b];
  consumed |= shortcutInput.buttonsHeld;
  static bool draining = false;
  if ((wasMenu || wasReplay || wasRecoveryCapture) &&
      !menu && !playingReplay && !recoveryCapture)
    draining = true;
  if (!(buttons & consumed))
    draining = false;
  captureButtons.store(foreground && connected && !drivingAssignmentArmed &&
                       (menu || playingReplay || recoveryCapture || draining));
  captureRaceAxes.store(foreground && connected && (raceMenu || recoveryCapture));
  const bool settingsVisible = foreground && menu && settings;
  static DrivingRumble rumble;
  static bool hadTelemetry = false;
  static bool hadWheelTelemetry = false;
  static ControllerId lastRumbleController = -1;
  const bool canRumbleWhileDriving = foreground && onTrack && !menu && !settings && !keyboard && !nativeText && !playingReplay &&
      !recoveryConsumesConfirm && !menuPressed && !lostAssignmentFocus &&
      !(sim.flags & (2 | 4 | 8 | 16 | 256 | 32768)) &&
      sim.localPlayer && sim.localPlayer == sim.viewedPlayer && cfg.rumbleStrength;
  ControllerId rumbleController = -1;
  if (canRumbleWhileDriving) {
    if (connected)
      rumbleController = inputSlot < 0 && padCount > 1
          ? automaticRumbleController(activeSlot, lastRumbleController, cfg.deadzone)
          : activeSlot;
    else if (inputSlot < 0 && padCount == 0)
      rumbleController = soleUnmappedController();
    lastRumbleController = rumbleController;
  }
  const bool rumbleDriving = rumbleController >= 0;
  const auto inSimTelemetry = pollInSimRumble(rumbleDriving);
  if (rumbleDriving) {
    const auto outSimTelemetry = drivingTelemetry();
    const auto tick = GetTickCount64();
    const auto telemetry = selectRumbleTelemetry(outSimTelemetry, inSimTelemetry, tick);
    const bool fresh = telemetry.fresh(tick);
    const bool wheelTelemetry = telemetry.sample.hasWheels;
    if (cfg.diagnostics && fresh && (!hadTelemetry || wheelTelemetry != hadWheelTelemetry))
      log("Rumble telemetry: %s local=%u viewed=%u engine=%.1f gear=%u",
          wheelTelemetry ? "OutSim" : "InSim", sim.localPlayer, sim.viewedPlayer,
          telemetry.sample.engineSpeed, telemetry.sample.gear);
    hadTelemetry = fresh;
    hadWheelTelemetry = wheelTelemetry;
    driveMotors(rumbleController, rumble.update(telemetry.sample, telemetry.received, tick,
                                         cfg.rumbleStrength), window, cfg.diagnostics);
  } else {
    rumble = {};
    hadTelemetry = false;
    const bool preview = foreground && !lostAssignmentFocus && settings &&
        settingRow == rumbleSetting && cfg.rumbleStrength &&
        rumblePreviewSlot >= 0 && time < rumblePreviewUntil;
    XINPUT_STATE candidate{};
    if (preview && readController(rumblePreviewSlot, candidate) == ERROR_SUCCESS) {
      const bool secondPulse = rumblePreviewUntil - time <= .21;
      const auto result = driveMotors(rumblePreviewSlot,
          secondPulse ? motorLevels(0, 1, cfg.rumbleStrength)
                      : motorLevels(1, 0, cfg.rumbleStrength),
          window, cfg.diagnostics);
      if (result != ERROR_BUSY)
        vibrationPreviewResult = result;
    } else {
      rumblePreviewUntil = 0;
      stopMotors();
    }
  }
  const bool meterFits = width >= meterColumn * 2 + 4 * meterMargin &&
                         height >= meterHeight * 2;
  const bool showMeter = cfg.inputMonitor && foreground && !keyboard && meterFits &&
      ((!settings && (raceMenu || (onTrack && !menu))) ||
       (settingsVisible && width > settingsWidth + 2 * (meterColumn + 2 * meterMargin)));
  const float settingsW = std::min(width - 30, settingsWidth);
  const float settingsX = (width - settingsW) * .5f;
  const float settingsY = std::max(meterMargin, (height - settingsHeight) * .5f);
  const bool collectInput = showMeter && connected && singlePad;
  static ULONGLONG meterSince = 0;
  if (!monitorInput.exchange(collectInput) && collectInput)
    meterSince = GetTickCount64();
  if (foreground && menu) {
    if (settings && shortcutEditor)
      drawShortcutEditor(settingsX, settingsY, settingsW);
    else if (settings)
      drawSettings(settingsX, settingsY, settingsW);
    else if (keyboard)
      drawKeyboard(width, height);
    else {
      if (!assigningDrivingButton && !cursorMode && selected >= 0) {
        auto r = items[selected].rect;
        ImGui::GetForegroundDrawList()->AddRect(
            pixel(r.l, r.t), pixel(r.r, r.b), IM_COL32(65, 255, 196, 255), 3, 0,
            3);
      }
      if (menuShortcutsOpen && !assigningDrivingButton)
        drawMenuShortcuts(width, height, menuShortcuts, selected);
      else if (assigningDrivingButton) {
        drawCornerHints(width, height, false, {
            {PadDirections, text("Atamak için bir tuşa ya da yöne bas", "Press a button or direction to assign")}});
        drawCornerHints(width, height, true, {{-1, text("Esc · İptal", "Esc · Cancel")}});
      } else if (!connected && !cfg.diagnostics) {
        drawCornerHints(width, height, false,
                        {{-1, text("Kontrolcü bekleniyor", "Waiting for controller")}});
      } else if (cfg.controllerIcons && garageViewer && !cursorMode) {
        drawCornerHints(width, height, false, {
            {cfg.bindings[Back], text("Garaja dön", "Back to garage")},
            {cfg.bindings[CursorMode], text("İmleç", "Cursor")}});
        if (singlePad)
          drawCornerHints(width, height, true, {
              {PadRightStick, text("Döndür", "Rotate")},
              {PadLT, text("Yakınlaştır", "Zoom"), PadRT}});
      } else if (cfg.controllerIcons && adjustingSlider) {
        drawCornerHints(width, height, false, {{PadLeft, text("Değiştir", "Adjust"), PadRight}});
        drawCornerHints(width, height, true, {
            {cfg.bindings[Confirm], text("Bitir", "Finish")},
            {cfg.bindings[Back], text("Geri", "Back")}});
      } else if (cfg.controllerIcons) {
        drawCornerHints(width, height, false, {
            {cfg.bindings[Confirm], cursorMode ? text("Tıkla", "Click") : text("Seç", "Select")},
            {cfg.bindings[Back], text("Geri", "Back")},
            {cfg.bindings[CursorMode], cursorMode ? text("Seçim", "Selection") : text("İmleç", "Cursor")}});
        const int modifier = cfg.bindings[SettingsModifier];
        const HudHint settingsHint{PadLT, text("Ayarlar için tut", "Hold for settings"), PadRT};
        if (hasMenuShortcuts && !cursorMode)
          drawCornerHints(width, height, true, {
              {modifier, text("Kısayollar", "Shortcuts"), modifier >= PadUp ? PadLeftStick : PadDirections},
              {cfg.bindings[Keyboard], text("Klavye", "Keyboard")}, settingsHint});
        else if (inList && !cursorMode)
          drawCornerHints(width, height, true, {
              {cfg.bindings[PreviousTab], text("Sayfa", "Page"), cfg.bindings[NextTab]},
              {cfg.bindings[Keyboard], text("Klavye", "Keyboard")}, settingsHint});
        else if (pedalCalibration)
          drawCornerHints(width, height, true, {{cfg.bindings[Keyboard], text("Klavye", "Keyboard")}});
        else
          drawCornerHints(width, height, true, {
              {cfg.bindings[Keyboard], text("Klavye", "Keyboard")}, settingsHint});
      }
      if (finishHint && (connected || cfg.diagnostics))
        drawFinishHold(items[raceFinishItem], width, time);

    }
  }
  if (foreground && playingReplay && replay.active && (connected || cfg.diagnostics))
    drawReplay(width, height, replay);
  if (showMeter) {
    const float y = std::max(meterMargin, height - meterHeight - (menu ? 110.f : 38.f));
    drawInputMeter(width, y, state.Gamepad, connected, activeSlot, singlePad,
                   collectInput ? drivingInputSample(meterSince) : DrivingInputSample{});
  }
  if (foreground && onTrack && recovery.phase != RecoveryPhase::Idle)
    drawRecovery(width, height, recovery.phase, session, recoveryConnected, canTogglePause);
  if (quickOpen)
    drawQuickMenu(width, height, quickAction);
  if (foreground && !sim.known) {
    char hint[64];
    std::snprintf(hint, sizeof(hint), text("LfsPad InSim bağlantısı bekliyor (%u)", "LfsPad waiting for InSim (%u)"),
                  static_cast<unsigned>(inSimPort()));
    label(20, 20, hint, IM_COL32(255, 210, 90, 255));
  }
  if (cfg.diagnostics && ticks++ % 300 == 0)
    log("Frame: flags=%u mode=%u menu=%d raceMenu=%d text=%d items=%zu "
        "selection=%d rumble=%d local=%u viewed=%u",
        sim.flags, sim.mode, menu, raceMenu, nativeText, items.size(), selected,
        rumbleDriving, sim.localPlayer, sim.viewedPlayer);
  wasMenu = menu;
  wasRaceMenu = raceMenu;
  wasRecoveryCapture = recoveryCapture;
  wasReplay = playingReplay;
  previousList = list.kind;
  wasForeground = foreground;
  wasConnected = connected;
  ImGui::Render();
  ID3D11RenderTargetView *old = nullptr;
  ID3D11DepthStencilView *oldDepth = nullptr;
  context->OMGetRenderTargets(1, &old, &oldDepth);
  context->OMSetRenderTargets(1, &target, nullptr);
  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
  context->OMSetRenderTargets(1, &old, oldDepth);
  if (old)
    old->Release();
  if (oldDepth)
    oldDepth->Release();
}
} // namespace pad
