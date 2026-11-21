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

static SRWLOCK motorsLock = SRWLOCK_INIT;
static ControllerId motorSlot = -1;
static MotorLevels motorLevelsSent;
static ULONGLONG lastCommand = 0, lastWrite = 0;
static ControllerId declinedSlot = -1;
static ULONGLONG declinedAt = 0;
static bool diagnosticLease = false;

static void clearMotors() {
  if (motorSlot < 0)
    return;
  const DWORD result = setControllerVibration(motorSlot, 0, 0);
  if (diagnosticLease)
    log("Rumble stop: device=%lld status=%lu", static_cast<long long>(motorSlot), result);
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

void driveMotors(ControllerId slot, MotorLevels levels, HWND window, bool diagnostic) {
  if (slot < 0 || (!levels.left && !levels.right)) {
    stopMotors();
    return;
  }
  // The module is loaded for the life of the game. A Windows timer queue
  // clears any active command even if Present stops delivering frames.
  static const bool ready = [] {
    HANDLE timer = nullptr;
    return CreateTimerQueueTimer(&timer, nullptr, watchdog, nullptr,
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
  if (slot == declinedSlot && now - declinedAt < 1000) {
    ReleaseSRWLockExclusive(&motorsLock);
    return;
  }
  if (motorSlot != slot)
    clearMotors();
  lastCommand = now;
  if (rumbleWriteDue(motorSlot >= 0, motorLevelsSent, levels, now - lastWrite)) {
    const DWORD result = setControllerVibration(slot, levels.left, levels.right);
    if (diagnostic && motorSlot != slot)
      log("Rumble start: device=%lld left=%u right=%u status=%lu", static_cast<long long>(slot),
          levels.left, levels.right, result);
    if (result == ERROR_SUCCESS) {
      declinedSlot = -1;
      motorSlot = slot;
      diagnosticLease = diagnostic;
      motorLevelsSent = levels;
      lastWrite = now;
    } else {
      declinedSlot = slot;
      declinedAt = now;
      clearMotors();
    }
  }
  ReleaseSRWLockExclusive(&motorsLock);
}
} // namespace pad
