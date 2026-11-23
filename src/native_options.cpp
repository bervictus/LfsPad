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
constexpr uintptr_t optionsWindowRva = 0x49129c, optionsRootRva = 0x491298;
constexpr uintptr_t renderRva = 0x1112b0, selectRva = 0x111820;
constexpr uintptr_t renderOperandRva = 0x111222, selectOperandRva = 0x11130e;
constexpr uintptr_t nativeSelectRva = 0x111760;
constexpr uintptr_t selectedRva = 0x2af52c, displayedRva = 0x2af530;
constexpr uintptr_t modalRva = 0x49c298;
constexpr uintptr_t buttonCountRva = 0x28c994, buttonTableRva = 0x28ca20;
constexpr uintptr_t closeRva = 0x128ab0;
constexpr uintptr_t keyRva = 0x12ba40, mouseRva = 0x12baf0;
unsigned char *base = nullptr;
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
Window *owner() { return reinterpret_cast<Window *>(base + optionsWindowRva); }
bool optionsOpen() {
  return installed && data<Window *>(optionsRootRva) == owner() && owner()->active;
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
  auto &count = data<int>(buttonCountRva);
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
  auto &item = data<NativeItem[capacity]>(buttonTableRva)[count++];
  item = {};
  item.rect = rect;
  item.value = value;
  item.text = reinterpret_cast<uintptr_t>(caption.data());
  item.type = static_cast<unsigned char>(type);
  item.codepage = static_cast<unsigned char>(codepage);
  const uintptr_t palette = type == 10 ? 0x43b378 : highlighted ? 0x43b330 : 0x43b380;
  item.colour = reinterpret_cast<uintptr_t>(base + palette);
  item.owner = reinterpret_cast<uintptr_t>(window);
  item.callback = reinterpret_cast<uintptr_t>(click);
  function<void (__fastcall *)(NativeItem *)>(0x2ce80)(&item);
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
    function<Event>(keyRva)(window, key, value);
}
void __fastcall openChild(Window *window) {
  if (window != owner() || window->pendingPage != settingsPage) {
    function<void (__fastcall *)(Window *)>(selectRva)(window);
    return;
  }
  const auto selected = data<unsigned>(selectedRva);
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
  page.mouse = function<Event>(mouseRva);
  page.close = function<Close>(closeRva);
  function<void (__fastcall *)(Window *, Window *)>(0x1289a0)(window, &page);
  data<Window *>(modalRva) = &page;
  data<unsigned>(selectedRva) = data<unsigned>(displayedRva) = settingsPage;
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
  const int first = data<int>(buttonCountRva);
  auto *result = function<Renderer>(renderRva)(window);
  if (!result && data<unsigned>(selectedRva) == settingsPage)
    data<unsigned>(selectedRva) = previousPage;
  if (!result || window != owner() || first < 0)
    return result;

  const auto &buttons = data<NativeItem[1024]>(buttonTableRva);
  const int count = std::min(data<int>(buttonCountRva), 1024);
  Rect category{}, extra{};
  for (int i = first; i < count; ++i) {
    const auto &item = buttons[i];
    if (item.owner != reinterpret_cast<uintptr_t>(window) || item.type != 4)
      continue;
    if (item.value == 9 && item.callback == reinterpret_cast<uintptr_t>(base + nativeSelectRva))
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
} // namespace

bool install(unsigned char *image) {
  if (installed)
    return true;
  base = image;
  const auto originalRender = reinterpret_cast<uintptr_t>(base + renderRva);
  const auto originalSelect = selectRva - (selectOperandRva + 4);
  if (base[renderOperandRva - 1] != 0x68 || base[selectOperandRva - 1] != 0xe8 ||
      operandAt(renderOperandRva) != originalRender ||
      operandAt(selectOperandRva) != originalSelect)
    return false;
  const auto relative = reinterpret_cast<uintptr_t>(openChild) -
                        reinterpret_cast<uintptr_t>(base + selectOperandRva + 4);
  if (!replaceOperand(selectOperandRva, relative))
    return false;
  if (!replaceOperand(renderOperandRva, reinterpret_cast<uintptr_t>(drawOptions))) {
    replaceOperand(selectOperandRva, originalSelect);
    return false;
  }
  installed = true;
  if (optionsOpen() && owner()->render == function<Renderer>(renderRva))
    owner()->render = drawOptions;
  return true;
}
bool canOpen() { return optionsOpen() && owner()->child && !owner()->child->child; }
bool active() { return optionsOpen() && owner()->child == &page && page.active; }
void open() { openRequested = canOpen(); }
void leave() {
  if (active())
    function<Event>(nativeSelectRva)(owner(), 0, previousPage);
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
