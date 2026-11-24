#include "native_options.hpp"
#include "runtime.hpp"
#include <array>
#include <cstring>

namespace pad::nativeOptions {
namespace {
struct Window;
using Renderer = Window *(__cdecl *)(Window *);
using Event = void (__cdecl *)(Window *, unsigned, unsigned);
using Close = void (__cdecl *)(Window *);

// LFS attaches one child window to Options at a time and dispatches mouse
// actions through that window's button descriptors.
struct Window {
  Window *parent, *child;
  unsigned char active, codepage, reserved, state;
  const void *theme;
  Renderer render;
  Event key, mouse;
  Close close;
  unsigned stateData, pendingPage;
};
static_assert(sizeof(Window) == 0x28 && offsetof(Window, pendingPage) == 0x24);

constexpr unsigned settingsPage = 11;
unsigned char *base = nullptr;
const LfsBuildProfile *build = nullptr;
Window page{};
unsigned previousPage = 1;
bool installed = false, openRequested = false;
std::atomic<unsigned> action{None};
using ButtonText = std::array<char, 160>;
std::array<ButtonText, 64> captions;
std::size_t captionCount = 0;

template <typename T> T &data(uintptr_t offset) {
  return *reinterpret_cast<T *>(base + offset);
}
template <typename T> T function(uintptr_t offset) {
  return reinterpret_cast<T>(base + offset);
}
Window *owner() { return reinterpret_cast<Window *>(base + build->optionsWindow); }
bool optionsOpen() {
  return installed && build && data<Window *>(build->optionsRoot) == owner() && owner()->active;
}
void __cdecl onButton(Window *window, unsigned, unsigned value) {
  if (window == &page && page.active)
    action.store(value);
}
void __cdecl selectCategory(Window *window, unsigned, unsigned) {
  if (window == owner() && optionsOpen())
    openRequested = true;
}
void addButton(Window *window, Rect rect, const char *text, Event click, unsigned value,
               bool highlighted, unsigned type) {
  constexpr int capacity = 1024;
  auto &count = data<int>(build->buttonCount);
  if (count < 0 || count >= capacity || captionCount >= captions.size())
    return;
  // Text is kept alive between drawing and LFS processing the next click.
  // LFS uses western codepage 0 and Turkish codepage 2 for these captions.
  auto &caption = captions[captionCount++];
  wchar_t unicode[160]{};
  unsigned codepage = 0;
  if (const auto length = MultiByteToWideChar(CP_UTF8, 0, text, -1, unicode, _countof(unicode))) {
    for (int i = 0; i < length; ++i) {
      if (unicode[i] == L'^')
        unicode[i] = L' ';
      if (unicode[i] == L'ğ' || unicode[i] == L'Ğ' || unicode[i] == L'ş' || unicode[i] == L'Ş' ||
          unicode[i] == L'ı' || unicode[i] == L'İ')
        codepage = 2;
    }
    WideCharToMultiByte(codepage ? 1254 : 1252, 0, unicode, -1, caption.data(),
                        int(caption.size()), nullptr, nullptr);
  } else {
    caption[0] = '\0';
  }
  auto &item = data<NativeItem[capacity]>(build->buttonTable)[count++];
  item = {};
  item.rect = rect;
  item.value = value;
  item.text = reinterpret_cast<uintptr_t>(caption.data());
  item.type = static_cast<unsigned char>(type);
  item.codepage = static_cast<unsigned char>(codepage);
  const uintptr_t palette = type == 10 ? build->paletteHeader
                          : highlighted ? build->selectedColour : build->paletteNormal;
  item.colour = reinterpret_cast<uintptr_t>(base + palette);
  item.owner = reinterpret_cast<uintptr_t>(window);
  item.callback = reinterpret_cast<uintptr_t>(click);
  function<void (__fastcall *)(NativeItem *)>(build->buttonRegister)(&item);
}
Window *__cdecl drawPage(Window *window) {
  if (!window->active)
    return nullptr;
  addButton(nullptr, {-1.f, -1.f, 1.f, -.85f}, "LfsPad", nullptr, 0, false, 10);
  paintNativeSettings();
  return window;
}
void __cdecl pageKey(Window *window, unsigned key, unsigned value) {
  if (key == VK_ESCAPE)
    action.store(Cancel);
  else
    function<Event>(build->windowKey)(window, key, value);
}
void __fastcall openChild(Window *window) {
  if (window != owner() || window->pendingPage != settingsPage) {
    function<void (__fastcall *)(Window *)>(build->optionsSelect)(window);
    return;
  }
  const auto selected = data<unsigned>(build->optionsSelected);
  if (selected >= 1 && selected < settingsPage)
    previousPage = selected;
  page = {};
  page.parent = window;
  page.active = 1;
  page.state = 1;
  page.codepage = window->codepage;
  page.theme = window->theme;
  page.render = drawPage;
  page.key = pageKey;
  page.mouse = function<Event>(build->windowMouse);
  page.close = function<Close>(build->windowClose);
  function<void (__fastcall *)(Window *, Window *)>(build->windowAttachChild)(window, &page);
  data<Window *>(build->modal) = &page;
  data<unsigned>(build->optionsSelected) = data<unsigned>(build->optionsDisplayed) = settingsPage;
  window->pendingPage = 0;
  log("Native LFS Options opened LfsPad");
}
Window *__cdecl drawOptions(Window *window) {
  captionCount = 0;
  if (openRequested && window == owner() && window->active && window->child &&
      !window->child->child && window->child != &page) {
    window->child->close(window->child);
    window->pendingPage = settingsPage;
    openRequested = false;
  }
  if (!window->active)
    openRequested = false;
  const int first = data<int>(build->buttonCount);
  auto *result = function<Renderer>(build->optionsRender)(window);
  if (!result && data<unsigned>(build->optionsSelected) == settingsPage)
    data<unsigned>(build->optionsSelected) = previousPage;
  if (!result || window != owner() || first < 0)
    return result;

  const auto &buttons = data<NativeItem[1024]>(build->buttonTable);
  const int count = std::min(data<int>(build->buttonCount), 1024);
  Rect category{}, extra{};
  for (int i = first; i < count; ++i) {
    const auto &item = buttons[i];
    if (item.owner != reinterpret_cast<uintptr_t>(window) || item.type != 4)
      continue;
    if (item.value == 9 && item.callback == reinterpret_cast<uintptr_t>(base + build->optionsNativeSelect))
      category = item.rect;
    if (item.value == 10)
      extra = item.rect; // RFS appends its own category here.
  }
  if (category.r <= category.l || category.b <= category.t)
    return result;
  const float height = category.b - category.t;
  if (extra.r > extra.l) {
    category.l += (category.r - category.l) * .48f;
    category.t = extra.b + height * .50f;
  } else
    category.t = category.b + height * .29f;
  category.b = category.t + height;
  for (int i = first; i < count; ++i) {
    const auto &rect = buttons[i].rect;
    if (buttons[i].callback && rect.l < category.r && rect.r > category.l &&
        rect.t < category.b && rect.b > category.t)
      return result;
  }
  addButton(window, category, "LfsPad", selectCategory, settingsPage, active(), 4);
  return result;
}
bool replaceOperand(uintptr_t offset, std::uint32_t value) {
  auto *operand = base + offset;
  DWORD protection = 0;
  if (!VirtualProtect(operand, sizeof(value), PAGE_EXECUTE_READWRITE, &protection))
    return false;
  std::memcpy(operand, &value, sizeof(value));
  FlushInstructionCache(GetCurrentProcess(), operand, sizeof(value));
  VirtualProtect(operand, sizeof(value), protection, &protection);
  return true;
}
std::uint32_t operandAt(uintptr_t offset) {
  std::uint32_t value = 0;
  std::memcpy(&value, base + offset, sizeof(value));
  return value;
}
bool hasRequiredProfile(const LfsBuildProfile &profile) {
  return profile.optionsWindow && profile.optionsRoot && profile.optionsRender &&
         profile.optionsSelect && profile.optionsRenderOperand &&
         profile.optionsSelectOperand && profile.optionsNativeSelect &&
         profile.optionsSelected && profile.optionsDisplayed &&
         profile.modal && profile.buttonCount && profile.buttonTable &&
         profile.windowClose && profile.windowKey && profile.windowMouse &&
         profile.windowAttachChild && profile.buttonRegister &&
         profile.paletteHeader && profile.selectedColour && profile.paletteNormal;
}
} // namespace

bool install(unsigned char *image, const LfsBuildProfile &profile) {
  if (installed)
    return true;
  if (!hasRequiredProfile(profile))
    return false;
  base = image;
  build = &profile;
  const auto originalRender = reinterpret_cast<uintptr_t>(base + build->optionsRender);
  const auto originalSelect = build->optionsSelect - (build->optionsSelectOperand + 4);
  if (base[build->optionsRenderOperand - 1] != 0x68 ||
      base[build->optionsSelectOperand - 1] != 0xe8 ||
      operandAt(build->optionsRenderOperand) != originalRender ||
      operandAt(build->optionsSelectOperand) != originalSelect) {
    build = nullptr;
    return false;
  }
  const auto relative = reinterpret_cast<uintptr_t>(openChild) -
                        reinterpret_cast<uintptr_t>(base + build->optionsSelectOperand + 4);
  if (!replaceOperand(build->optionsSelectOperand, relative)) {
    build = nullptr;
    return false;
  }
  if (!replaceOperand(build->optionsRenderOperand, reinterpret_cast<uintptr_t>(drawOptions))) {
    replaceOperand(build->optionsSelectOperand, originalSelect);
    build = nullptr;
    return false;
  }
  installed = true;
  if (optionsOpen() && owner()->render == function<Renderer>(build->optionsRender))
    owner()->render = drawOptions;
  return true;
}
bool canOpen() { return optionsOpen() && owner()->child && !owner()->child->child; }
bool active() { return optionsOpen() && owner()->child == &page && page.active; }
void open() { openRequested = canOpen(); }
void leave() {
  if (active())
    function<Event>(build->optionsNativeSelect)(owner(), 0, previousPage);
}
bool isCategory(const NativeItem &item) {
  return installed && item.owner == reinterpret_cast<uintptr_t>(owner()) &&
         item.callback == reinterpret_cast<uintptr_t>(selectCategory) && item.value == settingsPage;
}
unsigned takeAction() { return action.exchange(None); }
void button(Rect rect, const char *caption, unsigned action, bool highlighted, bool category) {
  addButton(&page, rect, caption, action ? onButton : nullptr, action, highlighted,
            category ? 4u : 0u);
}
}
