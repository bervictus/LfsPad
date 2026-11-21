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
static DWORD declinedError = ERROR_NOT_SUPPORTED;
static bool diagnosticLease = false;

static DWORD clearMotors() {
  if (motorSlot < 0)
    return ERROR_SUCCESS;
  const DWORD result = setControllerVibration(motorSlot, 0, 0);
  if (result == ERROR_BUSY)
    return result; // Retry on the next frame or watchdog callback.
  if (diagnosticLease)
    log("Rumble stop: device=%lld status=%lu", static_cast<long long>(motorSlot), result);
  motorSlot = -1;
  motorLevelsSent = {};
  return result;
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

DWORD driveMotors(ControllerId slot, MotorLevels levels, HWND window, bool diagnostic) {
  if (slot < 0 || (!levels.left && !levels.right)) {
    stopMotors();
    return ERROR_SUCCESS;
  }
  // The module is loaded for the life of the game. A Windows timer queue
  // clears any active command even if Present stops delivering frames.
  static const bool ready = [] {
    HANDLE timer = nullptr;
    return CreateTimerQueueTimer(&timer, nullptr, watchdog, nullptr,
                                 40, 40, WT_EXECUTEDEFAULT);
  }();
  if (!ready)
    return ERROR_NOT_READY;
  AcquireSRWLockExclusive(&motorsLock);
  if (stopping.load() || GetForegroundWindow() != window) {
    clearMotors();
    ReleaseSRWLockExclusive(&motorsLock);
    return ERROR_CANCELLED;
  }
  const auto now = GetTickCount64();
  if (motorSlot != slot && clearMotors() == ERROR_BUSY) {
    ReleaseSRWLockExclusive(&motorsLock);
    return ERROR_BUSY;
  }
  if (slot == declinedSlot && now - declinedAt < 1000) {
    const auto error = declinedError;
    ReleaseSRWLockExclusive(&motorsLock);
    return error;
  }
  lastCommand = now;
  DWORD result = ERROR_SUCCESS;
  if (rumbleWriteDue(motorSlot >= 0, motorLevelsSent, levels, now - lastWrite)) {
    result = setControllerVibration(slot, levels.left, levels.right);
    if (diagnostic && motorSlot != slot)
      log("Rumble start: device=%lld left=%u right=%u status=%lu", static_cast<long long>(slot),
          levels.left, levels.right, result);
    if (result == ERROR_SUCCESS) {
      declinedSlot = -1;
      motorSlot = slot;
      diagnosticLease = diagnostic;
      motorLevelsSent = levels;
      lastWrite = now;
    } else if (result != ERROR_BUSY) {
      declinedSlot = slot;
      declinedAt = now;
      declinedError = result;
      clearMotors();
    }
  }
  ReleaseSRWLockExclusive(&motorsLock);
  return result;
}
} // namespace pad
