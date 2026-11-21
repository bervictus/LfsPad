#include "core.hpp"
#include "insim_packets.hpp"
#include <limits>
#include "rumble.hpp"
#include "shortcuts.hpp"
#include <cstdlib>
#include <iostream>
static int checks = 0;
void check(bool b, const char *label) {
  ++checks;
  if (!b) {
    std::cerr << label << '\n';
    std::exit(1);
  }
}
int main() {
  using namespace pad;
  check(parseLanguage(languageKey(Language::English)) == Language::English &&
            parseLanguage(L"") == Language::Turkish, "saved and legacy language settings");
  check(keyboardRows(Language::English, false, false)[1] == L"qwertyuiop" &&
            keyboardRows(Language::Turkish, true, false)[2] == L"ASDFGHJKLŞİ",
        "English and Turkish keyboard layouts");
  for (const auto &menu : shortcutMenus)
    for (int i = 0; i < menu.count; ++i)
      check(menu.targets[i].label.english && menu.targets[i].label.turkish,
            "shortcut labels available in both languages");
  const unsigned char stateDatagram[28]{7, 5, 1};
  const unsigned char batchedDatagram[]{1, 3, 1, 0, 2, 64, 1, 0, 3, 0, 0, 0};
  const unsigned char zeroPacket[]{0, 0, 0, 0};
  check(validInSimDatagram(stateDatagram, sizeof(stateDatagram)) &&
            validInSimDatagram(batchedDatagram, sizeof(batchedDatagram)),
        "complete InSim datagrams can contain one or more packets");
  check(!validInSimDatagram(zeroPacket, sizeof(zeroPacket)) &&
            !validInSimDatagram(stateDatagram, 0) &&
            !validInSimDatagram(stateDatagram, 2) &&
            !validInSimDatagram(batchedDatagram, sizeof(batchedDatagram) - 1),
        "reject empty and truncated datagrams without reusing their bytes");
  const auto invalidMotorLevels = motorLevels(
      std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), 100);
  check(!invalidMotorLevels.left && !invalidMotorLevels.right,
        "invalid rumble calculations do not reach the controller");
  check(pedalAxis(0, 0, 65535, false) == 0 &&
            pedalAxis(255, 0, 65535, false) == 65535,
        "each pedal reports zero released and full scale fully pressed");
  check(pedalAxis(96, 0, 65535, false) == 24672 &&
            pedalAxis(192, 0, 65535, false) == 49344,
        "simultaneous throttle and brake retain their independent levels");
  check(pedalAxis(255, -1000, 1000, true) == -1000 &&
            pedalAxis(160, 0, 65535, true) == 0,
        "suppressed pedals return the released endpoint");
  check(pedalAxis(255, INT32_MIN, INT32_MAX, false) == INT32_MAX,
        "pedal conversion uses a wide intermediate");
  constexpr std::array<std::wstring_view, 5> defaults{{
      L"solo,options,replays,online", L"garage,go,track,driver",
      L"setups,ok,colours,viewer", L"controls,ok,display,graphics",
      L"resume,pit,options,replay",
  }};
  ShortcutMappings mappings;
  for (std::size_t i = 0; i < shortcutMenus.size(); ++i) {
    const auto &menu = shortcutMenus[i];
    mappings[i] = parseShortcutDirections(menu, L"");
    check(formatShortcutDirections(menu, mappings[i]) == defaults[i],
          "missing game config uses the 0.2.9 directions for this menu");
    check(parseShortcutDirections(menu, defaults[i]) == mappings[i],
          "default direction records round-trip in every menu");
  }
  const auto garage = std::size_t(ShortcutScreen::Garage);
  const auto &garageMenu = shortcutMenus[garage];
  assignShortcut(mappings[garage], int(Direction::Right), 5);
  check(formatShortcutDirections(garageMenu, mappings[garage]) ==
            L"setups,ok,colours,skins" && mappings[0] == defaultShortcutDirections,
        "assigning a garage target leaves main menu directions alone");
  assignShortcut(mappings[garage], int(Direction::Up), 5);
  check(mappings[garage] == ShortcutDirections{5, 2, 3, 1},
        "assigning an occupied target swaps the direction pair");
  assignShortcut(mappings[garage], int(Direction::Down), 0);
  assignShortcut(mappings[garage], int(Direction::Left), 0);
  const auto garageRecord = formatShortcutDirections(garageMenu, mappings[garage]);
  check(garageRecord == L"skins,off,off,setups" &&
            parseShortcutDirections(garageMenu, garageRecord) == mappings[garage],
        "disabled directions persist independently");
  for (const auto *invalid : {L"skins,off,off", L"skins,off,off,off,off",
                              L"skins,,off,off", L"skins,off,off,skins",
                              L"training,off,off,off", L"skins,off,off,off,"})
    check(parseShortcutDirections(garageMenu, invalid) == defaultShortcutDirections,
          "invalid config record falls back for its own menu");
  const auto resolved = resolveShortcuts(garageMenu, mappings[garage],
      [](const ShortcutTarget &target) { return target.callback ? -1 : 23; });
  check(resolved[0].item == -1 && resolved[0].label &&
            resolved[1].item == -1 && !resolved[1].label && resolved[3].item == 23,
        "missing geometry retains a disabled label and never retargets a direction");
  const auto allDisabled = resolveShortcuts(garageMenu, {0, 0, 0, 0},
      [](const ShortcutTarget &) { return 23; });
  check(std::all_of(allDisabled.begin(), allDisabled.end(),
                    [](const ResolvedShortcut &slot) { return !slot.label && slot.item == -1; }),
        "four disabled directions do not create live targets");
  ShortcutInputDrain shortcutInput;
  shortcutInput.update(true, false, true, 3u);
  shortcutInput.update(false, false, true, 2u);
  check(shortcutInput.stickHeld && shortcutInput.buttonsHeld == 2,
        "shortcut editor holds consumed stick and remaining buttons through screen exit");
  shortcutInput.update(false, false, false, 0u);
  shortcutInput.update(false, false, true, 3u);
  check(!shortcutInput.stickHeld && !shortcutInput.buttonsHeld,
        "fresh driving input passes through after release");
  shortcutInput.update(false, true, true, 1u);
  check(shortcutInput.stickHeld && !shortcutInput.buttonsHeld,
        "holding the shortcut panel captures stick without borrowing settings buttons");
  SettingsTriggerChord settingsChord;
  check(!settingsChord.update(255, 255, 0, false) &&
            !settingsChord.update(255, 255, 1, true) &&
            !settingsChord.update(255, 255, 2, true),
        "held driving triggers cannot open settings when a menu appears");
  settingsChord.update(0, 0, 3, true);
  check(!settingsChord.update(255, 0, 4, true) &&
            !settingsChord.update(255, 0, 5, true),
        "one trigger cannot open settings");
  check(!settingsChord.update(255, 255, 6, true) &&
            !settingsChord.update(255, 255, 6.2, true),
        "brief dual-trigger press does not open settings");
  check(!settingsChord.update(255, 50, 6.3, true) &&
            !settingsChord.update(255, 255, 6.4, true) &&
            settingsChord.update(255, 255, 6.8, true),
        "the chord requires both triggers held for the whole interval");
  check(!settingsChord.update(255, 255, 7.5, true) && settingsChord.draining,
        "a held chord toggles settings just once");
  check(!settingsChord.update(0, 80, 8, false) && settingsChord.draining,
        "partially released chord remains suppressed after leaving the menu");
  check(!settingsChord.update(0, 0, 8.1, false) && !settingsChord.draining,
        "released triggers return to normal driving input");
  settingsChord.update(0, 0, 9, true);
  settingsChord.update(255, 255, 10, true);
  settingsChord.update(255, 255, 10.1, false);
  check(!settingsChord.update(255, 255, 11, true) &&
            !settingsChord.update(255, 255, 12, true),
        "restoring focus cannot finish a chord that began before focus loss");
  std::array<char, 280> outSim{};
  std::memcpy(outSim.data(), "LFST", 4);
  auto putFloat = [&](int offset, float value) {
    std::memcpy(outSim.data() + offset, &value, sizeof(value));
  };
  putFloat(48, 20.f);
  putFloat(72, .8f);
  putFloat(96, 400.f);
  outSim[92] = 3;
  outSim[112 + 29] = char(200);
  outSim[112 + 30] = 1;
  RumbleSample drive;
  check(decodeOutSim(outSim.data(), outSim.size(), drive) &&
            drive.velocity[0] == 20.f && drive.engineSpeed == 400.f &&
            drive.gear == 3 && drive.wheels[0].slip == 200 && drive.wheels[0].touching,
        "OutSim decodes the engine, speed, gear and wheel data at the documented offsets");
  check(!decodeOutSim(outSim.data(), 279, drive), "partial OutSim packet ignored");
  outSim[3] = 'X';
  check(!decodeOutSim(outSim.data(), outSim.size(), drive), "other datagrams ignored");
  outSim[3] = 'T';
  putFloat(96, INFINITY);
  check(!decodeOutSim(outSim.data(), outSim.size(), drive), "invalid telemetry ignored");
  DrivingRumble rumble;
  auto sliding = rumble.update(drive, 100, 100, 100);
  check(sliding.right > sliding.left && sliding.left > 0,
        "sliding wheel favours the high-frequency motor");
  drive.wheels[0].touching = false;
  const auto engineOnly = rumble.update(drive, 100, 100, 100);
  check(engineOnly.right < sliding.right, "airborne wheels do not produce tyre rumble");
  drive.wheels[0].touching = true;
  drive.wheels[0].slip = 0;
  drive.time = 10;
  rumble.update(drive, 110, 110, 100);
  drive.wheels[0].deflection = .02f;
  drive.time = 20;
  const auto bump = rumble.update(drive, 120, 120, 100);
  check(bump.right > engineOnly.right, "suspension movement produces road rumble");
  drive.acceleration[0] = 90.f;
  drive.time = 30;
  const auto impact = rumble.update(drive, 130, 130, 100);
  check(impact.left == 65535 && impact.right > bump.right,
        "impact produces a capped low-frequency pulse");
  const auto faded = rumble.update(drive, 130, 130 + rumbleTimeout, 100);
  check(!faded.left && !faded.right && !rumble.lastPacket,
        "stale telemetry stops both motors and resets the mixer");
  const auto mute = rumble.update(drive, 200, 200, 0);
  check(!mute.left && !mute.right, "zero intensity mutes both motors");
  const auto half = motorLevels(1.5f, -.2f, 50);
  check(half.left == 32768 && !half.right, "motor output respects configured intensity");
  DrivingRumble gearRumble;
  drive.acceleration = {};
  auto inGear = gearRumble.update(drive, 200, 200, 100);
  drive.gear = 4;
  drive.time += 10;
  auto shifted = gearRumble.update(drive, 210, 210, 100);
  check(shifted.left > inGear.left && shifted.right > inGear.right,
        "a driving gear change produces a distinct pulse");
  std::array<unsigned char, 96> carInfo{24, 69, rumbleCarRequest, 7};
  auto putInfoFloat = [&](int offset, float value) {
    std::memcpy(carInfo.data() + offset, &value, sizeof(value));
  };
  putInfoFloat(40, 5.f);
  putInfoFloat(44, 20.f);
  putInfoFloat(68, 4200.f);
  carInfo[64] = 1;
  carInfo[65] = 4;
  RumbleSample fallback;
  check(decodeInSimRumble(carInfo.data(), carInfo.size(), 7, 125, fallback) &&
            !fallback.hasWheels && fallback.velocity[1] == 20.f &&
            fallback.engineSpeed > 439.f && fallback.engineSpeed < 441.f &&
            fallback.gear == 4 && fallback.time == 125,
        "InSim decodes speed, engine and gear without OutSim wheel data");
  check(!decodeInSimRumble(carInfo.data(), carInfo.size(), 8, 125, fallback),
        "InSim reply for another player is ignored");
  carInfo[2] = 3;
  check(!decodeInSimRumble(carInfo.data(), carInfo.size(), 7, 125, fallback),
        "InSim reply for another request is ignored");
  carInfo[2] = rumbleCarRequest;
  carInfo[64] = 0;
  RumbleSample stopped;
  check(decodeInSimRumble(carInfo.data(), carInfo.size(), 7, 125, stopped) &&
            stopped.engineSpeed == 0,
        "engine vibration stops with ignition off");
  const DrivingTelemetry fullTelemetry{drive, 80}, basicTelemetry{fallback, 140};
  check(selectRumbleTelemetry(fullTelemetry, basicTelemetry, 144).sample.hasWheels,
        "fresh OutSim keeps detailed wheel feedback");
  const auto chaseTelemetry = selectRumbleTelemetry(fullTelemetry, basicTelemetry, 145);
  check(chaseTelemetry.fresh(145) && !chaseTelemetry.sample.hasWheels,
        "fresh InSim takes over after the camera stops producing OutSim");
  check(!selectRumbleTelemetry(fullTelemetry, basicTelemetry, 320).fresh(320),
        "losing both telemetry sources stops rumble");
  DrivingRumble chase;
  chase.update(drive, 80, 80, 100);
  const auto chaseLevels = chase.update(fallback, 140, 145, 100);
  check(chaseLevels.left > 0 && chaseLevels.right > 0 && !chase.impact && !chase.shift,
        "chase telemetry preserves engine and side slip without spurious switch pulses");
  check(!rumbleWriteDue(true, chaseLevels, chaseLevels, 99) &&
            rumbleWriteDue(true, chaseLevels, chaseLevels, 100),
        "unchanged active motors get a periodic refresh");
  check(!rumbleWriteDue(true, chaseLevels, {100, 100}, 19) &&
            rumbleWriteDue(true, chaseLevels, {100, 100}, 20),
        "changing active motors respect the update rate limit");
  std::vector<Item> items = {{{0, 0, 1, 1}, 0, 0},
                             {{0, 2, 1, 3}, 1, 0},
                             {{2, 0, 3, 1}, 2, 0},
                             {{4, 1, 5, 2}, 3, 0}};
  check(neighbour(items, 0, Direction::Down) == 1, "aligned down");
  check(neighbour(items, 0, Direction::Right) == 2, "aligned right");
  check(neighbour(items, 0, Direction::Up) == 0, "no edge wrap");
  check(neighbour({}, 0, Direction::Up) == -1, "empty screen");
  check(neighbour(items, -1, Direction::Up) == 0, "lost selection");
  check(menuShortcutDirection(1u, 1u) == int(Direction::Up) &&
            menuShortcutDirection(2u, 2u) == int(Direction::Down),
        "vertical chord chooses the corresponding shortcut");
  check(menuShortcutDirection(4u, 4u) == int(Direction::Left) &&
            menuShortcutDirection(8u, 8u) == int(Direction::Right),
        "horizontal chord chooses the corresponding shortcut");
  check(menuShortcutDirection(5u, 4u) == -1,
        "a diagonal chord cannot pick a shortcut");
  check(menuShortcutDirection(8u, 0u) == -1 &&
            menuShortcutDirection(0u, 4u) == -1,
        "a shortcut requires a fresh held direction");
  ButtonGate shortcutGate;
  shortcutGate.update(0, false);
  check(shortcutGate.update(0x1001, false) == 0x1001,
        "confirm and direction can arrive in the same shortcut frame");
  shortcutGate.blocked |= 0x1001;
  check(!shortcutGate.held(0x1001) && shortcutGate.update(0x1001, false) == 0,
        "a held shortcut and confirm cannot repeat after leaving the layer");
  shortcutGate.update(0, false);
  check(shortcutGate.update(0x1000, false) == 0x1000,
        "fresh confirm can open the focused shortcut target");
  Repeat r;
  check(r.update(true, 0), "first repeat");
  check(!r.update(true, .1), "delay");
  check(r.update(true, .36), "held repeat");
  check(!r.update(false, .4), "release");
  check(r.update(true, .41), "repress");
  Repeat fast;
  check(fast.update(true, 1., true), "list initial movement");
  check(fast.update(true, 1.95, true), "list repeat after holding");
  check(fast.update(true, 2.0, true), "list repeat accelerates");
  HoldConfirm hold;
  check(!hold.update(true, false, true, 0), "tap cannot confirm race finish");
  check(!hold.update(false, true, true, 1), "held on arrival does not arm");
  check(!hold.update(true, true, true, 1.1), "fresh press starts hold");
  check(!hold.update(false, true, true, 1.79), "hold below 700 ms does not confirm");
  check(hold.progress(1.45) > .49f && hold.progress(1.45) < .51f,
        "finish progress reflects elapsed time");
  check(hold.update(false, true, true, 1.81), "full hold confirms finish");
  check(!hold.update(false, true, true, 3), "continued hold cannot confirm twice");
  hold.update(true, true, true, 4);
  check(!hold.update(false, false, true, 4.5), "releasing early cancels finish");
  check(!hold.update(false, true, true, 5), "cancelled hold requires a fresh press");
  hold.update(true, true, true, 6);
  check(!hold.update(false, true, false, 7), "lost target cancels after deadline");
  check(!hold.update(false, true, true, 8), "returning to target cannot complete old hold");
  check(pageOffset(0, 30, 88, 1) == 30, "skip one visible page");
  check(pageOffset(45, 30, 88, 1) == 58, "clamp to last page");
  check(pageOffset(5, 30, 88, -1) == 0, "clamp to first page");
  check(seekTime(1000, 25000, -1) == 0, "seek clamps to start");
  check(seekTime(23800, 25000, 1) == 25000, "seek clamps to end");
  check(seekTime(12345, 25000, 1) == 17345, "five second replay step");
  check(triggerDirection(80, 0) == -1 && triggerDirection(0, 50) == 1,
        "replay trigger directions");
  check(triggerDirection(100, 100) == 0 && triggerDirection(40, 0) == 0,
        "replay trigger ambiguity and thresholds");
  check(raceSession(true, 1, false, false) == RaceSession::Unknown,
        "game flag alone does not distinguish MPR from live session");
  check(raceSession(true, 1, false, true) == RaceSession::Offline,
        "live offline session identified");
  check(raceSession(true, 513, false, true) == RaceSession::Multiplayer,
        "live multiplayer session identified");
  check(raceSession(true, 1, true, false) == RaceSession::Outside &&
            raceSession(true, 2, false, false) == RaceSession::Outside,
        "offline and multiplayer replay excluded");
  const auto offline = RaceSession::Offline;
  const auto online = RaceSession::Multiplayer;
  PadRecovery recovery;
  check(recovery.update(offline, false, false, false, true, false, 0) ==
            RecoveryAction::None && recovery.phase == RecoveryPhase::Idle,
        "launching without a pad cannot pause");
  check(recovery.update(offline, true, false, false, true, false, 1) ==
            RecoveryAction::Pause && recovery.blocksPad(),
        "active pad loss requests offline pause");
  check(recovery.update(offline, false, true, false, true, true, 1.25) ==
            RecoveryAction::None && recovery.phase == RecoveryPhase::Pausing,
        "resume cannot run before pause acknowledgement");
  check(recovery.update(offline, false, true, true, true, true, 1.5) ==
            RecoveryAction::None && recovery.phase == RecoveryPhase::Paused,
        "confirm on acknowledgement cannot resume");
  check(recovery.update(offline, false, true, true, true, false, 2) ==
            RecoveryAction::None && recovery.blocksPad(),
        "reconnect alone waits for confirmation");
  check(recovery.update(RaceSession::Unknown, false, true, false, false, true, 2.1) ==
            RecoveryAction::None && recovery.phase == RecoveryPhase::Paused,
        "lost InSim state cannot clear or resume an owned pause");
  check(recovery.update(offline, false, true, true, true, true, 2.3) ==
            RecoveryAction::Resume && recovery.blocksPad(),
        "fresh confirm resumes and blocks pad until acknowledgement");
  check(recovery.update(offline, false, true, false, true, false, 2.4) ==
            RecoveryAction::None && !recovery.blocksPad(),
        "unpause acknowledgement frees driving input");
  check(recovery.update(online, true, false, false, true, false, 3) ==
            RecoveryAction::None && recovery.phase == RecoveryPhase::Warning &&
            !recovery.blocksPad(),
        "multiplayer loss warns without pausing or capturing");
  check(recovery.update(online, false, true, false, true, true, 3.1) ==
            RecoveryAction::None && recovery.phase == RecoveryPhase::Idle,
        "multiplayer reconnect clears warning without confirm");
  check(recovery.update(offline, true, false, true, true, false, 4) ==
            RecoveryAction::None && recovery.phase == RecoveryPhase::Warning,
        "already manually paused session must not toggle");
  check(recovery.update(offline, false, true, true, true, true, 4.1) ==
            RecoveryAction::None && recovery.phase == RecoveryPhase::Idle,
        "manual pause is not resumed on reconnection");
  check(recovery.update(offline, true, false, false, false, false, 5) ==
            RecoveryAction::None && recovery.phase == RecoveryPhase::Waiting,
        "loss without foreground defers pause");
  check(recovery.update(offline, false, false, false, true, false, 6) ==
            RecoveryAction::Pause,
        "foreground restores pause eligibility");
  check(recovery.update(offline, false, false, false, true, false, 8) ==
            RecoveryAction::None && recovery.phase == RecoveryPhase::Warning,
        "timeout warns and avoids resending a toggle");
  check(recovery.update(RaceSession::Outside, false, false, false, false, false, 9) ==
            RecoveryAction::None && recovery.phase == RecoveryPhase::Idle,
        "leaving the race clears recovery");
  check(quickDirection(-.8f, .3f) == QuickAction::Mirror &&
            quickDirection(.9f, .5f) == QuickAction::Pit &&
            quickDirection(.3f, .8f) == QuickAction::Camera,
        "quick menu follows the dominant stick direction");
  check(quickDirection(.4f, .4f) == QuickAction::None &&
            quickDirection(.2f, -.8f) == QuickAction::None,
        "quick menu cancels in the center and down");
  check(analog(0, .2f) == 0, "center");
  check(analog(1000, .2f) == 0, "deadzone");
  check(analog(-32768, .2f) == -1, "negative endpoint");
  check(analog(32767, .2f) == 1, "positive endpoint");
  ButtonGate g;
  check(g.update(0x1000, false) == 0, "held on connection blocked");
  g.update(0, false);
  check(g.update(0x1000, false) == 0x1000, "fresh confirm");
  check(g.update(0x1000, true) == 0, "transition blocks held confirm");
  check(g.update(0x1000, false) == 0, "still blocked");
  g.update(0, false);
  check(g.update(0x1000, false) == 0x1000, "released confirm restored");
  check(!menuCapture(false, 0, 0, false),
        "unknown state does not capture driving");
  check(menuCapture(true, 256, 0, false), "main menu");
  check(!menuCapture(true, 1, 0, false), "driving");
  check(menuCapture(true, 17, 0, false), "escape dialog");
  check(menuCapture(true, 1, 3, false), "garage");
  check(menuCapture(true, 1, 0, true), "text entry");
  check(menuCapture(true, 2177, 0, true), "race menu with driving InSim flags");
  check(!menuCapture(true, 2177, 0, false),
        "same flags after resuming driving");
  check(menuCapture(false, 0, 0, true),
        "native race menu while InSim is offline");
  check(axisCenter(0, 65535) == 32768, "combined trigger axis rests at center");
  check(axisCenter(-32768, 32767) == 0, "signed steering axis rests at zero");
  check(axisCenter(-10, -3) == -6, "negative calibrated axis center");
  check(axisFraction(32768, 0, 65535) == 0.f, "unsigned axis at rest");
  check(axisFraction(0, 0, 65535) == -1.f &&
            axisFraction(65535, 0, 65535) == 1.f,
        "unsigned axis endpoints reach both ends");
  check(axisFraction(-16384, -32768, 32767) == -.5f,
        "raw XInput steering uses its full signed range");
  check(axisFraction(-8, -10, -3) == -.5f, "calibrated axis fraction");
  check(axisFraction(120, -100, 100) == 1.f &&
            axisFraction(-120, -100, 100) == -1.f,
        "out of range readings are clamped");
  check(axisFraction(INT32_MIN, INT32_MIN, INT32_MAX) == -1.f &&
            axisFraction(INT32_MAX, INT32_MIN, INT32_MAX) == 1.f,
        "wide range uses a safe intermediate");
  check(axisCenter(2000000000, 2100000000) == 2050000000,
        "calibrated center without integer overflow");
  g.update(0, true);
  check(g.update(0x1000, true) == 0, "reconnect while held");
  check(g.held(0x1000) == 0, "held direction suppressed on reconnect");
  g.update(0, false);
  check(g.update(0x1000, false) == 0x1000, "reconnected fresh button");
  std::vector<Item> tabs = {{{0, 0, 1, 1}, 10, 0, 0, true, true},
                            {{1, 0, 2, 1}, 11, 0, 0, true, false},
                            {{2, 0, 3, 1}, 12, 0, 0, true, false}};
  check(nextTab(tabs, 1) == 1, "next native tab");
  check(nextTab(tabs, -1) == 2, "tab wrap backwards");
  check(nextTab(items, 1) == -1, "page fallback without tabs");
  std::vector<Item> slider = {{{0, 0, 1, 1}, 63, 0, 44},
                              {{1, 0, 4, 1}, 64, 0, 40},
                              {{4, 0, 5, 1}, 65, 0, 43},
                              {{4, 2, 5, 3}, 69, 0, 43}};
  check(sliderArrow(slider, 1, -1) == 0, "decrease correct slider");
  check(sliderArrow(slider, 1, 1) == 2, "increase correct slider");
  check(sliderArrow(slider, 0, 1) == -1, "button is not slider");
  std::array<int, 9> bindings{0, 1, 2, 3, 4, 5, 6, 7, 8};
  rebind(bindings, 0, 1);
  check(bindings[0] == 1 && bindings[1] == 0, "swapped confirm preserves back");
  rebind(bindings, 3, 9);
  check(bindings[3] == 9 && bindings[0] == 1, "unused button assignment");
  rebind(bindings, 0, 9);
  check(bindings[0] == 9 && bindings[3] == 1,
        "reassign occupied custom button");
  const std::array<std::uint16_t, 14> assignable = {
      0x1000, 0x2000, 0x4000, 0x8000, 0x0100, 0x0200, 0x0010, 0x0020, 0x0080, 0x0040,
      0x0001, 0x0002, 0x0004, 0x0008};
  BindingCapture capture;
  check(capture.update(assignable[2], assignable) == -1,
        "inactive capture ignores input");
  capture.begin();
  check(capture.update(assignable[2], assignable) == -1 &&
            capture.phase == BindingCapture::Phase::Release,
        "opening X cannot assign itself");
  check(capture.update(0, assignable) == -1 &&
            capture.phase == BindingCapture::Phase::Ready,
        "release arms capture");
  check(capture.update(assignable[0] | assignable[1], assignable) == -1 &&
            capture.phase == BindingCapture::Phase::Release,
        "multiple buttons require release and a new press");
  check(capture.update(assignable[1], assignable) == -1,
        "remaining held button cannot complete a chord assignment");
  check(capture.update(0, assignable) == -1 &&
            capture.update(0x0005, assignable) == -1 &&
            capture.update(0x0004, assignable) == -1 && capture.active(),
        "diagonal D-pad input requires a fresh cardinal press");
  for (int i = 0; i < int(assignable.size()); ++i) {
    capture.begin();
    capture.update(0, assignable);
    check(capture.update(assignable[i], assignable) == i && !capture.active(),
          "each physical button is assignable on its own");
  }
  capture.begin();
  capture.update(0, assignable);
  capture.cancel();
  check(capture.update(assignable[6], assignable) == -1 && !capture.active(),
        "cancel discards the pending assignment");
  NativeButtonAssignmentGate nativeAssignment;
  check(!nativeAssignment.update(true, true, assignable[0]) &&
            !nativeAssignment.update(true, false, assignable[0]) &&
            nativeAssignment.update(true, false, 0),
        "native driving assignment waits for the opening button to be released");
  check(nativeAssignment.update(true, false, assignable[12]) &&
            !nativeAssignment.update(false, false, assignable[12]) &&
            !nativeAssignment.update(true, true, assignable[12]),
        "native assignment forwards a fresh D-pad direction only inside the binding prompt");
  check(colourSliderPosition(0, 160, -1) > 0.f &&
            colourSliderPosition(160, 160, 1) < 1.f,
        "colour clicks stay inside both ends of the slider");
  check(colourSliderPosition(80, 160, 1) > .5f &&
            colourSliderPosition(80, 160, -1) < .5f,
        "colour step moves in the requested direction");
  GarageViewInput view;
  check(view.update(true, true, true, true) == (rightStickMask | triggerAxisMask) &&
            !view.armed,
        "entering the viewer with both controls held does not orbit or zoom");
  view.update(true, false, false, false);
  check(view.armed, "a neutral frame arms the viewer");
  view.update(true, false, true, true);
  check(view.update(false, true, true, true) == (rightStickMask | triggerAxisMask) &&
            !view.armed,
        "both held viewer axes are filtered after exiting");
  check(view.update(false, false, false, true) == triggerAxisMask &&
            view.update(false, false, false, false) == 0,
        "axes resume independently when released");
  check(view.update(false, false, true, true) == 0,
        "fresh movement outside the viewer passes through");
  std::cout << checks << " checks passed\n";
}
