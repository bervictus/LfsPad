#pragma once
#include "core.hpp"
#include "lfs_profile.hpp"
#include <cstdint>

namespace pad {
// The menu descriptor layout of the verified 32-bit LFS executables.
struct NativeItem {
  Rect rect;
  unsigned value, text, extra, style;
  unsigned char type, pad[3], codepage, pad2[3];
  unsigned colour, owner, callback;
};
static_assert(sizeof(NativeItem) == 52);

namespace nativeOptions {
enum Action : unsigned {
  None, Save, Cancel, SelectTab = 10, SelectRow = 20,
  Decrease = 60, Increase = 100, Assign = 140
};
bool install(unsigned char *image, const LfsBuildProfile &profile);
bool canOpen();
bool active();
void open();
void leave();
bool isCategory(const NativeItem &item);
unsigned takeAction();
void button(Rect rect, const char *caption, unsigned action = None,
            bool highlighted = false, bool category = false);
}
void paintNativeSettings();
}
