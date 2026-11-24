#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace pad {
// Build-specific executable hashes and RVAs for supported 32-bit LFS releases.
struct LfsBuildProfile {
  const char *version = nullptr;
  std::array<unsigned char, 32> sha256{};

  // Core menu / modal / text globals
  std::uintptr_t buttonCount = 0;
  std::uintptr_t buttonTable = 0;
  std::uintptr_t modal = 0;
  std::uintptr_t nativeTextDialog = 0;
  std::uintptr_t selectedColour = 0;

  // Native Options RVAs
  std::uintptr_t optionsWindow = 0;
  std::uintptr_t optionsRoot = 0;
  std::uintptr_t optionsRender = 0;
  std::uintptr_t optionsSelect = 0;
  std::uintptr_t optionsRenderOperand = 0;
  std::uintptr_t optionsSelectOperand = 0;
  std::uintptr_t optionsNativeSelect = 0;
  std::uintptr_t optionsSelected = 0;
  std::uintptr_t optionsDisplayed = 0;
  std::uintptr_t windowClose = 0;
  std::uintptr_t windowKey = 0;
  std::uintptr_t windowMouse = 0;
  std::uintptr_t windowAttachChild = 0;
  std::uintptr_t buttonRegister = 0;
  std::uintptr_t paletteHeader = 0;
  std::uintptr_t paletteNormal = 0;

  // Screen, list, garage, and controls callback RVAs
  std::uintptr_t mainMenuCallback = 0;
  std::uintptr_t raceMenuCallback = 0;
  std::uintptr_t singlePlayerNavCallback = 0;
  std::uintptr_t singlePlayerGoCallback = 0;
  std::uintptr_t controlsTabCallback = 0;
  std::uintptr_t controlsAssignCallback = 0;
  std::uintptr_t autoClutchCallback = 0;
  std::uintptr_t autoClutchValue = 0;
  std::uintptr_t garageOkCallback = 0;
  std::uintptr_t garageTabCallback = 0;
  std::uintptr_t garageViewerCallback = 0;
  std::uintptr_t garageSkinsOpenCallback = 0;
  std::uintptr_t garageSkinPrefixCallback = 0;
  std::uintptr_t garageSkinsModalCallback = 0;
  std::uintptr_t garageSkinsRowCallback = 0;
  std::uintptr_t garageSetupCallback1 = 0;
  std::uintptr_t garageSetupCallback2 = 0;
  std::uintptr_t garageColourCallback = 0;
  std::uintptr_t garagePresetCallback = 0;
  std::uintptr_t garageViewerLightCallback = 0;
  std::uintptr_t garageViewerLodCallback = 0;
  std::uintptr_t garageColourSlider1 = 0;
  std::uintptr_t garageColourSlider2 = 0;
  std::uintptr_t garageViewerSlider = 0;
  std::uintptr_t modsSearchClearCallback = 0;
  std::uintptr_t replaysListCallback = 0;
  std::uintptr_t serversListCallback = 0;
  std::uintptr_t modsListCallback = 0;
};

using ProfileRva = std::uintptr_t LfsBuildProfile::*;

inline constexpr std::uintptr_t resolveProfileRva(const LfsBuildProfile &profile,
                                                  ProfileRva member) {
  return member ? profile.*member : 0;
}

inline constexpr std::array<LfsBuildProfile, 2> lfsProfiles{{
    {
        "0.8C38",
        {0x77, 0x60, 0x6E, 0x57, 0x88, 0x5F, 0xEC, 0x33, 0xCD, 0xDA, 0xBD,
         0xFD, 0x22, 0x71, 0xD2, 0xF1, 0x81, 0xC6, 0x58, 0x06, 0xC2, 0xD7,
         0xEC, 0x00, 0x49, 0x65, 0x4B, 0xD5, 0x48, 0x7D, 0x8A, 0x52},
        0x28c994, // buttonCount
        0x28ca20, // buttonTable
        0x49c298, // modal
        0x95ec20, // nativeTextDialog
        0x43b330, // selectedColour
        0x49129c, // optionsWindow
        0x491298, // optionsRoot
        0x1112b0, // optionsRender
        0x111820, // optionsSelect
        0x111222, // optionsRenderOperand
        0x11130e, // optionsSelectOperand
        0x111760, // optionsNativeSelect
        0x2af52c, // optionsSelected
        0x2af530, // optionsDisplayed
        0x128ab0, // windowClose
        0x12ba40, // windowKey
        0x12baf0, // windowMouse
        0x1289a0, // windowAttachChild
        0x2ce80,  // buttonRegister
        0x43b378, // paletteHeader
        0x43b380, // paletteNormal
        0x84750,  // mainMenuCallback
        0x8e310,  // raceMenuCallback
        0xac200,  // singlePlayerNavCallback
        0xac0c0,  // singlePlayerGoCallback
        0x10aba0, // controlsTabCallback
        0x10abc0, // controlsAssignCallback
        0xc8eb0,  // autoClutchCallback
        0x2805b8, // autoClutchValue
        0xb3e00,  // garageOkCallback
        0x141220, // garageTabCallback
        0xb1780,  // garageViewerCallback
        0xb5740,  // garageSkinsOpenCallback
        0x141340, // garageSkinPrefixCallback
        0x13dba0, // garageSkinsModalCallback
        0x13dbe0, // garageSkinsRowCallback
        0xb4420,  // garageSetupCallback1
        0xb4440,  // garageSetupCallback2
        0xb4650,  // garageColourCallback
        0xb4640,  // garagePresetCallback
        0xb2dd0,  // garageViewerLightCallback
        0xb2de0,  // garageViewerLodCallback
        0xb4870,  // garageColourSlider1
        0xb4920,  // garageColourSlider2
        0xb2e00,  // garageViewerSlider
        0x4c950,  // modsSearchClearCallback
        0x1372d0, // replaysListCallback
        0xa6d60,  // serversListCallback
        0xf3f60,  // modsListCallback
    },
    {
        "0.8C39",
        {0xB7, 0xE3, 0xBB, 0x23, 0x28, 0x8C, 0x90, 0x7F, 0x50, 0x60, 0x07,
         0x5B, 0xCD, 0xD9, 0xAC, 0xCF, 0x9B, 0x7B, 0xB2, 0x6E, 0xD5, 0x7A,
         0xD7, 0x6B, 0xBE, 0x3A, 0x34, 0x04, 0xDA, 0x76, 0xE8, 0x81},
        0x28c994, // buttonCount
        0x28ca20, // buttonTable
        0x49c298, // modal
        0x95ec40, // nativeTextDialog
        0x43b330, // selectedColour
        0x49129c, // optionsWindow
        0x491298, // optionsRoot
        0x111600, // optionsRender
        0x111b70, // optionsSelect
        0x111572, // optionsRenderOperand
        0x11165e, // optionsSelectOperand
        0x111ab0, // optionsNativeSelect
        0x2af52c, // optionsSelected
        0x2af530, // optionsDisplayed
        0x128e40, // windowClose
        0x12bdd0, // windowKey
        0x12be80, // windowMouse
        0x128d30, // windowAttachChild
        0x02cea0, // buttonRegister
        0x43b378, // paletteHeader
        0x43b380, // paletteNormal
        0x084d80, // mainMenuCallback
        0x08e940, // raceMenuCallback
        0x0ac8b0, // singlePlayerNavCallback
        0x0ac770, // singlePlayerGoCallback
        0x10af00, // controlsTabCallback
        0x10af20, // controlsAssignCallback
        0x0c9550, // autoClutchCallback
        0x2805bc, // autoClutchValue
        0x0b44a0, // garageOkCallback
        0x1415a0, // garageTabCallback
        0x0b1e20, // garageViewerCallback
        0x0b5dd0, // garageSkinsOpenCallback
        0x1416c0, // garageSkinPrefixCallback
        0x13df20, // garageSkinsModalCallback
        0x13df60, // garageSkinsRowCallback
        0x0b4ab0, // garageSetupCallback1
        0x0b4ad0, // garageSetupCallback2
        0x0b4ce0, // garageColourCallback
        0x0b4cd0, // garagePresetCallback
        0x0b3470, // garageViewerLightCallback
        0x0b3480, // garageViewerLodCallback
        0x0b4f00, // garageColourSlider1
        0x0b4fb0, // garageColourSlider2
        0x0b34a0, // garageViewerSlider
        0x04c960, // modsSearchClearCallback
        0x137650, // replaysListCallback
        0x0a7410, // serversListCallback
        0x0f47c0, // modsListCallback
    },
}};

inline const LfsBuildProfile *findLfsProfile(const std::array<unsigned char, 32> &digest) {
  for (const auto &profile : lfsProfiles)
    if (std::equal(profile.sha256.begin(), profile.sha256.end(), digest.begin()))
      return &profile;
  return nullptr;
}
} // namespace pad
