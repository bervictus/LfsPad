#include "core.hpp"
#include "controller_axes.hpp"
#include "runtime.hpp"
#include <bcrypt.h>
#include <cstddef>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <dinput.h>
#include <map>
#include <mutex>
#include <vector>
#include <xinput.h>

namespace pad {
HMODULE self = nullptr;
std::atomic<bool> captureButtons{false};
std::atomic<bool> captureRaceAxes{false};
std::atomic<unsigned> captureQuickInput{0};
std::atomic<bool> monitorInput{false};
std::atomic<int> pedalSlot{-1};
std::atomic<std::uint64_t> nativePedals{0};
std::atomic<bool> stopping{false};
std::wstring path(const wchar_t *name) {
  wchar_t p[32768];
  const DWORD length = GetModuleFileNameW(self, p, _countof(p));
  if (!length || length >= _countof(p))
    return {};
  std::wstring s(p, length);
  return s.substr(0, s.find_last_of(L"\\/") + 1) + name;
}
void log(const char *fmt, ...) {
  FILE *f = _wfopen(path(L"LfsPad.log").c_str(), L"ab");
  if (!f)
    return;
  std::fprintf(f, "[%llu] ", GetTickCount64());
  va_list a;
  va_start(a, fmt);
  std::vfprintf(f, fmt, a);
  va_end(a);
  std::fputc('\n', f);
  std::fclose(f);
}
static bool verifyBuild() {
  wchar_t exe[32768];
  const DWORD length = GetModuleFileNameW(nullptr, exe, _countof(exe));
  if (!length || length >= _countof(exe))
    return false;
  FILE *f = _wfopen(exe, L"rb");
  if (!f)
    return false;
  BCRYPT_ALG_HANDLE alg = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  unsigned char digest[32]{};
  bool ok = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr,
                                        0) >= 0;
  if (ok)
    ok = BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
  unsigned char b[16384];
  size_t n;
  while (ok && (n = std::fread(b, 1, sizeof(b), f)))
    ok = BCryptHashData(hash, b, (ULONG)n, 0) >= 0;
  if (std::ferror(f))
    ok = false;
  std::fclose(f);
  if (ok)
    ok = BCryptFinishHash(hash, digest, 32, 0) >= 0;
  if (hash)
    BCryptDestroyHash(hash);
  if (alg)
    BCryptCloseAlgorithmProvider(alg, 0);
  const unsigned char expected[] = {
      0x77, 0x60, 0x6E, 0x57, 0x88, 0x5F, 0xEC, 0x33, 0xCD, 0xDA, 0xBD,
      0xFD, 0x22, 0x71, 0xD2, 0xF1, 0x81, 0xC6, 0x58, 0x06, 0xC2, 0xD7,
      0xEC, 0x00, 0x49, 0x65, 0x4B, 0xD5, 0x48, 0x7D, 0x8A, 0x52};
  ok = ok && std::memcmp(expected, digest, 32) == 0;
  log("0.8C38 SHA256 validation: %s",
      ok ? "PASS" : "UNSUPPORTED - menu adapter disabled");
  return ok;
}
bool supportedBuild() {
  static const bool verified = verifyBuild();
  return verified;
}

static bool patch(void **slot, void *value, void **old) {
  DWORD prot;
  if (!VirtualProtect(slot, sizeof(void *), PAGE_READWRITE, &prot))
    return false;
  const auto previous = InterlockedCompareExchangePointer(slot, nullptr, nullptr);
  if (previous != value) {
    // Publish the original before another thread can enter the new hook.
    *old = previous;
    InterlockedExchangePointer(slot, value);
  }
  VirtualProtect(slot, sizeof(void *), prot, &prot);
  return true;
}
using Present = HRESULT(STDMETHODCALLTYPE *)(IDXGISwapChain *, UINT, UINT);
using Resize = HRESULT(STDMETHODCALLTYPE *)(IDXGISwapChain *, UINT, UINT, UINT,
                                            DXGI_FORMAT, UINT);
static Present presentOriginal = nullptr;
static Resize resizeOriginal = nullptr;
static bool graphicsInstalled = false;
static HRESULT STDMETHODCALLTYPE presentHook(IDXGISwapChain *s, UINT sync,
                                             UINT flags) {
  if (!stopping.load() && !(flags & DXGI_PRESENT_TEST))
    frame(s);
  return presentOriginal(s, sync, flags);
}
static HRESULT STDMETHODCALLTYPE resizeHook(IDXGISwapChain *s, UINT n, UINT w,
                                            UINT h, DXGI_FORMAT format,
                                            UINT flags) {
  releaseTarget();
  return resizeOriginal(s, n, w, h, format, flags);
}
void installGraphicsHook() {
  if (graphicsInstalled)
    return;
  WNDCLASSW cls{};
  cls.lpfnWndProc = DefWindowProcW;
  cls.hInstance = self;
  cls.lpszClassName = L"LfsPadPrivateBootstrap";
  RegisterClassW(&cls);
  HWND w = CreateWindowW(cls.lpszClassName, L"", WS_POPUP, 0, 0, 16, 16,
                         nullptr, nullptr, self, nullptr);
  DXGI_SWAP_CHAIN_DESC desc{};
  desc.BufferCount = 1;
  desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.OutputWindow = w;
  desc.SampleDesc.Count = 1;
  desc.Windowed = TRUE;
  desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  IDXGISwapChain *swap = nullptr;
  ID3D11Device *dev = nullptr;
  ID3D11DeviceContext *context = nullptr;
  HRESULT hr = D3D11CreateDeviceAndSwapChain(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
      D3D11_SDK_VERSION, &desc, &swap, &dev, nullptr, &context);
  if (SUCCEEDED(hr)) {
    auto table = *reinterpret_cast<void ***>(swap);
    bool p = patch(table + 8, (void *)presentHook, (void **)&presentOriginal);
    bool r = patch(table + 13, (void *)resizeHook, (void **)&resizeOriginal);
    graphicsInstalled = p && r;
    log("D3D11 Present/Resize hooks: %d/%d", p, r);
  } else
    log("D3D11 bootstrap failed: %08lx", (unsigned long)hr);
  if (context)
    context->Release();
  if (dev)
    dev->Release();
  if (swap)
    swap->Release();
  if (w)
    DestroyWindow(w);
  UnregisterClassW(cls.lpszClassName, self);
}
using GetState = HRESULT(STDMETHODCALLTYPE *)(void *, DWORD, void *);
using GetData = HRESULT(STDMETHODCALLTYPE *)(void *, DWORD, void *, DWORD *,
                                             DWORD);
using CreateDevice = HRESULT(STDMETHODCALLTYPE *)(void *, REFGUID, void **,
                                                  IUnknown *);
using SetFormat = HRESULT(STDMETHODCALLTYPE *)(void *, LPCDIDATAFORMAT);
using SetProperty = HRESULT(STDMETHODCALLTYPE *)(void *, REFGUID,
                                                 LPCDIPROPHEADER);
using GetProperty = HRESULT(STDMETHODCALLTYPE *)(void *, REFGUID, LPDIPROPHEADER);
using EnumObjects = HRESULT(STDMETHODCALLTYPE *)(void *, LPDIENUMDEVICEOBJECTSCALLBACKA,
                                                 void *, DWORD);
using ReleaseDevice = ULONG(STDMETHODCALLTYPE *)(void *);
struct DeviceHooks {
  GetState state;
  GetData data;
  SetFormat format;
  SetProperty property;
  GetProperty getProperty;
  EnumObjects enumerate;
  ReleaseDevice release;
};
struct InputHooks {
  CreateDevice create;
  bool ansi;
};
enum class ControlKind { Button, Pov, CenteredAxis, ZeroAxis };
enum class MeterAxis { None, Steering, Triggers };
struct Control {
  DWORD offset;
  ControlKind kind = ControlKind::Button;
  DWORD neutral = 0;
  unsigned quickMask = 0;
  MeterAxis meterAxis = MeterAxis::None;
  bool rangeAvailable = false;
  LONG minimum = 0, maximum = 65535;
};
struct GamepadFormat {
  std::vector<Control> controls;
  GUID device{};
  DWORD vidpid = 0;
  DWORD size = 0;
  bool xinput = false;
  bool canSplitPedals = false, splitPedals = false;
  bool rangesKnown = false;
  bool gateLogged = false;
  DrivingInputSample sample;
};
static std::map<void **, DeviceHooks> deviceHooks;
static std::map<void **, InputHooks> inputHooks;
static std::map<void *, GamepadFormat> gamepads;
static std::mutex hooksMutex;
static constexpr std::array<DWORD, 2> pedalOffsets{
    offsetof(DIJOYSTATE2, rglSlider), offsetof(DIJOYSTATE2, rglSlider) + sizeof(LONG)};
static constexpr DWORD pedalObject = DIDFT_ABSAXIS | DIDFT_MAKEINSTANCE(0x7ffe);
static constexpr LONG pedalMinimum = 0, pedalMaximum = 65535;

static bool supportedGamepad(const GamepadFormat &format) {
  return format.xinput || isMappedNativeDevice(format.vidpid);
}

static bool hasPedalOffsets(const DIDATAFORMAT &format) {
  if (format.dwDataSize != sizeof(DIJOYSTATE2) ||
      format.dwObjSize != sizeof(DIOBJECTDATAFORMAT) ||
      !(format.dwFlags & DIDF_ABSAXIS))
    return false;
  unsigned sliders = 0;
  for (DWORD i = 0; i < format.dwNumObjs; ++i) {
    const auto &object = format.rgodf[i];
    if (object.pguid && IsEqualGUID(*object.pguid, GUID_Slider) &&
        (object.dwType & DIDFT_OPTIONAL) && (object.dwType & DIDFT_ABSAXIS)) {
      for (std::size_t j = 0; j < pedalOffsets.size(); ++j)
        if (object.dwOfs == pedalOffsets[j])
          sliders |= 1u << j;
    }
  }
  return sliders == 3;
}

struct PedalEnumeration {
  LPDIENUMDEVICEOBJECTSCALLBACKA callback;
  void *context = nullptr;
  bool nativeSlider = false, stopped = false;
};
static BOOL CALLBACK enumPedalProbe(const DIDEVICEOBJECTINSTANCEA *item, void *context) {
  auto &state = *static_cast<PedalEnumeration *>(context);
  state.nativeSlider |= IsEqualGUID(item->guidType, GUID_Slider);
  const BOOL result = state.callback(item, state.context);
  state.stopped = result == DIENUM_STOP;
  return result;
}
static HRESULT STDMETHODCALLTYPE enumObjectsHook(void *obj, LPDIENUMDEVICEOBJECTSCALLBACKA callback,
                                                 void *context, DWORD flags) {
  EnumObjects original;
  bool tryPedals = false;
  {
    std::lock_guard<std::mutex> lock(hooksMutex);
    original = deviceHooks.at(*reinterpret_cast<void ***>(obj)).enumerate;
    const auto gamepad = gamepads.find(obj);
    tryPedals = gamepad != gamepads.end() && gamepad->second.canSplitPedals &&
                supportedGamepad(gamepad->second);
  }
  const bool acceptsAxis = (flags == DIDFT_ALL || (flags & DIDFT_ABSAXIS)) &&
                          !(flags & (DIDFT_FFACTUATOR | DIDFT_FFEFFECTTRIGGER));
  if (!tryPedals || !acceptsAxis || !callback)
    return original(obj, callback, context, flags);
  PedalEnumeration state{callback, context};
  const auto hr = original(obj, enumPedalProbe, &state, flags);
  if (SUCCEEDED(hr) && !state.stopped && !state.nativeSlider) {
    bool first = false;
    {
      std::lock_guard<std::mutex> lock(hooksMutex);
      const auto gamepad = gamepads.find(obj);
      if (gamepad == gamepads.end() || !gamepad->second.canSplitPedals)
        return hr;
      first = !gamepad->second.splitPedals;
      gamepad->second.splitPedals = true;
    }
    // LFS registers both DIJOYSTATE2 slider positions from one GUID_Slider.
    DIDEVICEOBJECTINSTANCEA slider{};
    slider.dwSize = sizeof(slider);
    slider.guidType = GUID_Slider;
    slider.dwOfs = pedalOffsets[0];
    slider.dwType = pedalObject;
    slider.dwFlags = DIDOI_ASPECTPOSITION;
    std::strcpy(slider.tszName, "LfsPad RT / LT");
    callback(&slider, context);
    if (first)
      log("DirectInput split pedals: Slider 0=throttle, Slider 1=brake");
  }
  return hr;
}
static HRESULT STDMETHODCALLTYPE getPropertyHook(void *obj, REFGUID property,
                                                 LPDIPROPHEADER value) {
  GetProperty original;
  {
    std::lock_guard<std::mutex> lock(hooksMutex);
    original = deviceHooks.at(*reinterpret_cast<void ***>(obj)).getProperty;
    const auto gamepad = gamepads.find(obj);
    if (&property == &DIPROP_RANGE && value &&
        value->dwSize == sizeof(DIPROPRANGE) && value->dwHeaderSize == sizeof(DIPROPHEADER) &&
        gamepad != gamepads.end() && gamepad->second.splitPedals &&
        ((value->dwHow == DIPH_BYID && value->dwObj == pedalObject) ||
         (value->dwHow == DIPH_BYOFFSET &&
          (value->dwObj == pedalOffsets[0] || value->dwObj == pedalOffsets[1])))) {
      auto range = reinterpret_cast<LPDIPROPRANGE>(value);
      range->lMin = pedalMinimum;
      range->lMax = pedalMaximum;
      return DI_OK;
    }
  }
  return original(obj, property, value);
}
static ULONG STDMETHODCALLTYPE releaseDeviceHook(void *obj) {
  ReleaseDevice original;
  {
    std::lock_guard<std::mutex> lock(hooksMutex);
    original = deviceHooks.at(*reinterpret_cast<void ***>(obj)).release;
  }
  const auto count = original(obj);
  if (!count) {
    std::lock_guard<std::mutex> lock(hooksMutex);
    gamepads.erase(obj);
  }
  return count;
}

DrivingInputSample drivingInputSample(ULONGLONG since) {
  std::lock_guard<std::mutex> lock(hooksMutex);
  const auto now = GetTickCount64();
  DrivingInputSample result;
  for (const auto &entry : gamepads) {
    const auto &sample = entry.second.sample;
    if (!sample.tick || sample.tick < since || now - sample.tick > 500)
      continue;
    if (result.tick) {
      result = {};
      result.ambiguous = true;
      break;
    }
    result = sample;
  }
  return result;
}

static bool isAxis(ControlKind kind) {
  return kind == ControlKind::CenteredAxis || kind == ControlKind::ZeroAxis;
}
static bool filterControl(const Control &control, bool buttons, bool axes,
                          unsigned quick) {
  return axes || (buttons && !isAxis(control.kind)) || (control.quickMask & quick);
}

static GamepadFormat describeFormat(const DIDATAFORMAT &format, const GamepadFormat &device) {
  GamepadFormat result;
  result.size = format.dwDataSize;
  result.device = device.device;
  result.vidpid = device.vidpid;
  result.xinput = device.xinput;
  const bool knownButtons = !device.xinput && knownPlayStationButtonLayout(device.vidpid);
  for (DWORD i = 0; i < format.dwNumObjs; ++i) {
    const auto &object = format.rgodf[i];
    Control control{object.dwOfs, ControlKind::Button};
    if (object.dwType & DIDFT_BUTTON) {
      int instance = DIDFT_GETINSTANCE(object.dwType);
      if (knownButtons)
        instance = playStationButtonIndex(unsigned(instance));
      if ((device.xinput || knownButtons) && instance >= 0 && instance < 10)
        control.quickMask = 1u << instance;
      if (control.offset < result.size)
        result.controls.push_back(control);
      continue;
    }
    if (control.offset >= result.size ||
        result.size - control.offset < sizeof(DWORD))
      continue;
    if (object.dwType & DIDFT_POV) {
      control.kind = ControlKind::Pov;
      control.neutral = 0xffffffff;
    } else if (object.dwType & DIDFT_AXIS) {
      auto aspect = object.dwFlags & DIDOI_ASPECTMASK;
      control.kind = (format.dwFlags & DIDF_RELAXIS) ||
                             (aspect && aspect != DIDOI_ASPECTPOSITION)
                         ? ControlKind::ZeroAxis
                         : ControlKind::CenteredAxis;
      if (object.pguid &&
          (IsEqualGUID(*object.pguid, GUID_RxAxis) ||
           IsEqualGUID(*object.pguid, GUID_RyAxis)))
        control.quickMask = rightStickMask;
      if (object.pguid && IsEqualGUID(*object.pguid, GUID_ZAxis))
        control.quickMask = triggerAxisMask;
      if (control.kind == ControlKind::CenteredAxis && object.pguid) {
        if (IsEqualGUID(*object.pguid, GUID_XAxis)) {
          control.quickMask = leftStickMask;
          control.meterAxis = MeterAxis::Steering;
        } else if (IsEqualGUID(*object.pguid, GUID_YAxis)) {
          control.quickMask = leftStickMask;
        } else if (IsEqualGUID(*object.pguid, GUID_ZAxis) && device.xinput) {
          control.meterAxis = MeterAxis::Triggers;
        }
        if (!device.xinput && control.quickMask != leftStickMask)
          control.quickMask = rightStickMask | triggerAxisMask;
      }
    } else {
      continue;
    }
    result.controls.push_back(control);
  }
  return result;
}

static HRESULT STDMETHODCALLTYPE formatHook(void *obj, LPCDIDATAFORMAT format) {
  SetFormat original;
  {
    std::lock_guard<std::mutex> lock(hooksMutex);
    original = deviceHooks.at(*reinterpret_cast<void ***>(obj)).format;
  }
  auto hr = original(obj, format);
  if (SUCCEEDED(hr) && format && format->dwSize == sizeof(DIDATAFORMAT) &&
      format->dwObjSize == sizeof(DIOBJECTDATAFORMAT) &&
      (format->rgodf || !format->dwNumObjs)) {
    std::lock_guard<std::mutex> lock(hooksMutex);
    auto gamepad = gamepads.find(obj);
    if (gamepad != gamepads.end()) {
      auto description = describeFormat(*format, gamepad->second);
      static const bool splitEnabled = GetPrivateProfileIntW(
          L"Input", L"SplitTriggers", 1, path(L"LfsPad.ini").c_str()) != 0;
      description.canSplitPedals = splitEnabled && hasPedalOffsets(*format);
      gamepad->second = std::move(description);
      log("DirectInput gamepad format: %lu bytes, %zu controls",
          format->dwDataSize, gamepad->second.controls.size());
    }
  }
  return hr;
}

static HRESULT STDMETHODCALLTYPE propertyHook(void *obj, REFGUID property,
                                              LPCDIPROPHEADER value) {
  SetProperty original;
  {
    std::lock_guard<std::mutex> lock(hooksMutex);
    original = deviceHooks.at(*reinterpret_cast<void ***>(obj)).property;
  }
  auto hr = original(obj, property, value);
  if (SUCCEEDED(hr) && &property == &DIPROP_RANGE) {
    std::lock_guard<std::mutex> lock(hooksMutex);
    auto gamepad = gamepads.find(obj);
    if (gamepad != gamepads.end())
      gamepad->second.rangesKnown = false;
  }
  return hr;
}

static void readAxisRanges(void *obj, GetProperty getProperty, GamepadFormat &format) {
  for (auto &control : format.controls) {
    if (control.kind != ControlKind::CenteredAxis)
      continue;
    DIPROPRANGE range{
        {sizeof(range), sizeof(DIPROPHEADER), control.offset, DIPH_BYOFFSET},
        0,
        65535};
    control.rangeAvailable =
        SUCCEEDED(getProperty(obj, DIPROP_RANGE, &range.diph)) &&
        range.lMin < range.lMax && range.lMin != DIPROPRANGE_NOMIN &&
        range.lMax != DIPROPRANGE_NOMAX;
    control.minimum = control.rangeAvailable ? range.lMin : 0;
    control.maximum = control.rangeAvailable ? range.lMax : 65535;
    control.neutral = DWORD(axisCenter(control.minimum, control.maximum));
  }
  format.rangesKnown = true;
}

static void writePedals(const GamepadFormat &format, void *data, bool suppressed,
                       DrivingInputSample &sample) {
  std::uint8_t right = 0, left = 0;
  if (!suppressed) {
    const bool uniqueDevice = std::none_of(gamepads.begin(), gamepads.end(), [&](const auto &entry) {
      const auto &other = entry.second;
      const bool sameFamily = format.xinput ? other.xinput
                                           : !other.xinput && other.vidpid == format.vidpid;
      return sameFamily && !IsEqualGUID(format.device, other.device);
    });
    if (uniqueDevice && format.xinput) {
      XINPUT_STATE state{};
      if (readXInputController(pedalSlot.load(), state) == ERROR_SUCCESS) {
        right = state.Gamepad.bRightTrigger;
        left = state.Gamepad.bLeftTrigger;
      }
    } else if (uniqueDevice) {
      const auto pedals = nativePedals.load();
      if (pedals && std::uint32_t(pedals) == format.vidpid) {
        right = std::uint8_t(pedals >> 40);
        left = std::uint8_t(pedals >> 32);
      }
    }
  }
  const auto throttle = pedalAxis(right, pedalMinimum, pedalMaximum, false);
  const auto brake = pedalAxis(left, pedalMinimum, pedalMaximum, false);
  auto *base = static_cast<unsigned char *>(data);
  std::memcpy(base + pedalOffsets[0], &throttle, sizeof(throttle));
  std::memcpy(base + pedalOffsets[1], &brake, sizeof(brake));
  sample.splitPedals = true;
  sample.throttle = float(throttle - pedalMinimum) / (pedalMaximum - pedalMinimum);
  sample.brake = float(brake - pedalMinimum) / (pedalMaximum - pedalMinimum);
}

static HRESULT STDMETHODCALLTYPE stateHook(void *obj, DWORD size, void *data) {
  DeviceHooks hooks;
  {
    std::lock_guard<std::mutex> l(hooksMutex);
    hooks = deviceHooks.at(*reinterpret_cast<void ***>(obj));
  }
  auto hr = hooks.state(obj, size, data);
  const bool axes = captureRaceAxes.load();
  const bool buttons = captureButtons.load();
  const auto quick = captureQuickInput.load();
  const bool observe = monitorInput.load();
  if (SUCCEEDED(hr) && data) {
    std::lock_guard<std::mutex> lock(hooksMutex);
    auto gamepad = gamepads.find(obj);
    if (gamepad == gamepads.end())
      return hr;
    auto &format = gamepad->second;
    if (!supportedGamepad(format))
      return hr;
    if (!(axes || buttons || quick || observe || format.splitPedals))
      return hr;
    if (format.size == size && !format.controls.empty()) {
      if ((observe || axes || (quick & (rightStickMask | triggerAxisMask | leftStickMask))) &&
          !format.rangesKnown)
        readAxisRanges(obj, hooks.getProperty, format);
      DrivingInputSample sample;
      if (observe) {
        sample.tick = GetTickCount64();
        sample.buttonsFiltered = axes || buttons;
        sample.axesFiltered = axes;
      }
      for (const auto &control : format.controls) {
        auto dest = static_cast<unsigned char *>(data) + control.offset;
        const bool filtered = filterControl(control, buttons, axes, quick);
        if (control.kind == ControlKind::Button) {
          const auto mask = std::uint16_t(control.quickMask);
          if (observe && mask) {
            sample.buttonsAvailable |= mask;
            if (filtered && (*dest & 0x80))
              sample.buttonsSuppressed |= mask;
          }
          if (filtered)
            *dest = 0;
          if (observe && (*dest & 0x80))
            sample.buttonsDown |= mask;
        } else {
          if (filtered)
            std::memcpy(dest, &control.neutral, sizeof(control.neutral));
          if (!observe)
            continue;
          if (control.kind == ControlKind::Pov && !sample.povAvailable) {
            sample.povAvailable = true;
            std::memcpy(&sample.pov, dest, sizeof(sample.pov));
          }
          if (control.meterAxis != MeterAxis::None && control.rangeAvailable) {
            LONG value;
            std::memcpy(&value, dest, sizeof(value));
            const float normalized =
                axisFraction(value, control.minimum, control.maximum);
            if (control.meterAxis == MeterAxis::Steering) {
              sample.steering = normalized;
              sample.steeringAvailable = true;
            } else {
              sample.triggers = normalized;
              sample.triggersAvailable = true;
            }
          }
        }
      }
      if (format.splitPedals)
        writePedals(format, data, axes || (quick & triggerAxisMask), sample);
      if (observe)
        format.sample = sample;
      if (axes && !format.gateLogged) {
        log("DirectInput axes, buttons and POV neutralized "
            "(%lu-byte state)",
            size);
        format.gateLogged = true;
      }
    }
  }
  return hr;
}
static HRESULT STDMETHODCALLTYPE dataHook(void *obj, DWORD size, void *data,
                                          DWORD *count, DWORD flags) {
  GetData orig;
  {
    std::lock_guard<std::mutex> l(hooksMutex);
    orig = deviceHooks.at(*reinterpret_cast<void ***>(obj)).data;
  }
  auto hr = orig(obj, size, data, count, flags);
  const bool axes = captureRaceAxes.load();
  const bool buttons = captureButtons.load();
  const auto quick = captureQuickInput.load();
  if (SUCCEEDED(hr) && (axes || buttons || quick) && count && data &&
      (size == sizeof(DIDEVICEOBJECTDATA) ||
       size == sizeof(DIDEVICEOBJECTDATA_DX3))) {
    std::lock_guard<std::mutex> lock(hooksMutex);
    auto gamepad = gamepads.find(obj);
    if (gamepad == gamepads.end() || !supportedGamepad(gamepad->second))
      return hr;
    if (axes) {
      *count = 0;
      return hr;
    }
    auto events = static_cast<unsigned char *>(data);
    const auto &controls = gamepad->second.controls;
    DWORD out = 0;
    for (DWORD i = 0; i < *count; ++i) {
      DWORD offset;
      std::memcpy(&offset, events + i * size, sizeof(offset));
      const bool captured = std::any_of(
          controls.begin(), controls.end(), [=](const Control &control) {
            return control.offset == offset &&
                   filterControl(control, buttons, axes, quick);
          });
      if (!captured) {
        if (out != i)
          std::memcpy(events + out * size, events + i * size, size);
        ++out;
      }
    }
    *count = out;
  }
  return hr;
}
static HRESULT STDMETHODCALLTYPE createHook(void *obj, REFGUID guid, void **out,
                                            IUnknown *outer) {
  InputHooks hooks;
  {
    std::lock_guard<std::mutex> l(hooksMutex);
    hooks = inputHooks.at(*reinterpret_cast<void ***>(obj));
  }
  HRESULT hr = hooks.create(obj, guid, out, outer);
  if (SUCCEEDED(hr) && out && *out) {
    std::lock_guard<std::mutex> l(hooksMutex);
    gamepads.erase(*out);
  }
  if (SUCCEEDED(hr) && out && *out && guid != GUID_SysMouse &&
      guid != GUID_SysKeyboard) {
    auto dev = (IDirectInputDevice8W *)*out;
    DIPROPGUIDANDPATH p{};
    p.diph = {sizeof(p), sizeof(DIPROPHEADER), 0, DIPH_DEVICE};
    bool xinput = SUCCEEDED(dev->GetProperty(DIPROP_GUIDANDPATH, &p.diph)) &&
                  std::wcsstr(p.wszPath, L"IG_");
    DIPROPDWORD vid{};
    vid.diph = {sizeof(vid), sizeof(DIPROPHEADER), 0, DIPH_DEVICE};
    // F710 X-mode fallback for drivers that omit the IG_ path marker.
    const bool hasId = SUCCEEDED(dev->GetProperty(DIPROP_VIDPID, &vid.diph));
    xinput |= hasId && vid.dwData == 0xC21F046D;
    auto table = *reinterpret_cast<void ***>(*out);
    std::lock_guard<std::mutex> l(hooksMutex);
    gamepads.erase(*out);
    if (xinput || (hasId && vid.dwData)) {
      auto &format = gamepads[*out];
      format.device = guid;
      format.xinput = xinput;
      format.vidpid = hasId ? vid.dwData : 0;
    }
    if (table[9] != (void *)stateHook) {
      deviceHooks[table] = {(GetState)table[9], (GetData)table[10],
                            (SetFormat)table[11], (SetProperty)table[6],
                            (GetProperty)table[5], (EnumObjects)table[4],
                            (ReleaseDevice)table[2]};
      void *old = nullptr;
      patch(table + 9, (void *)stateHook, &old);
      patch(table + 10, (void *)dataHook, &old);
      patch(table + 11, (void *)formatHook, &old);
      patch(table + 6, (void *)propertyHook, &old);
      patch(table + 2, (void *)releaseDeviceHook, &old);
      if (hooks.ansi) {
        patch(table + 4, (void *)enumObjectsHook, &old);
        patch(table + 5, (void *)getPropertyHook, &old);
      }
      log("DirectInput device routed through LfsPad (XInput=%d)", xinput);
    }
  }
  return hr;
}
static HMODULE realInput() {
  static HMODULE module = []() {
    const auto previous = path(L"LfsPad.previous.dll");
    auto prior = previous.empty() ? nullptr : LoadLibraryExW(
        previous.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (prior) {
      log("Proxy chain: LfsPad.previous.dll");
      return prior;
    }
    log("Proxy chain: system dinput8.dll");
    return loadSystemLibrary(L"dinput8.dll");
  }();
  return module;
}
using Factory = HRESULT(WINAPI *)(REFIID, void **);
using Process = decltype(&CreateProcessA);
static Process processOriginal = nullptr;
static bool automaticHelper(const char *s) {
  if (!s)
    return false;
  for (; *s; ++s)
    if (_strnicmp(s, "LfsAutomatic.exe", 16) == 0)
      return true;
  return false;
}
static BOOL WINAPI processHook(LPCSTR app, LPSTR command,
                               LPSECURITY_ATTRIBUTES processAttributes,
                               LPSECURITY_ATTRIBUTES threadAttributes,
                               BOOL inherit, DWORD flags, LPVOID environment,
                               LPCSTR directory, LPSTARTUPINFOA startup,
                               LPPROCESS_INFORMATION info) {
  // LFS /exec inherits its listening sockets into this persistent helper.
  // The helper needs no parent handles; retaining them prevents the next LFS
  // process from opening InSim. Preserve all other child launch behaviour.
  if (inherit && (automaticHelper(app) || automaticHelper(command))) {
    inherit = FALSE;
    log("LfsAutomatic compatibility: parent handle inheritance disabled");
  }
  return processOriginal(app, command, processAttributes, threadAttributes,
                         inherit, flags, environment, directory, startup, info);
}
static void processCompatibility() {
  auto base = (unsigned char *)GetModuleHandleW(nullptr);
  auto dos = reinterpret_cast<IMAGE_DOS_HEADER *>(base);
  auto nt = reinterpret_cast<IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
  auto rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]
                 .VirtualAddress;
  for (auto imp = (IMAGE_IMPORT_DESCRIPTOR *)(base + rva); imp->Name; ++imp) {
    if (!imp->OriginalFirstThunk)
      continue;
    auto names = (IMAGE_THUNK_DATA *)(base + imp->OriginalFirstThunk);
    auto addresses = (IMAGE_THUNK_DATA *)(base + imp->FirstThunk);
    for (; names->u1.AddressOfData; ++names, ++addresses) {
      if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
        continue;
      auto name = (IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData);
      if (std::strcmp((char *)name->Name, "CreateProcessA") == 0) {
        patch((void **)&addresses->u1.Function, (void *)processHook,
              (void **)&processOriginal);
        return;
      }
    }
  }
}

using SendTo = decltype(&sendto);
using Send = decltype(&send);
static SendTo sendToOriginal = nullptr;
static Send sendOriginal = nullptr;
static int WSAAPI outSimSend(SOCKET fd, const char *data, int size, int flags,
                             const sockaddr *destination, int destinationSize) {
  observeOutSim(data, size);
  return sendToOriginal(fd, data, size, flags, destination, destinationSize);
}
static int WSAAPI outSimSendConnected(SOCKET fd, const char *data, int size, int flags) {
  observeOutSim(data, size);
  return sendOriginal(fd, data, size, flags);
}

using Bind = decltype(&bind);
static Bind bindOriginal = nullptr;
static int WSAAPI localInSimBind(SOCKET fd, const sockaddr *address, int size) {
  if (address && size >= int(sizeof(sockaddr_in)) && address->sa_family == AF_INET) {
    const auto &requested = *reinterpret_cast<const sockaddr_in *>(address);
    if (requested.sin_port == htons(inSimPort()) && supportedBuild()) {
      static const bool localOnly = GetPrivateProfileIntW(
          L"General", L"AllowRemoteInSim", 0, path(L"LfsPad.ini").c_str()) != 1;
      if (!localOnly)
        return bindOriginal(fd, address, size);
      auto local = requested;
      local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      return bindOriginal(fd, reinterpret_cast<const sockaddr *>(&local), sizeof(local));
    }
  }
  return bindOriginal(fd, address, size);
}

static void installNetworkHooks() {
  auto base = reinterpret_cast<unsigned char *>(GetModuleHandleW(nullptr));
  auto dos = reinterpret_cast<IMAGE_DOS_HEADER *>(base);
  auto nt = reinterpret_cast<IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
  auto rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
  if (!rva)
    return;
  for (auto imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR *>(base + rva);
       imp->Name; ++imp) {
    if (_stricmp(reinterpret_cast<char *>(base + imp->Name), "WS2_32.dll") ||
        !imp->OriginalFirstThunk)
      continue;
    auto names = reinterpret_cast<IMAGE_THUNK_DATA *>(base + imp->OriginalFirstThunk);
    auto addresses = reinterpret_cast<IMAGE_THUNK_DATA *>(base + imp->FirstThunk);
    bool observedSend = false, observedSendTo = false;
    for (; names->u1.AddressOfData; ++names, ++addresses) {
      // The verified executable imports Winsock send and sendto by ordinal.
      if (!IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
        continue;
      const auto ordinal = IMAGE_ORDINAL(names->u1.Ordinal);
      auto slot = reinterpret_cast<void **>(&addresses->u1.Function);
      if (ordinal == 19)
        observedSend = patch(slot, reinterpret_cast<void *>(outSimSendConnected),
                             reinterpret_cast<void **>(&sendOriginal));
      else if (ordinal == 20)
        observedSendTo = patch(slot, reinterpret_cast<void *>(outSimSend),
                               reinterpret_cast<void **>(&sendToOriginal));
    }
    log("OutSim observer: send=%d sendto=%d", observedSend, observedSendTo);
    return;
  }
}
using CreateSwap = HRESULT(STDMETHODCALLTYPE *)(IDXGIFactory *, IUnknown *,
                                                DXGI_SWAP_CHAIN_DESC *,
                                                IDXGISwapChain **);
static Factory factoryOriginal = nullptr;
static CreateSwap swapOriginal = nullptr;
static HRESULT STDMETHODCALLTYPE swapHook(IDXGIFactory *f, IUnknown *d,
                                          DXGI_SWAP_CHAIN_DESC *desc,
                                          IDXGISwapChain **out) {
  auto hr = swapOriginal(f, d, desc, out);
  if (SUCCEEDED(hr) && out && *out && !graphicsInstalled) {
    auto t = *reinterpret_cast<void ***>(*out);
    bool p = patch(t + 8, (void *)presentHook, (void **)&presentOriginal);
    bool r = patch(t + 13, (void *)resizeHook, (void **)&resizeOriginal);
    graphicsInstalled = p && r;
    log("Game swapchain hooks: %d/%d", p, r);
  }
  return hr;
}
static HRESULT WINAPI factoryHook(REFIID iid, void **out) {
  realInput();
  HRESULT hr = factoryOriginal(iid, out);
  static bool supported = supportedBuild();
  if (supported && SUCCEEDED(hr) && out && *out) {
    static std::once_flag networkHooks;
    std::call_once(networkHooks, [] {
      processCompatibility();
      installNetworkHooks();
    });
    static const bool swapHooked = [&] {
      auto t = *reinterpret_cast<void ***>(*out);
      return patch(t + 10, reinterpret_cast<void *>(swapHook),
                   reinterpret_cast<void **>(&swapOriginal));
    }();
    (void)swapHooked;
  }
  return hr;
}
// Only exchange an already resolved import here. All loading, hashing and
// graphics work is deferred until the game calls an import outside the
// Windows loader lock.
static void bootstrap() {
  auto base = reinterpret_cast<unsigned char *>(GetModuleHandleW(nullptr));
  auto dos = reinterpret_cast<IMAGE_DOS_HEADER *>(base);
  auto nt = reinterpret_cast<IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
  const auto rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]
                       .VirtualAddress;
  if (!rva)
    return;
  for (auto imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR *>(base + rva);
       imp->Name; ++imp) {
    const auto library = reinterpret_cast<const char *>(base + imp->Name);
    const bool dxgi = _stricmp(library, "dxgi.dll") == 0;
    const bool winsock = _stricmp(library, "WS2_32.dll") == 0;
    if ((!dxgi && !winsock) || !imp->OriginalFirstThunk)
      continue;
    auto names = reinterpret_cast<IMAGE_THUNK_DATA *>(base + imp->OriginalFirstThunk);
    auto addresses = reinterpret_cast<IMAGE_THUNK_DATA *>(base + imp->FirstThunk);
    for (; names->u1.AddressOfData; ++names, ++addresses) {
      auto slot = reinterpret_cast<void **>(&addresses->u1.Function);
      const bool ordinal = IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal);
      // LFS can start InSim from the command line before it creates DXGI.
      // Check the executable and INI in the callback, outside the loader lock.
      if (winsock && ordinal && IMAGE_ORDINAL(names->u1.Ordinal) == 2) {
        patch(slot, reinterpret_cast<void *>(localInSimBind),
              reinterpret_cast<void **>(&bindOriginal));
        break;
      }
      if (dxgi && !ordinal) {
        auto name = reinterpret_cast<IMAGE_IMPORT_BY_NAME *>(base + names->u1.AddressOfData);
        if (std::strcmp(reinterpret_cast<const char *>(name->Name), "CreateDXGIFactory1") == 0) {
          patch(slot, reinterpret_cast<void *>(factoryHook),
                reinterpret_cast<void **>(&factoryOriginal));
          break;
        }
      }
    }
  }
}
} // namespace pad
extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE instance, DWORD version,
                                             REFIID iid, void **out,
                                             IUnknown *outer) {
  using Fn = HRESULT(WINAPI *)(HINSTANCE, DWORD, REFIID, void **, IUnknown *);
  auto mod = pad::realInput();
  auto fn = (Fn)GetProcAddress(mod, "DirectInput8Create");
  if (!fn)
    return E_FAIL;
  HRESULT hr = fn(instance, version, iid, out, outer);
  static bool enabled = pad::supportedBuild();
  if (enabled) {
    static bool hooked = []() {
      pad::installGraphicsHook();
      return true;
    }();
    (void)hooked;
    if (SUCCEEDED(hr) && out && *out) {
      auto table = *reinterpret_cast<void ***>(*out);
      std::lock_guard<std::mutex> l(pad::hooksMutex);
      if (table[3] != (void *)pad::createHook) {
        pad::inputHooks[table] = {(pad::CreateDevice)table[3],
                                 IsEqualGUID(iid, IID_IDirectInput8A) != 0};
        void *old = nullptr;
        pad::patch(table + 3, (void *)pad::createHook, &old);
      }
    }
  }
  return hr;
}
extern "C" HRESULT WINAPI DllCanUnloadNow() { return S_FALSE; }
extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID c, REFIID i, void **out) {
  using Fn = HRESULT(WINAPI *)(REFCLSID, REFIID, void **);
  auto f = (Fn)GetProcAddress(pad::realInput(), "DllGetClassObject");
  return f ? f(c, i, out) : CLASS_E_CLASSNOTAVAILABLE;
}
extern "C" HRESULT WINAPI DllRegisterServer() { return E_NOTIMPL; }
extern "C" HRESULT WINAPI DllUnregisterServer() { return E_NOTIMPL; }
extern "C" const DIDATAFORMAT *WINAPI GetdfDIJoystick() {
  using Fn = const DIDATAFORMAT *(WINAPI *)();
  auto f = (Fn)GetProcAddress(pad::realInput(), "GetdfDIJoystick");
  return f ? f() : nullptr;
}
BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, void *) {
  if (reason == DLL_PROCESS_ATTACH) {
    pad::self = h;
    DisableThreadLibraryCalls(h);
    pad::bootstrap();
  } else if (reason == DLL_PROCESS_DETACH) {
    pad::stopping.store(true);
    pad::captureButtons.store(false);
    pad::captureRaceAxes.store(false);
    pad::captureQuickInput.store(0);
    pad::monitorInput.store(false);
    pad::pedalSlot.store(-1);
    pad::nativePedals.store(0);
  }
  return TRUE;
}
