#include "runtime.hpp"
#include "core.hpp"
#include "controller_axes.hpp"
#include "shortcuts.hpp"
#include "controller_check.hpp"
#include "player_profiles.hpp"
#include "steering_help.hpp"
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
enum class OverlayClick {
  None, OptionsTab, UpdateAccept, UpdateLater, KeyboardFirst,
  SettingsTabFirst = 64, SettingsRowFirst = 69
};
struct OverlayTarget { RECT rect{}; OverlayClick action{}; };
static std::array<OverlayTarget, 73> overlayTargets;
static OverlayClick overlayClick = OverlayClick::None, overlayPress = OverlayClick::None;
static RECT overlayShield{};
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
  if (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP) {
    const POINT p{short(LOWORD(l)), short(HIWORD(l))};
    if (message == WM_LBUTTONUP && overlayPress != OverlayClick::None) {
      for (const auto &target : overlayTargets)
        if (target.action == overlayPress && PtInRect(&target.rect, p))
          overlayClick = target.action;
      overlayPress = OverlayClick::None;
      return 0;
    }
    if (message == WM_LBUTTONDOWN)
      for (const auto &target : overlayTargets)
        if (target.action != OverlayClick::None && PtInRect(&target.rect, p)) {
          overlayPress = target.action;
          return 0;
        }
    if (PtInRect(&overlayShield, p))
      return 0;
  }
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
                        : w == 'R'           ? 19
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
    steeringAssist.store(0);
    diagnosticDrivingPov.store(0xffffffff);
    captureQuickInput.store(0);
    monitorInput.store(false);
    pedalSlot.store(-1);
    nativePedals.store(0);
  }
  if (message == WM_ACTIVATEAPP && !w) {
    overlayPress = overlayClick = OverlayClick::None;
    assignmentFocusLost.store(true);
    stopMotors();
    cancelFinishHold();
    diagnosticKeys.store(0);
    releaseKeys(true);
    releaseMouse();
    captureButtons.store(false);
    captureRaceAxes.store(false);
    steeringAssist.store(0);
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
            keyboard = false, manualKeyboard = false;
static bool adjustingSlider = false;
static int selected = -1, settingRow = 0, settingsTab = 0;
struct KeyboardSelection {
  int row = 0, col = 0;
  bool upper = false, symbols = false;
  float preferredX = -1;
};
static KeyboardSelection gameKeys, profileKeys;
static bool modsText = false, gameTextEntry = false, dismissedText = false;
static bool rfsCursor = false;
static ControllerId activeSlot = -1;
static bool playStationCaptions = false;
static DWORD vibrationPreviewResult = ERROR_CANCELLED;
static std::vector<Item> items;
static int mainMenuInitial = -1, raceFinishItem = -1, raceResumeItem = -1;
static int optionsInitial = -1, optionsLast = -1;
static Rect optionsExtension{};
static bool optionsEntryFocus = false;
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
static PlayerProfiles profiles;
static bool profileNaming = false;
static std::wstring profileNameDraft;
struct Config : PlayerInputSettings {
  Language language = Language::Turkish;
  int slot = -1;
  bool diagnostics = false;
  bool autoUpdate = true;
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
  controllerSetting, profileSetting, languageSetting, controllerIconsSetting,
  controllerCheckSetting,
  deadzoneSetting, cursorSpeedSetting, firstBindingSetting,
  quickMenuSetting = firstBindingSetting + 9, monitorSetting, rumbleSetting,
  steeringSetting, steeringHelpSetting, clutchSetting,
  shortcutSetting, shortcutUsageSetting, autoUpdateSetting, manualUpdateSetting, settingCount
};
struct SettingsTab {
  LocalizedText label;
  int first, end;
};
static constexpr SettingsTab settingsTabs[] = {
    {{"Genel", "General"}, controllerSetting, deadzoneSetting},
    {{"Kontroller", "Controls"}, deadzoneSetting, quickMenuSetting},
    {{"Sürüş", "Driving"}, quickMenuSetting, shortcutSetting},
    {{"Kısayollar", "Shortcuts"}, shortcutSetting, autoUpdateSetting},
    {{"Güncelleme", "Updates"}, autoUpdateSetting, settingCount},
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
static constexpr wchar_t profileSection[] = L"PlayerProfiles";

static std::wstring profileKeyPrefix(int slot) {
  return L"P" + std::to_wstring(slot + 1) + L".";
}

static PlayerInputSettings readPlayerInput(const wchar_t *section, const wchar_t *shortcutsSection,
                                           const std::wstring &prefix) {
  PlayerInputSettings result;
  const auto get = [&](const wchar_t *key, int fallback) {
    return int(GetPrivateProfileIntW(section, (prefix + key).c_str(), fallback, ini.c_str()));
  };
  result.deadzone = std::clamp(get(L"DeadzonePercent", 24), 10, 45) / 100.f;
  result.cursorSpeed = float(std::clamp(get(L"CursorSpeed", 900), 200, 2400));
  for (int i = 0; i < int(result.bindings.size()); ++i)
    rebind(result.bindings, i, std::clamp(get(bindingKeys[i], i), 0, int(physical.size()) - 1));
  result.quickButton = std::clamp(get(L"QuickMenu", 1), 0, 3);
  result.controllerIcons = get(L"ControllerIcons", 1) != 0;
  result.inputMonitor = get(L"InputMonitor", 0) != 0;
  result.rumbleStrength = std::clamp(get(L"RumbleStrength", 50), 0, 100);
  result.steeringCurve = SteeringCurve(std::clamp(get(L"SteeringCurve", 0), 0, 2));
  result.steeringHelp = SteeringHelp(std::clamp(get(L"SpeedSteering", int(result.steeringHelp)), 0, 2));
  result.shortcutSequential = get(L"ShortcutSequential", 0) == 1;
  for (std::size_t i = 0; i < shortcutMenus.size(); ++i) {
    wchar_t value[128]{};
    const auto &menu = shortcutMenus[i];
    const auto length = GetPrivateProfileStringW(shortcutsSection, (prefix + menu.key).c_str(),
                                                  L"", value, _countof(value), ini.c_str());
    if (length < _countof(value) - 1)
      result.shortcuts[i] = parseShortcutDirections(menu, value);
  }
  return result;
}

template <typename Put>
static void writePlayerInput(const PlayerInputSettings &input, Put put) {
  put(L"DeadzonePercent", int(input.deadzone * 100 + .5f));
  put(L"CursorSpeed", int(input.cursorSpeed));
  put(L"QuickMenu", input.quickButton);
  put(L"InputMonitor", input.inputMonitor);
  put(L"ControllerIcons", input.controllerIcons);
  put(L"RumbleStrength", input.rumbleStrength);
  put(L"SteeringCurve", int(input.steeringCurve));
  put(L"SpeedSteering", int(input.steeringHelp));
  put(L"ShortcutSequential", input.shortcutSequential);
  for (std::size_t i = 0; i < input.bindings.size(); ++i)
    put(bindingKeys[i], input.bindings[i]);
}

static void loadConfig() {
  missingSlot = false;
  shortcutEditor = profileNaming = settingsSaveFailed = false;
  ini = path(L"LfsPad.ini");
  wchar_t language[16]{};
  GetPrivateProfileStringW(L"General", L"Language", L"tr", language,
                           _countof(language), ini.c_str());
  cfg.language = parseLanguage(language);
  cfg.autoUpdate =
      GetPrivateProfileIntW(L"General", L"AutoUpdate", 1, ini.c_str()) != 0;
  cfg.slot = std::clamp(
      (int)GetPrivateProfileIntW(L"Input", L"Controller", 0, ini.c_str()) - 1,
      -1, nativePreference);
  cfg.diagnostics =
      GetPrivateProfileIntW(L"General", L"Diagnostics", 0, ini.c_str()) != 0;
  diagnosticsEnabled = cfg.diagnostics;
  profiles = {};
  for (int i = 0; i < PlayerProfiles::count; ++i) {
    const auto prefix = profileKeyPrefix(i);
    auto &profile = profiles.slots[i];
    if (GetPrivateProfileIntW(profileSection, (prefix + L"Used").c_str(), 0, ini.c_str()) != 1)
      continue;
    profile.used = true;
    profile.input = readPlayerInput(profileSection, profileSection, prefix);
    wchar_t encoded[profileNameLimit * 4 + 2]{};
    GetPrivateProfileStringW(profileSection, (prefix + L"NameHex").c_str(), L"", encoded,
                             _countof(encoded), ini.c_str());
    profile.name = decodeProfileName(encoded);
  }
  profiles.enabled = profiles.slots[0].used;
  const auto active = GetPrivateProfileIntW(profileSection, L"Active", 1, ini.c_str());
  if (profiles.enabled && active >= 1 && active <= PlayerProfiles::count &&
      profiles.slots[active - 1].used)
    profiles.active = int(active - 1);
  if (!profiles.enabled) {
    profiles = {};
    profiles.current().input = readPlayerInput(L"Input", shortcutSection, L"");
    profiles.current().used = true;
  }
  static_cast<PlayerInputSettings &>(cfg) = profiles.current().input;
  savedShortcuts = cfg.shortcuts;
  inputSlot = cfg.slot;
}

static bool savePlayerProfiles() {
  profiles.current().input = cfg;
  profiles.current().used = true;
  std::wstring records;
  const auto append = [&](const std::wstring &key, const std::wstring &value) {
    records += key;
    records += L'=';
    records += value;
    records += wchar_t{};
  };
  append(L"Active", std::to_wstring(profiles.active + 1));
  for (int i = 0; i < PlayerProfiles::count; ++i) {
    const auto &profile = profiles.slots[i];
    if (!profile.used)
      continue;
    const auto prefix = profileKeyPrefix(i);
    append(prefix + L"Used", L"1");
    if (!profile.name.empty())
      append(prefix + L"NameHex", encodeProfileName(profile.name));
    writePlayerInput(profile.input, [&](const wchar_t *key, int value) {
      append(prefix + key, std::to_wstring(value));
    });
    for (std::size_t j = 0; j < shortcutMenus.size(); ++j) {
      const auto &menu = shortcutMenus[j];
      append(prefix + menu.key, formatShortcutDirections(menu, profile.input.shortcuts[j]));
    }
  }
  return WritePrivateProfileSectionW(profileSection, records.c_str(), ini.c_str()) != 0;
}

static bool saveConfig() {
  if (!WritePrivateProfileStringW(L"Input", L"Controller", std::to_wstring(cfg.slot + 1).c_str(), ini.c_str()) ||
      !WritePrivateProfileStringW(L"General", L"Language", languageKey(cfg.language), ini.c_str()) ||
      !WritePrivateProfileStringW(L"General", L"AutoUpdate", cfg.autoUpdate ? L"1" : L"0", ini.c_str()))
    return false;
  if (profiles.enabled)
    return savePlayerProfiles();
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
  writePlayerInput(cfg, put);
  return inputSaved;
}
enum class UpdateView { Idle, Checking, Offer, Downloading, Current, Failed };
static UpdateView updateView = UpdateView::Idle;
static HANDLE updateWorker = nullptr;
static bool updateAutomatic = false, updateExitSent = false;
static bool updateAccept = true;
static std::wstring updateRequest;
static std::string updateVersion, updateHash;

static std::string readUpdateSignal(const wchar_t *suffix) {
  const auto signal = updateRequest + suffix;
  const auto file = CreateFileW(signal.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return {};
  BY_HANDLE_FILE_INFORMATION info{};
  char payload[110]{};
  DWORD length = 0;
  const bool read = GetFileInformationByHandle(file, &info) &&
      !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) &&
      !info.nFileSizeHigh && info.nFileSizeLow < sizeof(payload) &&
      ReadFile(file, payload, sizeof(payload), &length, nullptr);
  CloseHandle(file);
  return read ? std::string(payload, length) : std::string{};
}

static bool parseUpdateOffer(const std::string &result) {
  if (result.compare(0, 6, "offer ") != 0)
    return false;
  const auto space = result.find(' ', 6);
  if (space == std::string::npos)
    return false;
  const auto version = result.substr(6, space - 6);
  const auto hash = result.substr(space + 1);
  if (version.empty() || version.size() > 29 || version.front() == '.' ||
      version.back() == '.' || version.find("..") != std::string::npos ||
      std::count(version.begin(), version.end(), '.') != 2 ||
      version.find_first_not_of("0123456789.") != std::string::npos ||
      hash.size() != 64 || hash.find_first_not_of("0123456789abcdef") != std::string::npos)
    return false;
  updateVersion = version;
  updateHash = hash;
  return true;
}

static bool startUpdate(bool install, bool automatic) {
  if (updateWorker)
    return false;
  const auto worker = path(L"addons\\LfsPad\\update\\update.ps1");
  if (worker.empty() || GetFileAttributesW(worker.c_str()) == INVALID_FILE_ATTRIBUTES)
    return false;
  wchar_t system[MAX_PATH]{};
  const auto length = GetSystemDirectoryW(system, _countof(system));
  if (!length || length >= _countof(system))
    return false;
  const std::wstring powershell = std::wstring(system) +
      L"\\WindowsPowerShell\\v1.0\\powershell.exe";
  static unsigned sequence = 0;
  const auto request = std::to_wstring(GetCurrentProcessId()) + L"-" +
      std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(++sequence);
  updateRequest = path((L"addons\\LfsPad\\update\\request-" + request).c_str());
  if (updateRequest.empty())
    return false;
  std::wstring command = L"\"" + powershell +
      L"\" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy RemoteSigned -File \"" +
      worker + L"\" -GamePid " + std::to_wstring(GetCurrentProcessId()) +
      L" -RunningVersion " L"" LFSPAD_VERSION L" -RequestId " + request;
  if (automatic)
    command += L" -Automatic";
  if (install)
    command += L" -Phase Install -TargetVersion " +
        std::wstring(updateVersion.begin(), updateVersion.end()) + L" -ExpectedArchiveHash " +
        std::wstring(updateHash.begin(), updateHash.end());
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if (CreateProcessW(powershell.c_str(), command.data(), nullptr, nullptr, FALSE,
                     CREATE_NO_WINDOW, nullptr, system, &startup, &process)) {
    CloseHandle(process.hThread);
    updateWorker = process.hProcess;
    updateAutomatic = automatic;
    updateExitSent = false;
    updateAccept = false;
    updateView = install ? UpdateView::Downloading : UpdateView::Checking;
    return true;
  }
  log("Update worker could not start: %lu", GetLastError());
  return false;
}

static void pollUpdate(bool safeToExit, ULONGLONG now) {
  static ULONGLONG nextPoll = 0;
  if (!updateWorker || now < nextPoll)
    return;
  nextPoll = now + 180;
  if (WaitForSingleObject(updateWorker, 0) != WAIT_TIMEOUT) {
    CloseHandle(updateWorker);
    updateWorker = nullptr;
    const auto result = readUpdateSignal(L".result");
    DeleteFileW((updateRequest + L".result").c_str());
    if (updateView == UpdateView::Checking && parseUpdateOffer(result)) {
      updateView = UpdateView::Offer;
      updateAccept = true;
    } else if (updateView == UpdateView::Checking && result == "current")
      updateView = updateAutomatic ? UpdateView::Idle : UpdateView::Current;
    else
      updateView = updateView == UpdateView::Downloading || !updateAutomatic
          ? UpdateView::Failed : UpdateView::Idle;
  } else if (updateView == UpdateView::Downloading && !updateExitSent && safeToExit &&
             readUpdateSignal(L".ready") == "ready") {
    updateExitSent = simCommand("/exit");
    if (updateExitSent)
      log("LfsPad update verified; sent LFS a clean exit request");
  }
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
  optionsLast = -1;
  optionsExtension = {};
  modsText = false;
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
  Rect extraOption{};
  for (int i = 0; i < count; ++i) {
    auto &n = native[i];
    auto r = n.rect;
    // Mods' Search and Author fields become an inline editor without
    // changing owner or opening the standard text dialog. The native clear
    // button remains immediately to the right of either focused field.
    if (modal && n.owner == modal && n.type == 14 && !n.callback && n.value == 0 &&
        i + 1 < count && validMenuRect(r)) {
      const auto &clear = native[i + 1];
      modsText |= clear.owner == modal && clear.type == 0 &&
          clear.callback == uintptr_t(imageBase) + 0x4c950 &&
          (clear.value == 1 || clear.value == 2) &&
          std::abs(r.r - clear.rect.l) < .002f && std::abs(r.t - clear.rect.t) < .002f &&
          std::abs(r.b - clear.rect.b) < .002f;
    }
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
    if (optionsTab && n.value == 9) {
      optionsLast = int(items.size());
      const float tabHeight = r.b - r.t;
      optionsExtension = {r.l, r.b + tabHeight * .29f, r.r, r.b + tabHeight * 1.29f};
    }
    if (n.owner == uintptr_t(imageBase) + 0x49129c && n.type == 4 && n.value == 10)
      extraOption = r;
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
  if (optionsLast >= 0 && validMenuRect(extraOption)) {
    // RFS uses the next full-width position. Leave the small native arrows
    // clear and use the remaining width of their row.
    const float tabHeight = extraOption.b - extraOption.t;
    optionsExtension.t = extraOption.b + tabHeight * .5f;
    optionsExtension.b = optionsExtension.t + tabHeight;
    optionsExtension.l += (optionsExtension.r - optionsExtension.l) * .48f;
  }
  if (validMenuRect(optionsExtension) && std::any_of(items.begin(), items.end(), [](const Item &item) {
        const auto &a = item.rect, &b = optionsExtension;
        return a.l < b.r && b.l < a.r && a.t < b.b && b.t < a.b;
      }))
    optionsExtension = {};
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
static POINT moveMenuCursor(float rx, float ry, float width, float height, float dt) {
  POINT p{};
  GetCursorPos(&p);
  ScreenToClient(window, &p);
  p.x = LONG(std::clamp(p.x + rx * cfg.cursorSpeed * dt, 0.f, width - 1));
  p.y = LONG(std::clamp(p.y - ry * cfg.cursorSpeed * dt, 0.f, height - 1));
  ImGui::GetForegroundDrawList()->AddCircle({float(p.x), float(p.y)}, 8,
                                            IM_COL32(70, 255, 205, 255), 0, 2);
  if ((rx || ry) && GetForegroundWindow() == window) {
    POINT screen = p;
    ClientToScreen(window, &screen);
    SetCursorPos(screen.x, screen.y);
  }
  return p;
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
static const ShortcutMenu &shortcutMenu(ShortcutScreen screen) {
  return shortcutMenus[std::size_t(screen)];
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
  const auto &garage = shortcutMenu(ShortcutScreen::SinglePlayer).targets[1];
  return findShortcutItem(garage, modal) >= 0 ? ShortcutScreen::SinglePlayer
                                              : ShortcutScreen::None;
}
static constexpr ShortcutTarget axesTab{L"axes", axesLabel, 0x10aba0, 0, 5};
static constexpr ShortcutTarget driverTab{L"driver", {"Sürücü", "Driver"}, 0x111760, 4, 6};

static int nativeClutchItem(uintptr_t modal) {
  // 0.8C38 exposes this toggle only when the current car and gearbox support it.
  const ShortcutTarget toggle{L"auto-clutch", clutchLabel, 0xc8eb0, 23,
                              unsigned(uintptr_t(imageBase) + 0x2805b8)};
  return findShortcutItem(toggle, modal);
}

static bool editableGarageColourSelected() {
  const auto &colours = garageList(ListKind::Colours).entries;
  return std::any_of(colours.begin(), colours.end(),
                     [](const ListItem &colour) { return items[colour.item].active; });
}

static int favoriteItem(FavoritePage destination, ShortcutScreen source, uintptr_t modal) {
  switch (destination) {
  case FavoritePage::Options:
    if (source == ShortcutScreen::Main)
      return findShortcutItem(shortcutMenu(source).targets[2], modal);
    if (source == ShortcutScreen::Race)
      return findShortcutItem(shortcutMenu(source).targets[3], modal);
    break;
  case FavoritePage::Controls: {
    const auto &menu = shortcutMenu(ShortcutScreen::Options);
    return findShortcutItem(menu.targets[1], uintptr_t(imageBase) + menu.ownerRva);
  }
  case FavoritePage::Driver:
    return findShortcutItem(driverTab, uintptr_t(imageBase) +
                                      shortcutMenu(ShortcutScreen::Options).ownerRva);
  case FavoritePage::Axes:
    return findShortcutItem(axesTab, modal);
  case FavoritePage::Colours:
    if (editableGarageColourSelected())
      return findShortcutItem(shortcutMenu(ShortcutScreen::Garage).targets[3], modal);
    break;
  case FavoritePage::Skins:
    if (editableGarageColourSelected())
      return findShortcutItem(shortcutMenu(ShortcutScreen::Garage).targets[5], modal);
    break;
  default:
    break;
  }
  return -1;
}

static FavoritePage favoritePage(Favorite favorite, ShortcutScreen source,
                                 uintptr_t sourceOwner, ShortcutScreen screen,
                                 uintptr_t modal) {
  if (items.empty())
    return FavoritePage::Transition;
  if (favorite == Favorite::Skins) {
    if (garageSkins)
      return FavoritePage::Skins;
    if (screen != ShortcutScreen::Garage || modal != sourceOwner)
      return FavoritePage::Other;
    const int colours = favoriteItem(FavoritePage::Colours, source, modal);
    return colours >= 0 && items[colours].active ? FavoritePage::Colours
                                                 : FavoritePage::Garage;
  }
  if (screen == source && screen != ShortcutScreen::Options && modal == sourceOwner)
    return FavoritePage::Entry;
  // Options draws the current page before its fixed-owner category bar. The
  // modal points to the page itself and changes when a category is selected.
  if (screen != ShortcutScreen::Options || optionsInitial < 0 ||
      !items[optionsInitial].active)
    return FavoritePage::Other;
  const auto *native = reinterpret_cast<const NativeItem *>(imageBase + 0x28ca20);
  const bool ownsPage = std::any_of(items.begin(), items.begin() + optionsInitial,
      [&](const Item &item) { return native[item.id].owner == modal; });
  if (!ownsPage)
    return FavoritePage::Other;
  if (favorite == Favorite::Clutch) {
    const int driver = favoriteItem(FavoritePage::Driver, source, modal);
    return driver >= 0 && items[driver].active ? FavoritePage::Driver : FavoritePage::Options;
  }
  const int controls = favoriteItem(FavoritePage::Controls, source, modal);
  if (controls >= 0 && items[controls].active)
    return pedalCalibration ? FavoritePage::Axes : FavoritePage::Controls;
  return FavoritePage::Options;
}

static ResolvedShortcuts findMenuShortcuts(ShortcutScreen screen, uintptr_t modal) {
  if (screen == ShortcutScreen::None)
    return {};
  const auto index = std::size_t(screen);
  const auto &menu = shortcutMenus[index];
  const auto owner = menu.ownerRva ? uintptr_t(imageBase) + menu.ownerRva : modal;
  return resolveShortcuts(menu, cfg.shortcuts[index], [&](const ShortcutTarget &target) {
    if (target.favorite != Favorite::None) {
      const auto page = favoritePage(target.favorite, screen, modal, screen, modal);
      return favoriteItem(nextFavoritePage(target.favorite, page), screen, modal);
    }
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
  if (GetForegroundWindow() != window)
    return;
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
static RECT hitRect(ImVec2 a, ImVec2 b) {
  return {LONG(a.x), LONG(a.y), LONG(b.x), LONG(b.y)};
}
static void drawOptionsEntry(bool focused) {
  const auto a = pixel(optionsExtension.l, optionsExtension.t),
             b = pixel(optionsExtension.r, optionsExtension.b);
  overlayTargets[0] = {hitRect(a, b), OverlayClick::OptionsTab};
  auto *draw = ImGui::GetForegroundDrawList();
  draw->AddRectFilled(a, b, IM_COL32(18, 26, 32, 211), 4);
  draw->AddRectFilled(a, {a.x + 4, b.y}, IM_COL32(255, 181, 79, 255), 2);
  if (focused)
    draw->AddRect(a, b, IM_COL32(119, 244, 207, 255), 4, 0, 2);
  const auto size = ImGui::CalcTextSize("LfsPad");
  label((a.x + b.x - size.x) * .5f, (a.y + b.y - size.y) * .5f, "LfsPad");
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
                              int selection, Favorite favorite, bool waiting) {
  const auto hint = [&](int direction) {
    const auto &entry = shortcuts[direction];
    const ImU32 colour = entry.item < 0 ? IM_COL32(151, 160, 171, 240)
        : (entry.favorite == Favorite::None ? entry.item == selection : entry.favorite == favorite)
                                  ? IM_COL32(105, 243, 211, 255)
                                  : IM_COL32(245, 245, 245, 255);
    return HudHint{PadUp + direction, entry.label ? entry.label->get(cfg.language) : nullptr,
                   -1, colour};
  };
  drawCornerHints(width, height, false, {
      hint(0), hint(1), {cfg.bindings[SettingsModifier], waiting
          ? text("İptal", "Cancel") : text("Bırak", "Release")}});
  drawCornerHints(width, height, true, {
      hint(2), hint(3), {cfg.bindings[Back], text("İptal", "Cancel")}});

}
static std::string utf8(const std::wstring &s) {
  int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0,
                              nullptr, nullptr);
  std::string out(n, 0);
  WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n,
                      nullptr, nullptr);
  return out;
}
static std::string profileCaption() {
  if (!profiles.current().name.empty())
    return utf8(profiles.current().name);
  if (!profiles.active)
    return text("Varsayılan", "Default");
  return std::string(text("Oyuncu ", "Player ")) + std::to_string(profiles.active + 1);
}

static void blockMenuInput();

static void finishProfileName(bool apply) {
  if (apply) {
    profiles.current().name = normalizeProfileName(profileNameDraft);
    profiles.enabled = true;
  }
  profileNaming = false;
  blockMenuInput();
}

static void moveKeyboard(KeyboardSelection &keys, const bool (&nav)[4], bool next) {
  const auto rows = keyboardRows(cfg.language, keys.upper, keys.symbols);
  const auto columns = [&](int row) { return row == 4 ? 6 : int(rows[row].size()); };
  keys.col = std::clamp(keys.col, 0, columns(keys.row) - 1);
  if (nav[0] != nav[1]) {
    if (keys.preferredX < 0)
      keys.preferredX = keyboardKey(keys.row, keys.col, columns(keys.row)).center();
    keys.row = (keys.row + (nav[0] ? 4 : 1)) % 5;
    keys.col = nearestKeyboardKey(keys.row, columns(keys.row), keys.preferredX);
  }
  if (nav[2] != (nav[3] || next)) {
    const int count = columns(keys.row), step = nav[2] ? count - 1 : 1;
    if (keys.row < 4)
      keys.col = (keys.col + step) % count;
    else {
      const auto position = std::find(std::begin(keyboardFooterOrder),
                                      std::end(keyboardFooterOrder), keys.col);
      keys.col = keyboardFooterOrder[(position - std::begin(keyboardFooterOrder) + step) % count];
    }
    keys.preferredX = -1;
  }
}

static void closeGameKeyboard(bool submit) {
  if (submit || gameTextEntry)
    simKey(submit ? "enter" : "esc");
  keyboard = manualKeyboard = false;
  dismissedText = true;
  blockMenuInput();
}

static void keyAction() {
  auto &keys = profileNaming ? profileKeys : gameKeys;
  const auto rows = keyboardRows(cfg.language, keys.upper, keys.symbols);
  const auto insert = [&](wchar_t key) {
    if (!profileNaming)
      character(key);
    else if (profileNameDraft.size() < profileNameLimit && profileNameCharacter(key))
      profileNameDraft += key;
  };
  if (keys.row < 4) {
    insert(rows[keys.row][keys.col]);
    return;
  }
  switch (keys.col) {
  case 0:
    insert(L' ');
    break;
  case 1:
    if (!profileNaming)
      localKey(VK_BACK);
    else if (!profileNameDraft.empty())
      profileNameDraft.pop_back();
    break;
  case 2:
    keys.upper = !keys.upper;
    keys.preferredX = -1;
    break;
  case 3:
    keys.symbols = !keys.symbols;
    keys.preferredX = -1;
    break;
  case 4:
  case 5:
    if (profileNaming)
      finishProfileName(keys.col == 4);
    else
      closeGameKeyboard(keys.col == 4);
    break;
  }
}
static bool selectClickedKey(KeyboardSelection &keys, OverlayClick click) {
  const int key = int(click) - int(OverlayClick::KeyboardFirst);
  if (key < 0 || key >= 60)
    return false;
  keys.row = key / 12;
  keys.col = key % 12;
  keys.preferredX = -1;
  return true;
}
static void drawKeyboard(float width, float height) {
  const auto &keys = profileNaming ? profileKeys : gameKeys;
  const float cw = std::min((width - 44) / 12, 68.f);
  const float ch = std::clamp(height * .062f, 29.f, 54.f);
  const float w = cw * 12 + 28, l = (width - w) / 2;
  const float h = ch * 5 + (profileNaming ? 100.f : 70.f);
  const float t = std::max(8.f, height - h - 21.f);
  panel(l, t, l + w, t + h);
  overlayShield = hitRect({l, t}, {l + w, t + h});
  char hint[160];
  if (profileNaming)
    std::snprintf(hint, sizeof(hint), "%s", text("Profil adı", "Profile name"));
  else
    std::snprintf(hint, sizeof(hint), "%s", text("Türkçe Q", "English QWERTY"));
  label(l + 16, t + 10, hint);
  std::snprintf(hint, sizeof(hint), text("%s Sil   %s Büyük/küçük   %s Tamam", "%s Delete   %s Change case   %s Done"),
                profileNaming ? buttonCaption(PadX) : bindingName(Secondary),
                profileNaming ? buttonCaption(PadY) : bindingName(Keyboard),
                profileNaming ? buttonCaption(PadStart) : bindingName(Menu));
  const float hintWidth = ImGui::CalcTextSize(hint).x;
  if (hintWidth + 180 < w)
    label(l + w - 16 - hintWidth, t + 10, hint, IM_COL32(192, 205, 220, 255));
  const auto rows = keyboardRows(cfg.language, keys.upper, keys.symbols);
  auto d = ImGui::GetForegroundDrawList();
  for (int row = 0; row < 5; ++row) {
    int count = row == 4 ? 6 : int(rows[row].size());
    for (int col = 0; col < count; ++col) {
      const auto key = keyboardKey(row, col, count);
      const float x = l + 14 + key.x * cw, y = t + 41 + row * ch, cell = key.width * cw;
      const bool selectedKey = row == keys.row && col == keys.col;
      const bool done = row == 4 && col == 4;
      const ImVec2 from{x + 3, y + 3}, to{x + cell - 3, y + ch - 3};
      const int index = row * 12 + col;
      overlayTargets[index + 2] = {hitRect(from, to),
          OverlayClick(int(OverlayClick::KeyboardFirst) + index)};
      d->AddRectFilled(from, to, selectedKey ? IM_COL32(35, 120, 101, 255)
                          : done ? IM_COL32(33, 75, 83, 255) : IM_COL32(45, 53, 66, 255), 6);
      if (selectedKey)
        d->AddRect(from, to, IM_COL32(119, 244, 207, 255), 6, 0, 2);
      const char *special[] = {text("Boşluk", "Space"), text("Sil", "Delete"),
                               keys.upper ? "AA" : "Aa", keys.symbols ? "ABC" : "#+=",
                               text("Tamam", "Done"), text("İptal", "Cancel")};
      const auto caption = row == 4 ? std::string(special[col])
                                    : utf8(std::wstring(1, rows[row][col]));
      const bool available = !profileNaming || row == 4 || profileNameCharacter(rows[row][col]);
      const auto size = ImGui::CalcTextSize(caption.c_str());
      label(x + (cell - size.x) * .5f, y + (ch - size.y) * .5f, caption.c_str(),
            available ? IM_COL32_WHITE : IM_COL32(145, 155, 165, 255));
    }
  }
  if (profileNaming) {
    const auto draft = profileNameDraft.empty() ? profileCaption() : utf8(profileNameDraft);
    label(l + 20, t + 64 + ch * 5, draft.c_str());
    char length[16];
    std::snprintf(length, sizeof(length), "%u/%u", unsigned(profileNameDraft.size()),
                  unsigned(profileNameLimit));
    label(l + w - 20 - ImGui::CalcTextSize(length).x, t + 64 + ch * 5, length);
  }
}
static void adjustSetting(int direction) {
  switch (settingRow) {
  case controllerSetting:
    cfg.slot = (cfg.slot + 1 + direction + 6) % 6 - 1;
    missingSlot = false;
    break;
  case profileSetting:
    profiles.select(direction, cfg);
    blockMenuInput();
    break;
  case languageSetting:
    cfg.language = cfg.language == Language::English ? Language::Turkish : Language::English;
    break;
  case controllerIconsSetting:
    cfg.controllerIcons = !cfg.controllerIcons;
    break;
  case autoUpdateSetting:
    cfg.autoUpdate = !cfg.autoUpdate;
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
  case controllerCheckSetting:
  case clutchSetting:
    break;
  case shortcutUsageSetting:
    cfg.shortcutSequential = !cfg.shortcutSequential;
    break;
  case rumbleSetting:
    cfg.rumbleStrength = std::clamp(cfg.rumbleStrength + direction * 10, 0, 100);
    break;
  case steeringSetting:
    cfg.steeringCurve = SteeringCurve((int(cfg.steeringCurve) + 3 + direction) % 3);
    break;
  case steeringHelpSetting:
    cfg.steeringHelp = SteeringHelp((int(cfg.steeringHelp) + 3 + direction) % 3);
    break;
  default:
    int action = settingRow - firstBindingSetting;
    const int count = int(physical.size());
    rebind(cfg.bindings, action, (cfg.bindings[action] + direction + count) % count);
    break;
  }
}
static constexpr float settingsWidth = 760.f, settingsHeight = 420.f;
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
static void drawSettings(float l, float t, float w, bool clutchAvailable) {
  panel(l, t, l + w, t + settingsHeight);
  overlayShield = hitRect({l, t}, {l + w, t + settingsHeight});
  const auto &tab = settingsTabs[settingsTab];
  auto *draw = ImGui::GetForegroundDrawList();
  const float tabWidth = std::min(w * .29f, 172.f);
  const float content = l + tabWidth + 30.f;
  draw->AddLine({l + 20, t + 49}, {l + w - 20, t + 49}, IM_COL32(255, 181, 79, 255), 2);
  label(l + 22, t + 15, text("Seçenekler / LfsPad", "Options / LfsPad"));
  char tabHint[96];
  std::snprintf(tabHint, sizeof(tabHint), text("%s / %s · Sekme", "%s / %s · Tabs"),
                buttonCaption(PadLB), buttonCaption(PadRB));
  label(l + w - ImGui::CalcTextSize(tabHint).x - 22, t + 16, tabHint);
  for (int i = 0; i < int(std::size(settingsTabs)); ++i) {
    const float y = t + 68.f + 45.f * i;
    const char *name = settingsTabs[i].label.get(cfg.language);
    overlayTargets[62 + i] = {hitRect({l + 20, y}, {l + tabWidth + 14, y + 37}),
        OverlayClick(int(OverlayClick::SettingsTabFirst) + i)};
    draw->AddRectFilled({l + 20, y}, {l + tabWidth + 14, y + 37},
                        i == settingsTab ? IM_COL32(79, 63, 38, 255)
                                         : IM_COL32(36, 44, 54, 255), 3);
    if (i == settingsTab)
      draw->AddRectFilled({l + 20, y}, {l + 24, y + 37}, IM_COL32(255, 181, 79, 255), 2);
    label(l + 32, y + 7, name,
          i == settingsTab ? IM_COL32_WHITE : IM_COL32(166, 185, 202, 255));
  }
  const char *actionNames[] = {text("Onay", "Confirm"),
                                    text("Geri", "Back"),
                                    text("Sağ tıklama / sil", "Right click / delete"),
                                    text("Klavye / büyük harf", "Keyboard / change case"),
                                    text("Önceki sekme", "Previous tab"),
                                    text("Sonraki sekme", "Next tab"),
                                    text("Menü / metni onayla", "Menu / submit text"),
                                    text("Menü kısayolları", "Menu shortcuts"),
                                    text("İmleç modu", "Cursor mode")};
  char values[settingCount][100];
  if (cfg.slot == nativePreference)
    std::snprintf(values[controllerSetting], 80, "%s", text("Kontrolcü: Yerel gamepad / USB adaptör", "Controller: Native gamepad / USB adapter"));
  else if (cfg.slot >= 0)
    std::snprintf(values[controllerSetting], 80, text("Kontrolcü: %d", "Controller: %d"), cfg.slot + 1);
  else
    std::snprintf(values[controllerSetting], 80, "%s", text("Kontrolcü: Otomatik", "Controller: Auto"));
  std::snprintf(values[languageSetting], 100, "%s", text("Dil: Türkçe", "Language: English"));
  std::snprintf(values[profileSetting], 100, text("Oyuncu profili: %s", "Player profile: %s"),
                profileCaption().c_str());
  std::snprintf(values[controllerIconsSetting], 100, text("Kontrolcü simgeleri: %s", "Controller icons: %s"),
                cfg.controllerIcons ? text("Açık", "On") : text("Kapalı", "Off"));
  std::snprintf(values[autoUpdateSetting], 100, text("Açılışta güncelleme denetle: %s", "Check on launch: %s"),
                cfg.autoUpdate ? text("Açık", "On") : text("Kapalı", "Off"));
  std::snprintf(values[manualUpdateSetting], 100, "%s", text("Şimdi güncelleme denetle", "Check for updates now"));
  std::snprintf(values[controllerCheckSetting], 100, "%s", text("Kontrolcümü test et", "Test my controller"));
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
  std::snprintf(values[shortcutUsageSetting], 100, text("Kısayol kullanımı: %s", "Shortcut input: %s"),
                cfg.shortcutSequential ? text("Art arda bas", "Tap then choose")
                                       : text("Basılı tut", "Hold"));
  std::snprintf(values[rumbleSetting], 100, text("Sürüş titreşimi: %d%%", "Driving vibration: %d%%"),
                cfg.rumbleStrength);
  const char *steeringOptions[]{text("Doğrusal", "Linear"), text("Yumuşak", "Soft"),
                               text("Çok yumuşak", "Extra soft")};
  std::snprintf(values[steeringSetting], 100, text("Direksiyon eğrisi: %s", "Steering curve: %s"),
                steeringOptions[unsigned(cfg.steeringCurve)]);
  const char *steeringHelpOptions[]{text("Kapalı", "Off"), text("Dengeli", "Balanced"),
                                  text("Güçlü", "Strong")};
  std::snprintf(values[steeringHelpSetting], 100, text("Direksiyon yardımı: %s", "Steering assist: %s"),
                steeringHelpOptions[unsigned(cfg.steeringHelp)]);
  std::snprintf(values[clutchSetting], 100, "%s", clutchLabel.get(cfg.language));
  constexpr int visibleRows = 6;
  const int first = std::clamp(settingRow - visibleRows / 2, tab.first,
                               std::max(tab.first, tab.end - visibleRows));
  draw->PushClipRect({content, t + 62}, {l + w - 18, t + 309}, true);
  for (int row = 0; row < visibleRows && first + row < tab.end; ++row) {
    const int i = first + row;
    const float y = t + 68 + row * 39.f;
    const ImVec2 a{content, y}, b{l + w - 20, y + 34};
    overlayTargets[67 + row] = {hitRect(a, b),
        OverlayClick(int(OverlayClick::SettingsRowFirst) + i)};
    draw->AddRectFilled(a, b, i == settingRow ? IM_COL32(37, 81, 76, 255)
                                              : IM_COL32(37, 45, 54, 255), 3);
    if (i == settingRow)
      draw->AddRect(a, b, bindingCapture.active() ? IM_COL32(255, 181, 79, 255)
                                                 : IM_COL32(119, 244, 207, 255), 3);
    const auto colour = i == clutchSetting && !clutchAvailable ?
        IM_COL32(166, 185, 202, 255) : IM_COL32_WHITE;
    const auto *divider = std::strstr(values[i], ": ");
    const auto *value = divider ? divider + 2 : nullptr;
    const float valueWidth = value ? ImGui::CalcTextSize(value).x : 0.f;
    const float lWidth = ImGui::CalcTextSize(values[i], divider).x;
    if (value && lWidth + valueWidth + 60 < b.x - a.x) {
      draw->AddText({a.x + 12, y + 6}, colour, values[i], divider);
      label(b.x - 12 - valueWidth, y + 6, value, IM_COL32(183, 223, 215, 255));
    } else
      label(a.x + 12, y + 6, values[i], colour);
  }
  draw->PopClipRect();
  if (tab.end - tab.first > visibleRows) {
    char position[32];
    std::snprintf(position, sizeof(position), "%d / %d", settingRow - tab.first + 1, tab.end - tab.first);
    label(l + w - ImGui::CalcTextSize(position).x - 22, t + 304, position);
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
  } else if (settingRow == shortcutSetting || settingRow == controllerCheckSetting ||
             settingRow == clutchSetting || settingRow == manualUpdateSetting) {
    char hint[128];
    std::snprintf(hint, sizeof(hint), text("D-pad: seç   %s: aç   %s: iptal", "D-pad: select   %s: open   %s: cancel"),
                  buttonCaption(PadA), buttonCaption(PadB));
    label(l + 22, t + 342, hint);
    if (settingRow == clutchSetting)
      label(l + 22, t + 374, clutchAvailable
          ? text("LFS'deki debriyaj ayarını açar.", "Opens LFS's clutch setting.")
          : text("Uygun araçla piste çıkınca durdurma menüsünden aç.",
                 "Open from the pause menu while in a compatible car."));
    if (settingRow == manualUpdateSetting)
      label(l + 22, t + 374, updateView == UpdateView::Offer
          ? text("Güncelleme hazır. Kurmak için ana menüye dön.", "Update available. Return to the main menu to install.")
          : text("GitHub ön sürümleri de denetlenir.", "Includes GitHub pre-releases."));
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
    else if (settingRow == steeringSetting)
      std::snprintf(hint, sizeof(hint), "%s", text("Küçük analog hareketleri yumuşatır.",
                                                  "Softens small left-stick movements."));
    else if (settingRow == steeringHelpSetting)
      std::snprintf(hint, sizeof(hint), "%s", text("Virajda fazla dönüşü azaltır, kaymada destek olur.",
                                                  "Reduces excess steering and helps correct slides."));
    else if (settingRow == profileSetting)
      std::snprintf(hint, sizeof(hint), text("%s: yeniden adlandır", "%s: rename"), buttonCaption(PadX));
    else if (settingRow == autoUpdateSetting)
      std::snprintf(hint, sizeof(hint), "%s", text("Yeni sürüm bulunduğunda yüklemeden önce sorar.",
                                                  "Asks before downloading and installing."));
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
static void drawControllerCheck(float l, float t, float w, const ControllerCheck &check,
                                const XINPUT_GAMEPAD &pad, bool connected, ControllerId slot) {
  panel(l, t, l + w, t + settingsHeight);
  label(l + 22, t + 16, text("Kontrolcümü test et", "Test my controller"));
  char xinput[20]{};
  if (connected && !isNativeController(slot))
    std::snprintf(xinput, sizeof(xinput), "XInput %d", int(slot) + 1);
  const char *source = !connected ? text("Kontrolcü bağlı değil", "No controller connected")
      : playStationCaptions ? "PlayStation"
      : isNativeController(slot) ? text("Yerel gamepad", "Native gamepad") : xinput;
  label(l + 22, t + 43, source, meterMuted);
  const char *leftTrigger = playStationCaptions ? "L2" : "LT";
  const char *rightTrigger = playStationCaptions ? "R2" : "RT";
  const char *steps[] = {
      buttonCaption(PadUp), buttonCaption(PadDown), buttonCaption(PadLeft), buttonCaption(PadRight),
      leftTrigger, rightTrigger, text("Düşük frekans titreşimi", "Low-frequency vibration"),
      text("Yüksek frekans titreşimi", "High-frequency vibration"),
  };
  const char *results[] = {
      "-", text("Geçti", "Passed"), text("Atlandı", "Skipped"),
      text("Hissedilmedi", "Not felt"), text("Komut başarısız", "Command failed"),
  };
  for (std::size_t i = 0; i < check.results.size(); ++i) {
    const float y = t + 76.f + float(i) * 23.f;
    const auto result = check.results[i];
    if (!check.interrupted && i == std::size_t(check.step))
      ImGui::GetForegroundDrawList()->AddRectFilled({l + 14, y - 2}, {l + w - 14, y + 19},
                                                     IM_COL32(35, 110, 94, 255), 4);
    const bool directionDown = connected && i <= std::size_t(CheckStep::Right) &&
                               (pad.wButtons & physical[PadUp + i]);
    label(l + 22, y, steps[i], directionDown ? meterActive : IM_COL32_WHITE);
    const auto *caption = results[std::size_t(result)];
    const auto colour = result == CheckResult::Passed ? meterActive
        : result == CheckResult::NoOutput || result == CheckResult::NotFelt ? meterWarning
                                                                           : meterMuted;
    label(l + w - 24 - ImGui::CalcTextSize(caption).x, y, caption, colour);
  }
  const float barWidth = (w - 68) * .5f;
  drawMeterAxis(l + 22, t + 270, barWidth, leftTrigger, pad.bLeftTrigger / 255.f, connected, false);
  drawMeterAxis(l + 46 + barWidth, t + 270, barWidth, rightTrigger, pad.bRightTrigger / 255.f,
                connected, false);
  const auto step = std::size_t(check.step);
  const auto *confirm = buttonCaption(PadA);
  const auto *skip = buttonCaption(PadX);
  char prompt[160]{};
  if (check.complete() || check.interrupted) {
    label(l + 22, t + 312, check.complete() ? text("Test tamamlandı", "Test complete")
                                            : text("Test durdu", "Test interrupted"));
    if (connected)
      std::snprintf(prompt, sizeof(prompt), text("%s ile yeniden başlat", "%s to restart"), confirm);
  } else if (check.step <= CheckStep::Right) {
    std::snprintf(prompt, sizeof(prompt), text("Yönleri ve tetikleri bırak, sonra %s yönüne bas.",
                                              "Release the triggers and D-pad, then press %s."), steps[step]);
  } else if (!check.testingMotor()) {
    std::snprintf(prompt, sizeof(prompt), text("İki tetiği bırak, sonra yalnız %s tuşunu sonuna kadar sık.",
                                              "Release both triggers, then squeeze %s fully."), steps[step]);
  } else if (check.motor == MotorPhase::Pulsing) {
    std::snprintf(prompt, sizeof(prompt), "%s", text("Titreşim gönderiliyor", "Sending vibration"));
  } else if (check.motor == MotorPhase::Feedback) {
    std::snprintf(prompt, sizeof(prompt), text("Titreşimi hissettin mi?  %s Evet   %s Hayır",
                                              "Did you feel it?  %s Yes   %s No"), confirm, skip);
  } else {
    std::snprintf(prompt, sizeof(prompt), text("Kısa titreşim için %s tuşuna bas.",
                                              "Press %s for a short vibration."), confirm);
  }
  label(l + 22, t + 338, prompt);
  char footer[120];
  const bool canSkip = !check.complete() && !check.interrupted && check.motor == MotorPhase::Ready;
  if (canSkip)
    std::snprintf(footer, sizeof(footer), text("%s Atla   %s Geri", "%s Skip   %s Back"),
                  skip, buttonCaption(PadB));
  else
    std::snprintf(footer, sizeof(footer), text("%s Geri", "%s Back"), buttonCaption(PadB));
  label(l + 22, t + 377, footer, meterMuted);
}

static void drawUpdate(float width, float height) {
  const float w = std::min(width - 24, 570.f), h = 194;
  const float l = (width - w) / 2, t = (height - h) / 2;
  auto *draw = ImGui::GetForegroundDrawList();
  draw->AddRectFilled({0, 0}, {width, height}, IM_COL32(0, 0, 0, 105));
  panel(l, t, l + w, t + h);
  overlayShield = hitRect({0, 0}, {width, height});
  label(l + 22, t + 16, text("LfsPad güncellemesi", "LfsPad update"));
  draw->AddLine({l + 22, t + 47}, {l + w - 22, t + 47}, IM_COL32(255, 181, 79, 255), 2);
  const bool offer = updateView == UpdateView::Offer;
  if (offer) {
    const auto version = std::string("LfsPad ") + updateVersion +
        text(" yüklenebilir.", " is available.");
    label(l + 22, t + 62, version.c_str());
    label(l + 22, t + 91, text("Oyun kurulumdan sonra yeniden açılacak.",
                               "LFS will reopen after installation."));
  } else {
    const char *message = updateView == UpdateView::Checking
        ? text("GitHub denetleniyor...", "Checking GitHub...")
        : updateView == UpdateView::Downloading
        ? text("Paket indiriliyor ve doğrulanıyor...", "Downloading and verifying the package...")
        : updateView == UpdateView::Current
        ? text("LfsPad güncel.", "LfsPad is up to date.")
        : text("Güncelleme tamamlanamadı. Tekrar dene.", "The update could not finish. Try again.");
    label(l + 22, t + 63, message);
  }
  if (updateView == UpdateView::Checking || updateView == UpdateView::Downloading)
    return;
  const auto button = [&](int slot, float x, const char *caption, bool focus, OverlayClick action) {
    const ImVec2 a{x, t + 136}, b{x + 122, t + 172};
    overlayTargets[slot] = {hitRect(a, b), action};
    draw->AddRectFilled(a, b, focus ? IM_COL32(36, 104, 90, 255) : IM_COL32(47, 55, 65, 255), 4);
    label(x + (122 - ImGui::CalcTextSize(caption).x) / 2, t + 142, caption);
  };
  button(0, l + w - (offer ? 278 : 144), text("Tamam", "OK"),
         !offer || updateAccept, OverlayClick::UpdateAccept);
  if (offer)
    button(1, l + w - 144, text("Daha sonra", "Later"), !updateAccept, OverlayClick::UpdateLater);
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
  if (cfg.autoUpdate)
    startUpdate(false, true);
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
  const bool wasGarageRoot = garageRoot, wasGarageViewer = garageViewer;
  const bool recoveringGarageFocus = garageSkins || garageModal || wasGarageRoot;
  const bool raceMenu = gather(modal, sim.known && sim.mode == 3);
  const bool nativeText = nativeTextEntry(modsText || modal == uintptr_t(imageBase + 0x95ec20),
                                          sim.known, sim.flags);
  gameTextEntry = nativeText;
  if (garageRoot && !recoveringGarageFocus && !wasGarageViewer)
    garageReturnCallback = 0;
  const bool playingReplay = (replay.active || (sim.flags & 2)) &&
                             sim.mode == 0 && !nativeText && !raceMenu &&
                             !(sim.flags & (8 | 16 | 256 | 32768));
  const bool menu = !playingReplay &&
                    menuCapture(sim.known, sim.flags, sim.mode,
                                nativeText || raceMenu || mainMenuInitial >= 0 || optionsInitial >= 0 ||
                                garageRoot || garageViewer || garageSkins);
  const bool shortcutEditorWasOpen = shortcutEditor;
  if (!menu && settings) {
    loadConfig();
    settings = false;
  }
  bool foreground = GetForegroundWindow() == window;
  const auto clicked = foreground ? overlayClick : OverlayClick::None;
  overlayClick = OverlayClick::None;
  overlayTargets = {};
  overlayShield = {};
  const bool safeUpdateMenu = foreground && sim.known && menu && !nativeText &&
      !(sim.flags & (1 | 2 | 512)) && (mainMenuInitial >= 0 || optionsInitial >= 0);
  pollUpdate(safeUpdateMenu, GetTickCount64());
  const bool updateModal = foreground && menu && !nativeText &&
      ((safeUpdateMenu && (updateView == UpdateView::Offer || updateView == UpdateView::Failed)) ||
       updateView == UpdateView::Downloading ||
       (settings && !updateAutomatic && (updateView == UpdateView::Checking ||
         updateView == UpdateView::Current || updateView == UpdateView::Failed)));
  static bool wasUpdateModal = false;
  if (updateModal != wasUpdateModal)
    blockMenuInput();
  wasUpdateModal = updateModal;
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
  const bool optionsEntryAvailable = menu && foreground && !nativeText && !rfsCursor &&
      optionsInitial >= 0 && validMenuRect(optionsExtension);
  if (!optionsEntryAvailable || reset)
    optionsEntryFocus = false;
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
  const auto &pad = state.Gamepad;
  const bool fromOptions = !settings && !keyboard && !cursorMode && !nativeButtonAssignment &&
      optionsEntryAvailable && (clicked == OverlayClick::OptionsTab ||
           (optionsEntryFocus && confirm && !back && !menuPressed));
  static ControllerCheck controllerCheck;
  static bool controllerCheckOpen = false, controllerCheckDrain = false;
  static ControllerId checkedController = -1;
  static uintptr_t checkedModal = 0;
  if (!settings || !menu)
    controllerCheckOpen = false;
  if (controllerCheckOpen && !controllerCheck.complete() &&
      (!foreground || lostAssignmentFocus || !connected || contextChanged ||
       activeSlot != checkedController || modal != checkedModal))
    controllerCheck.interrupt();
  const auto beginControllerCheck = [&] {
    controllerCheck = {};
    checkedController = activeSlot;
    checkedModal = modal;
    controllerCheckOpen = true;
    if (!connected)
      controllerCheck.interrupt();
    blockMenuInput();
  };
  static SettingsTriggerChord settingsChord;
  static double diagnosticTriggersUntil = 0;
  const bool settingsChordAllowed = menu && foreground && !contextChanged &&
      !lostAssignmentFocus && (connected || cfg.diagnostics) && !nativeText && !keyboard &&
      !garageViewer && !adjustingSlider && !cursorDragging && !bindingCapture.active() &&
      !nativeButtonAssignment && !controllerCheckOpen && !profileNaming &&
      (!pedalCalibration || settings);
  if (!settingsChordAllowed)
    diagnosticTriggersUntil = 0;
  else if (diagnosticSettings)
    diagnosticTriggersUntil = time + SettingsTriggerChord::duration + .18;
  constexpr auto rfsChord = XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_RIGHT_SHOULDER;
  const bool rfsAvailable = GetModuleHandleW(L"rfs.dll") != nullptr;
  const bool rfsAllowed = rfsAvailable && foreground && menu && (connected || cfg.diagnostics) &&
                          !settings && !keyboard && !nativeText && !nativeButtonAssignment &&
                          !pedalCalibration && !garageViewer && !lostAssignmentFocus && activeSlot == oldSlot;
  const bool rfsChordPressed = ((held & rfsChord) == rfsChord && (pressed & rfsChord)) ||
                               (diagnostic & (1u << 19));
  static bool rfsDrain = false, rfsStickReady = false;
  const bool wasRfsCursor = rfsCursor;
  if (!rfsAllowed || (rfsCursor && (back || menuPressed)))
    rfsCursor = false;
  else if (rfsChordPressed)
    rfsCursor = !rfsCursor;
  const bool rfsChanged = wasRfsCursor != rfsCursor;
  if (rfsChanged) {
    releaseMouse();
    blockMenuInput();
    cursorMode = false;
    rfsStickReady = false;
    rfsDrain = true;
    log("RFS menu cursor: %s", rfsCursor ? "open" : "closed");
  }
  const float rfsX = analog(pad.sThumbRX, cfg.deadzone);
  const float rfsY = analog(pad.sThumbRY, cfg.deadzone);
  if (!connected || (foreground && !rfsCursor && !buttons &&
                     pad.bLeftTrigger <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD &&
                     pad.bRightTrigger <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD && !lx && !ly && !rfsX && !rfsY))
    rfsDrain = false;
  const bool diagnosticTriggers = time < diagnosticTriggersUntil;
  const bool toggleSettings = settingsChord.update(
      diagnosticTriggers ? 255 : pad.bLeftTrigger,
      diagnosticTriggers ? 255 : pad.bRightTrigger, time,
      settingsChordAllowed && !rfsCursor && !rfsChanged);
  const auto shortcutScreen = menu && !nativeText ? findShortcutScreen(modal, raceMenu)
                                             : ShortcutScreen::None;
  const auto menuShortcuts = findMenuShortcuts(shortcutScreen, modal);
  const bool hasMenuShortcuts =
      std::any_of(menuShortcuts.begin(), menuShortcuts.end(),
                  [](const ResolvedShortcut &shortcut) { return shortcut.label != nullptr; });
  const bool shortcutContext = menu && foreground && (connected || cfg.diagnostics) &&
      !lostAssignmentFocus && !nativeText && !settings && !keyboard && !nativeButtonAssignment &&
      !rfsCursor && !rfsChanged && !cursorMode && !adjustingSlider && !menuPressed &&
      !toggleSettings && !diagnosticKeyboard && !(pressed & (binding(CursorMode) | binding(Keyboard)));
  const bool canUseMenuShortcuts = shortcutContext && hasMenuShortcuts && !reset;
  const auto modifier = binding(SettingsModifier);
  static ShortcutGesture shortcutGesture;
  static bool diagnosticMenuShortcuts = false;
  static Favorite pendingFavorite = Favorite::None;
  static ShortcutScreen favoriteSource = ShortcutScreen::None;
  static uintptr_t favoriteOwner = 0;
  static FavoriteRoute favoriteRoute;
  static double favoriteErrorUntil = 0;
  static Favorite failedFavorite = Favorite::None;
  const bool clutchSession = sim.known && (sim.flags & 1) && !(sim.flags & 2) &&
                            !replay.active && sim.localPlayer && sim.localPlayer == sim.viewedPlayer;
  if (favoriteRoute.favorite != Favorite::None &&
      (!shortcutContext || contextChanged || (favoriteRoute.favorite == Favorite::Clutch && !clutchSession))) {
    releaseMouse();
    favoriteRoute.cancel();
    blockMenuInput();
  }
  if (!canUseMenuShortcuts || shortcutScreen != favoriteSource || modal != favoriteOwner)
    pendingFavorite = Favorite::None;
  const bool cancelShortcut = back && (shortcutGesture.waiting || (held & modifier) ||
                                      diagnosticMenuShortcuts);
  if (cancelShortcut) {
    pendingFavorite = Favorite::None;
    blockMenuInput();
  }
  if (!canUseMenuShortcuts || cancelShortcut)
    diagnosticMenuShortcuts = false;
  else if (diagnostic & (1u << 18))
    diagnosticMenuShortcuts = !diagnosticMenuShortcuts;
  const bool otherShortcutInput = (buttons & ~modifier) != 0 ||
      lx != 0.f || ly != 0.f || rfsX != 0.f || rfsY != 0.f ||
      pad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD ||
      pad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
  const bool physicalShortcutPanel = shortcutGesture.update(
      canUseMenuShortcuts && favoriteRoute.favorite == Favorite::None, cfg.shortcutSequential,
      (held & modifier) != 0, (pressed & modifier) != 0, otherShortcutInput, cancelShortcut);
  bool menuShortcutsOpen = physicalShortcutPanel || diagnosticMenuShortcuts;
  const bool shortcutCaptured = menuShortcutsOpen || cancelShortcut ||
      pendingFavorite != Favorite::None || favoriteRoute.favorite != Favorite::None;
  static ShortcutInputDrain shortcutInput;
  shortcutInput.update(shortcutEditorWasOpen, shortcutCaptured, lx != 0.f || ly != 0.f,
                       buttons & (XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_B |
                                  XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_START));
  bool finishHint = false;
  const auto session = raceSession(sim.known, sim.flags, replay.active,
                                   replay.confirmedAbsent);
  const bool onTrack = sim.known && sim.mode == 0 &&
                       session != RaceSession::Outside && (sim.flags & 1);
  const auto clutchPage = settings && clutchSession && (raceMenu || shortcutScreen == ShortcutScreen::Options)
      ? favoritePage(Favorite::Clutch, shortcutScreen, modal, shortcutScreen, modal)
      : FavoritePage::Other;
  const bool clutchAvailable = clutchPage == FavoritePage::Driver ? nativeClutchItem(modal) >= 0
      : favoriteItem(nextFavoritePage(Favorite::Clutch, clutchPage), shortcutScreen, modal) >= 0;
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
      quickMasks[cfg.quickButton] != binding(Menu) &&
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
  // LFS can bind these XUSB/PlayStation buttons to its own Escape action.
  // Consume Back and Start before they reach LFS so only our Menu action can
  // request the race menu. The driving-assignment screen still sees raw input.
  constexpr unsigned nativeMenuButtons = (1u << 6) | (1u << 7);
  const bool checkTriggersHeld = pad.bLeftTrigger > ControllerCheck::neutralTrigger ||
                                 pad.bRightTrigger > ControllerCheck::neutralTrigger;
  controllerCheckDrain = controllerCheckOpen || (controllerCheckDrain && checkTriggersHeld);
  const auto nativeMenuFilter = nativeButtonAssignment && !settings ? 0u : nativeMenuButtons;
  const auto quickFilter = (quickAllowed ? quickButtonBits[cfg.quickButton] : 0u) |
                           (quickDrain ? rightStickMask : 0u) |
                           ((settingsChord.draining || controllerCheckDrain) ? triggerAxisMask : 0u);
  captureQuickInput.store(connected && foreground
      ? nativeMenuFilter | (singlePad ? quickFilter | viewerMask |
          (shortcutInput.stickHeld ? leftStickMask : 0u) : 0u)
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
  } else if (updateModal && foreground) {
    if (updateView == UpdateView::Offer) {
      if (nav[2] || nav[3])
        updateAccept = nav[2];
      if (back || menuPressed || clicked == OverlayClick::UpdateLater ||
          (confirm && !updateAccept)) {
        updateView = UpdateView::Idle;
        blockMenuInput();
      } else if (clicked == OverlayClick::UpdateAccept || (confirm && updateAccept)) {
        if (!startUpdate(true, updateAutomatic)) {
          updateAutomatic = false;
          updateView = UpdateView::Failed;
        }
        blockMenuInput();
      }
    } else if ((updateView == UpdateView::Failed || updateView == UpdateView::Current) &&
               (confirm || back || menuPressed || clicked == OverlayClick::UpdateAccept)) {
      updateView = UpdateView::Idle;
      blockMenuInput();
    }
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
  } else if (rfsCursor || rfsChanged) {
    if (rfsCursor && !rfsChanged && GetForegroundWindow() == window) {
      if (!rfsX && !rfsY)
        rfsStickReady = true;
      const auto p = moveMenuCursor(rfsStickReady ? rfsX : 0, rfsStickReady ? rfsY : 0,
                                    width, height, dt);
      if (confirm && !mouseDown)
        clickAt(float(p.x), float(p.y), false, (buttons & binding(Confirm)) != 0);
      if (cursorDragging && !(buttons & binding(Confirm)))
        releaseMouse();
      if (pressed & binding(Secondary))
        clickAt(float(p.x), float(p.y), true);
      if (nav[0] != nav[1])
        wheel(nav[0] ? WHEEL_DELTA : -WHEEL_DELTA);
    }
  } else if (foreground && !recoveryConsumesConfirm) {
    if (toggleSettings || fromOptions) {
      if (settings)
        loadConfig();
      settings = !settings;
      if (fromOptions) {
        settingsTab = 0;
        settingRow = settingsTabs[0].first;
        optionsEntryFocus = true;
      }
      blockMenuInput();
      if (cfg.diagnostics)
        log("Settings trigger chord: %s", settings ? "open" : "closed");
    } else if (menuPressed) {
      if (profileNaming) {
        finishProfileName(true);
      } else if (controllerCheckOpen) {
        controllerCheckOpen = false;
        blockMenuInput();
      } else if (keyboard) {
        closeGameKeyboard(true);
      } else if (settings) {
        loadConfig();
        settings = false;
        blockMenuInput();
        confirm = back = false;
        std::fill(std::begin(nav), std::end(nav), false);
      } else
        simKey("esc");
    }
    static bool wasNativeText = false;
    static uintptr_t keyboardOwner = 0;
    if (!nativeText || !wasNativeText || modal != keyboardOwner) {
      dismissedText = false;
      if (nativeText) {
        gameKeys = {};
        gameKeys.row = 1;
        manualKeyboard = false;
        keyboard = true;
        confirm = back = false;
        blockMenuInput();
      }
    }
    wasNativeText = nativeText;
    keyboardOwner = modal;
    if (menu && !fromOptions && !toggleSettings && !menuPressed &&
        ((pressed & binding(Keyboard)) || diagnosticKeyboard) &&
        !settings) {
      if (keyboard && !diagnosticKeyboard)
        gameKeys.upper = !gameKeys.upper;
      else {
        keyboard = !keyboard;
        manualKeyboard = keyboard;
        dismissedText = !keyboard;
        blockMenuInput();
      }
    }
    if (nativeText && !manualKeyboard && !settings)
      keyboard = !dismissedText && (!menuPressed || keyboard);
    if (!nativeText && !manualKeyboard)
      keyboard = false;
    if (!menu) {
      settings = keyboard = manualKeyboard = profileNaming = false;
      cursorMode = false;
    }
    if (menu && !fromOptions && !toggleSettings && !menuPressed) {
      if (keyboard) {
        moveKeyboard(gameKeys, nav, diagnosticNext);
        if (selectClickedKey(gameKeys, clicked) || confirm)
          keyAction();
        if (pressed & binding(Secondary))
          localKey(VK_BACK);
        if (back)
          closeGameKeyboard(false);
      } else if (settings && profileNaming) {
        if (back) {
          finishProfileName(false);
        } else {
          moveKeyboard(profileKeys, nav, diagnosticNext);
          if ((pressed & XINPUT_GAMEPAD_X) || diagnosticAssign) {
            if (!profileNameDraft.empty())
              profileNameDraft.pop_back();
          } else if ((pressed & XINPUT_GAMEPAD_Y) || diagnosticKeyboard) {
            profileKeys.upper = !profileKeys.upper;
          } else if (selectClickedKey(profileKeys, clicked) || confirm) {
            keyAction();
          }
        }
      } else if (settings && controllerCheckOpen) {
        if (back) {
          controllerCheckOpen = false;
          blockMenuInput();
        } else if (controllerCheck.interrupted || controllerCheck.complete()) {
          if (confirm && connected)
            beginControllerCheck();
        } else {
          controllerCheck.update(buttons & dpad, pad.bLeftTrigger, pad.bRightTrigger,
              confirm, (pressed & XINPUT_GAMEPAD_X) || diagnosticAssign, now);
        }
      } else if (settings && shortcutEditor) {
        if (back || confirm) {
          if (!back) {
            cfg.shortcuts = shortcutEditDraft;
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
        const int clickedTab = int(clicked) - int(OverlayClick::SettingsTabFirst);
        const int clickedRow = int(clicked) - int(OverlayClick::SettingsRowFirst);
        const auto &currentTab = settingsTabs[settingsTab];
        const bool previousTab = (pressed & XINPUT_GAMEPAD_LEFT_SHOULDER) || (diagnostic & (1u << 10));
        const bool nextTab = (pressed & XINPUT_GAMEPAD_RIGHT_SHOULDER) || (diagnostic & (1u << 11));
        if (clickedTab >= 0 && clickedTab < int(std::size(settingsTabs))) {
          settingsTab = clickedTab;
          settingRow = settingsTabs[settingsTab].first;
          blockMenuInput();
        } else if (clickedRow >= currentTab.first && clickedRow < currentTab.end) {
          settingRow = clickedRow;
          confirm = settingRow == manualUpdateSetting || settingRow == controllerCheckSetting ||
                    settingRow == shortcutSetting || settingRow == clutchSetting;
          blockMenuInput();
        } else if (previousTab != nextTab) {
          constexpr int count = int(std::size(settingsTabs));
          settingsTab = (settingsTab + count + (nextTab ? 1 : -1)) % count;
          settingRow = settingsTabs[settingsTab].first;
          vibrationPreviewResult = ERROR_CANCELLED;
          blockMenuInput();
        } else {
          const auto &tab = settingsTabs[settingsTab];
          if (nav[0])
            settingRow = settingRow == tab.first ? tab.end - 1 : settingRow - 1;
          if (nav[1] || diagnosticNext)
            settingRow = settingRow == tab.end - 1 ? tab.first : settingRow + 1;
          if (nav[2] != nav[3])
            adjustSetting(nav[2] ? -1 : 1);
        }
        if (back) {
          loadConfig();
          settings = false;
          blockMenuInput();
        } else if (settingRow == profileSetting && ((pressed & XINPUT_GAMEPAD_X) || diagnosticAssign)) {
          profileNameDraft = profiles.current().name;
          profileKeys = {};
          profileKeys.row = 1;
          profileNaming = true;
          blockMenuInput();
        } else if (settingRow == controllerCheckSetting && confirm) {
          rumblePreviewUntil = 0;
          beginControllerCheck();
        } else if (settingRow == manualUpdateSetting && confirm) {
          if (updateView != UpdateView::Offer && !updateWorker && !startUpdate(false, false)) {
            updateAutomatic = false;
            updateView = UpdateView::Failed;
          }
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
        } else if (confirm && (settingRow != clutchSetting || clutchAvailable)) {
          XINPUT_STATE candidate{};
          missingSlot = cfg.slot >= 0 &&
                        readController(controllerForPreference(cfg.slot, activeSlot),
                                       candidate) != ERROR_SUCCESS;
          if (!missingSlot) {
            settingsSaveFailed = !saveConfig();
            if (!settingsSaveFailed) {
              if (settingRow == clutchSetting) {
                favoriteSource = shortcutScreen;
                favoriteOwner = modal;
                favoriteErrorUntil = 0;
                favoriteRoute.begin(Favorite::Clutch, clutchPage, now);
              }
              if (inputSlot != cfg.slot || settingRow != clutchSetting)
                activeSlot = -1;
              inputSlot = cfg.slot;
              settings = false;
              blockMenuInput();
            }
          }
        }
      } else if (optionsEntryAvailable && !cursorMode &&
                 (optionsEntryFocus || (nav[1] && selected == optionsLast))) {
        if (back) {
          optionsEntryFocus = false;
          simKey("esc");
        } else if (optionsEntryFocus && (nav[0] || nav[3] ||
                   (pressed & (binding(PreviousTab) | binding(NextTab) | binding(CursorMode))))) {
          optionsEntryFocus = false;
          if (nav[3])
            selected = optionsInitial;
        } else
          optionsEntryFocus = true;
        blockMenuInput();
      } else if (favoriteRoute.favorite != Favorite::None) {
        if (back) {
          releaseMouse();
          favoriteRoute.cancel();
        } else {
          const auto favorite = favoriteRoute.favorite;
          const auto page = favoritePage(favorite, favoriteSource, favoriteOwner, shortcutScreen, modal);
          const int item = favoriteItem(nextFavoritePage(favorite, page),
                                        favoriteSource, modal);
          bool failed = false;
          const auto action = favoriteRoute.update(page, item >= 0 && !mouseDown, now, failed);
          if (action != FavoritePage::Other) {
            if (cfg.diagnostics)
              log("Favorite transition: %u -> %u", unsigned(page), unsigned(action));
            activate(items[item], width, height);
          } else if (failed) {
            releaseMouse();
            failedFavorite = favorite;
            favoriteErrorUntil = time + 2.;
            if (cfg.diagnostics)
              log("Favorite stopped at page %u", unsigned(page));
          } else if (favoriteRoute.favorite == Favorite::None) {
            if (favorite == Favorite::Clutch) {
              const int clutch = nativeClutchItem(modal);
              if (clutch >= 0)
                selected = clutch;
              else {
                failedFavorite = favorite;
                favoriteErrorUntil = time + 2.;
              }
            }
            if (cfg.diagnostics)
              log("Favorite reached page %u", unsigned(page));
          }
        }
        blockMenuInput();
      } else if (cancelShortcut) {
        blockMenuInput();
      } else if (menuShortcutsOpen) {
        const auto shortcutDirections = dpad & ~(modifier | binding(Back));
        auto directionalPress = pressed & shortcutDirections;
        auto directionalHeld = buttons & shortcutDirections;
        if (diagnosticMenuShortcuts) {
          directionalPress |= diagnostic & dpad;
          directionalHeld |= diagnostic & dpad;
        }
        if (!(directionalHeld | directionalPress)) {
          const bool stickDirections[]{ly > .55f, ly < -.55f, lx < -.55f, lx > .55f};
          for (int i = 0; stickArmed && i < 4; ++i)
            if (stickDirections[i]) {
              directionalPress |= physical[PadUp + i];
              directionalHeld |= physical[PadUp + i];
            }
        }
        const int direction = menuShortcutDirection(directionalHeld, directionalPress);
        if (direction >= 0) {
          const auto &target = menuShortcuts[direction];
          pendingFavorite = Favorite::None;
          if (target.item >= 0) {
            if (target.favorite == Favorite::None) {
              selected = target.item;
              scrollTarget = -1;
              restoreAfterScroll = false;
            } else {
              pendingFavorite = target.favorite;
              favoriteSource = shortcutScreen;
              favoriteOwner = modal;
              favoriteErrorUntil = 0;
              diagnosticMenuShortcuts = false;
            }
            if (shortcutGesture.waiting) {
              shortcutGesture.waiting = false;
              menuShortcutsOpen = diagnosticMenuShortcuts;
            }
          }
        }
        // A direction or Confirm held at the end of the chord must not
        // navigate or activate a second target when the modifier is released.
        gate.blocked |= buttons & (dpad | binding(Confirm)) & ~modifier;
        stickArmed = false;
        for (auto &directionRepeat : repeat)
          directionRepeat = {};
      } else if (pendingFavorite != Favorite::None) {
        if (back || confirm) {
          if (confirm && !back && !mouseDown) {
            const auto page = favoritePage(pendingFavorite, favoriteSource, favoriteOwner,
                                           shortcutScreen, modal);
            if (favoriteItem(nextFavoritePage(pendingFavorite, page), favoriteSource, modal) >= 0)
              favoriteRoute.begin(pendingFavorite, page, now);
            else {
              failedFavorite = pendingFavorite;
              favoriteErrorUntil = time + 2.;
            }
          }
          pendingFavorite = Favorite::None;
          blockMenuInput();
        } else if (std::any_of(std::begin(nav), std::end(nav), [](bool move) { return move; }) ||
                   (pressed & (binding(PreviousTab) | binding(NextTab)))) {
          pendingFavorite = Favorite::None;
          blockMenuInput();
        }
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
          const auto p = moveMenuCursor(rx, ry, width, height, dt);
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
        if (back && escapeFromBack(menu, raceMenu || (sim.flags & 16), onTrack))
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
                       (menu || playingReplay || recoveryCapture || draining || rfsDrain));
  captureRaceAxes.store(foreground && connected && (raceMenu || recoveryCapture || rfsCursor || rfsDrain));
  const bool settingsVisible = foreground && menu && settings;
  static DrivingRumble rumble;
  static bool hadTelemetry = false;
  static bool hadWheelTelemetry = false;
  static ControllerId lastRumbleController = -1;
  const bool localDriving = foreground && onTrack && !menu && !settings && !keyboard && !nativeText && !playingReplay &&
      !recoveryConsumesConfirm && !menuPressed && !lostAssignmentFocus &&
      !(sim.flags & (2 | 4 | 8 | 16 | 256 | 32768)) &&
      sim.localPlayer && sim.localPlayer == sim.viewedPlayer;
  const bool canRumbleWhileDriving = localDriving && cfg.rumbleStrength;
  const bool steeringAllowed = localDriving && singlePad && !rfsCursor && !rfsDrain;
  const bool assistDriving = steeringAllowed && cfg.steeringHelp != SteeringHelp::Off;
  static bool steeringArmed = false;
  if (!steeringAllowed || contextChanged)
    steeringArmed = false;
  if (steeringAllowed && std::abs(int(pad.sThumbLX)) <= 1024)
    steeringArmed = true;
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
  const auto inSimTelemetry = pollInSimCarTelemetry(rumbleDriving || assistDriving);
  auto outSimTelemetry = rumbleDriving || assistDriving ? drivingTelemetry() : DrivingTelemetry{};
  const auto tick = GetTickCount64();
  static unsigned telemetryOwner = 0;
  static ULONGLONG ownDrivingStarted = 0;
  const unsigned currentOwner = localDriving ? sim.localPlayer : 0;
  if (currentOwner != telemetryOwner) {
    telemetryOwner = currentOwner;
    ownDrivingStarted = tick;
  }
  // OutSim follows the camera in single player. Wait for a new sample when
  // returning to our own car so we never steer using another driver's data.
  if (outSimTelemetry.received <= ownDrivingStarted)
    outSimTelemetry = {};
  const auto telemetry = selectDrivingTelemetry(outSimTelemetry, inSimTelemetry, tick);
  const bool fresh = telemetry.fresh(tick);
  static SteeringHelpState steeringHelp;
  static unsigned steeringPlayer = 0;
  if (steeringPlayer != sim.localPlayer || session == RaceSession::Outside ||
      shortcutScreen == ShortcutScreen::Options || oldSlot != activeSlot || !connected ||
      cfg.steeringHelp == SteeringHelp::Off) {
    steeringHelp = {};
    steeringPlayer = sim.localPlayer;
  }
  const bool learnSteering = assistDriving && !steeringHelp.polarity;
  const auto input = learnSteering ? drivingInputSample(tick - std::min<ULONGLONG>(tick, 80))
                                  : DrivingInputSample{};
  const double axis = input.steeringAvailable && !input.axesFiltered ? input.rawSteering : 0.;
  const auto curve = steeringArmed ? cfg.steeringCurve : SteeringCurve::Linear;
  if (!assistDriving)
    steeringHelp.suspend();
  const auto response = assistDriving
      ? steeringHelp.update(cfg.steeringHelp, curve, telemetry.sample, fresh, axis, dt)
      : SteeringResponse{curve};
  const auto steering = steeringAllowed && GetForegroundWindow() == window
      ? steeringRequest(response, !native, nativeVidPid) : 0;
  const auto previousSteering = steeringAssist.exchange(steering);
  if (cfg.diagnostics && (previousSteering & 3) != (steering & 3))
    log("Driving steering curve: %u", unsigned(steering & 3));
  if (cfg.diagnostics) {
    static SteeringHelp reportedMode = SteeringHelp::Off;
    static bool reportedFresh = false;
    static int reportedPolarity = 0;
    const auto mode = assistDriving ? cfg.steeringHelp : SteeringHelp::Off;
    if (mode != reportedMode || (assistDriving &&
        (reportedFresh != fresh || reportedPolarity != steeringHelp.polarity))) {
      log("Steering assist: mode=%u range=%.1f%% source=%s polarity=%d", unsigned(mode),
          response.limit / 10., assistDriving && fresh ? (telemetry.sample.hasWheels ? "OutSim" : "InSim") : "none",
          steeringHelp.polarity);
      reportedMode = mode;
    }
    reportedFresh = fresh;
    reportedPolarity = steeringHelp.polarity;
  }
  if (rumbleDriving) {
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
    if (controllerCheckOpen && controllerCheck.motor == MotorPhase::Pulsing &&
        foreground && !lostAssignmentFocus && connected && activeSlot == checkedController) {
      constexpr int strength = 60;
      const auto motors = controllerCheck.step == CheckStep::LowMotor
          ? motorLevels(1, 0, strength) : motorLevels(0, 1, strength);
      const auto result = driveMotors(checkedController, motors, window, cfg.diagnostics);
      controllerCheck.motorReply(result == ERROR_SUCCESS ? MotorReply::Accepted
          : result == ERROR_BUSY ? MotorReply::Busy : MotorReply::Rejected);
    } else if (preview && readController(rumblePreviewSlot, candidate) == ERROR_SUCCESS) {
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
  static bool wasCollectingInput = false;
  if (collectInput && !wasCollectingInput)
    meterSince = tick;
  wasCollectingInput = collectInput;
  monitorInput.store(collectInput || learnSteering);
  if (foreground && menu) {
    if (optionsEntryAvailable && !rfsCursor && !updateModal)
      drawOptionsEntry(optionsEntryFocus);
    if (updateModal)
      drawUpdate(width, height);
    else if (settings && controllerCheckOpen)
      drawControllerCheck(settingsX, settingsY, settingsW, controllerCheck, pad, connected, activeSlot);
    else if (settings && shortcutEditor)
      drawShortcutEditor(settingsX, settingsY, settingsW);
    else if (settings && profileNaming)
      drawKeyboard(width, height);
    else if (settings)
      drawSettings(settingsX, settingsY, settingsW, clutchAvailable);
    else if (keyboard)
      drawKeyboard(width, height);
    else if (rfsCursor) {
      if (cfg.controllerIcons) {
        drawCornerHints(width, height, false, {
            {PadRightStick, text("RFS imleci", "RFS cursor")},
            {cfg.bindings[Confirm], text("Tıkla / sürükle", "Click / drag")}});
        drawCornerHints(width, height, true, {
            {PadDirections, text("Kaydır", "Scroll")},
            {cfg.bindings[Back], text("İmleci kapat", "Close cursor")}});
      }
    }
    else {
      if (!assigningDrivingButton && !cursorMode && !optionsEntryFocus && selected >= 0 &&
          pendingFavorite == Favorite::None && favoriteRoute.favorite == Favorite::None) {
        auto r = items[selected].rect;
        ImGui::GetForegroundDrawList()->AddRect(
            pixel(r.l, r.t), pixel(r.r, r.b), IM_COL32(65, 255, 196, 255), 3, 0,
            3);
      }
      if (menuShortcutsOpen && !assigningDrivingButton)
        drawMenuShortcuts(width, height, menuShortcuts, selected, pendingFavorite, shortcutGesture.waiting);
      else if (pendingFavorite != Favorite::None) {
        const auto &label = pendingFavorite == Favorite::Axes ? axesLabel : skinListLabel;
        drawCornerHints(width, height, false, {{cfg.bindings[Confirm], label.get(cfg.language)}});
        drawCornerHints(width, height, true, {{cfg.bindings[Back], text("İptal", "Cancel")}});
      } else if (favoriteRoute.favorite != Favorite::None) {
        const auto &label = favoriteRoute.favorite == Favorite::Axes ? axesLabel
            : favoriteRoute.favorite == Favorite::Clutch ? clutchLabel : skinListLabel;
        drawCornerHints(width, height, false, {{-1, label.get(cfg.language)}});
      } else if (time < favoriteErrorUntil) {
        const char *message = failedFavorite == Favorite::Clutch
            ? text("Otomatik debriyaj ayarı kullanılamıyor", "Auto clutch setting unavailable")
            : text("Favori açılamadı", "Couldn't open favorite");
        drawCornerHints(width, height, false, {{-1, message}});
      } else if (assigningDrivingButton) {
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
        const int modifierButton = cfg.bindings[SettingsModifier];
        const HudHint settingsHint{PadLT, text("Ayarlar için tut", "Hold for settings"), PadRT};
        if (hasMenuShortcuts && !cursorMode)
          drawCornerHints(width, height, true, {
              {modifierButton, cfg.shortcutSequential ? text("Bas, sonra yön seç", "Tap, then choose")
                                                       : text("Kısayollar", "Shortcuts"),
               modifierButton >= PadUp ? PadLeftStick : PadDirections},
              {cfg.bindings[Keyboard], text("Klavye", "Keyboard")}, settingsHint});
        else if (inList && !cursorMode)
          drawCornerHints(width, height, true, {
              {cfg.bindings[PreviousTab], text("Sayfa", "Page"), cfg.bindings[NextTab]},
              {cfg.bindings[Keyboard], text("Klavye", "Keyboard")}, settingsHint});
        else if (pedalCalibration)
          drawCornerHints(width, height, true, {{cfg.bindings[Keyboard], text("Klavye", "Keyboard")}});
        else if (rfsAvailable && optionsInitial >= 0)
          drawCornerHints(width, height, true, {
              {PadLB, text("RFS imleci", "RFS cursor"), PadRB}, settingsHint});
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
  auto *drawData = ImGui::GetDrawData();
  if (!drawData->TotalVtxCount)
    return;
  ID3D11RenderTargetView *old = nullptr;
  ID3D11DepthStencilView *oldDepth = nullptr;
  context->OMGetRenderTargets(1, &old, &oldDepth);
  context->OMSetRenderTargets(1, &target, nullptr);
  ImGui_ImplDX11_RenderDrawData(drawData);
  context->OMSetRenderTargets(1, &old, oldDepth);
  if (old)
    old->Release();
  if (oldDepth)
    oldDepth->Release();
}
} // namespace pad
