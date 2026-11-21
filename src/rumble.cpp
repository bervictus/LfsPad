#include "runtime.hpp"
#include <xinput.h>

namespace pad {
static SRWLOCK telemetryLock = SRWLOCK_INIT;
static DrivingTelemetry latest;

void observeOutSim(const char *data, int size) {
  RumbleSample sample;
  if (!data || !decodeOutSim(data, size, sample))
    return;
  AcquireSRWLockExclusive(&telemetryLock);
  const bool firstPacket = !latest.received;
  latest = {sample, GetTickCount64()};
  ReleaseSRWLockExclusive(&telemetryLock);
  if (firstPacket)
    log("OutSim Pack2 stream observed");
}

DrivingTelemetry drivingTelemetry() {
  AcquireSRWLockShared(&telemetryLock);
  const auto sample = latest;
  ReleaseSRWLockShared(&telemetryLock);
  return sample;
}

using SetState = decltype(&XInputSetState);
static SetState setState = nullptr;
static SRWLOCK motorsLock = SRWLOCK_INIT;
static int motorSlot = -1;
static MotorLevels motorLevelsSent;
static ULONGLONG lastCommand = 0, lastWrite = 0;
static bool diagnosticLease = false;

HMODULE xinputModule() {
  static const HMODULE module = [] {
    wchar_t system[MAX_PATH];
    GetSystemDirectoryW(system, MAX_PATH);
    return LoadLibraryW((std::wstring(system) + L"\\xinput1_4.dll").c_str());
  }();
  return module;
}

static void clearMotors() {
  if (motorSlot < 0)
    return;
  XINPUT_VIBRATION zero{};
  const DWORD result = setState(motorSlot, &zero);
  if (diagnosticLease)
    log("Rumble stop: slot=%d status=%lu", motorSlot, result);
  motorSlot = -1;
  motorLevelsSent = {};
}

static VOID CALLBACK watchdog(PVOID, BOOLEAN) {
  AcquireSRWLockExclusive(&motorsLock);
  if (motorSlot >= 0 && GetTickCount64() - lastCommand >= rumbleTimeout)
    clearMotors();
  ReleaseSRWLockExclusive(&motorsLock);
}

void stopMotors() {
  AcquireSRWLockExclusive(&motorsLock);
  clearMotors();
  ReleaseSRWLockExclusive(&motorsLock);
}

void driveMotors(int slot, MotorLevels levels, HWND window, bool diagnostic) {
  if (slot < 0 || (!levels.left && !levels.right)) {
    stopMotors();
    return;
  }
  // The module is loaded for the life of the game. A Windows timer queue
  // clears any active command even if Present stops delivering frames.
  static const bool ready = [] {
    auto module = xinputModule();
    if (module)
      setState = reinterpret_cast<SetState>(GetProcAddress(module, "XInputSetState"));
    HANDLE timer = nullptr;
    return setState && CreateTimerQueueTimer(&timer, nullptr, watchdog, nullptr,
                                            40, 40, WT_EXECUTEDEFAULT);
  }();
  if (!ready)
    return;
  AcquireSRWLockExclusive(&motorsLock);
  if (stopping.load() || GetForegroundWindow() != window) {
    clearMotors();
    ReleaseSRWLockExclusive(&motorsLock);
    return;
  }
  const auto now = GetTickCount64();
  if (motorSlot != slot)
    clearMotors();
  lastCommand = now;
  if (rumbleWriteDue(motorSlot >= 0, motorLevelsSent, levels, now - lastWrite)) {
    XINPUT_VIBRATION vibration{levels.left, levels.right};
    const DWORD result = setState(slot, &vibration);
    if (diagnostic && motorSlot != slot)
      log("Rumble start: slot=%d left=%u right=%u status=%lu", slot,
          levels.left, levels.right, result);
    if (result == ERROR_SUCCESS) {
      motorSlot = slot;
      diagnosticLease = diagnostic;
      motorLevelsSent = levels;
      lastWrite = now;
    } else {
      clearMotors();
    }
  }
  ReleaseSRWLockExclusive(&motorsLock);
}
} // namespace pad
