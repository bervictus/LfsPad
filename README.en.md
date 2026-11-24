# LfsPad

[Türkçe](README.md) · **English**

Use a controller to start a race, change your setup in the garage and get back on track in Live for Speed. LfsPad loads with the game and works with **LFS 0.8C38 and 0.8C39 for 32-bit Windows**.

Version 0.3.0 brings a controller steering assist inspired by LFS’s stabilised keyboard mode. You keep full lock when parking, and the assist can countersteer during a slide. The steering range opens up when you turn the stick to catch the slide yourself. [Controller guide](CONTROLS.md)

## Install or update

1. Close LFS and open the `-rfs-setup.exe` installer for your version. Published packages are on the [releases page](https://github.com/bervictus/LfsPad/releases).
2. Choose **English** or **Türkçe** at the top right and click **Next**. Check the detected folder or use **Browse** to select the folder containing `LFS.exe`.
3. Click **Install**, or **Update** if LfsPad is already installed. Then launch LFS as usual.

If you have 0.4.0 or later, you can update from inside LFS. For older builds, choose **Update** in the GitHub installer.

If you installed the first 0.2.0 build, run the new installer once and choose **Update**. This moves LfsPad to a separate file so RFS can update its own loader. To uninstall, open the same installer, select the same folder and click **Uninstall LfsPad**. Updates keep your settings. Setup uses the InSim port already set in the game’s startup script.

## Getting started

Move with the D-pad or left stick. Press A to select and B to go back (Cross and Circle on PlayStation). Click the right stick for a mouse pointer or press Y (Triangle) for the on-screen keyboard.

Hold both triggers in a menu to open LfsPad settings. Use **LB/RB** to switch between **General**, **Controls**, **Driving** and **Shortcuts**. In **General → Language**, use left or right to preview the language, A to save or B to cancel. This also switches the keyboard between English QWERTY and Turkish Q. Hold Back to see shortcuts for the current screen. [Controller guide](CONTROLS.md)

## RFS and updates

With RFS installed, press **LB + RB** together in an LFS menu for the RFS pointer. Move with the right stick, click or drag with **A**, and scroll with the D-pad. Press **B** to return to item navigation.

Select **LfsPad** in the game's **Options** list. Holding **LT + RT** in a menu opens the same page. In **Updates**, choose **Check for updates now** for a manual check. Launch checks include GitHub pre-releases and can be turned off on the same page. When the update prompt appears outside a race, choose **OK** to download and verify the package, install it, and reopen LFS.

## Controllers

| Controller | Connection |
|---|---|
| Xbox and other XInput controllers | For a Logitech F710, set the rear switch to **X**. |
| Other gamepads recognized by SDL | Connection and vibration depend on the model and Windows driver. |
| DualShock 4, DualSense and DualSense Edge | USB or Bluetooth. Use USB for vibration with a compatible driver. |
| Sixaxis and DualShock 3 | Requires a compatible Windows driver. |
| PS1 and PS2 controllers | Requires a USB adapter with a supported button and axis mapping. |

The F710 has been tested in LFS. PlayStation input has been tested with virtual controllers. Physical USB and Bluetooth tests are pending.

Controller icons and hints appear in both bottom corners. Hold **LT + RT**, change **Controller icons**, and press **A** to save. In LFS **Options → Controls**, select a driving action's controller binding, then press a D-pad direction to assign it.

## Separate throttle and brake

In **Options → Controls**, set the throttle and brake axes to **separate**. On **Axes / FF**, assign throttle to **Slider 0** (right trigger) and brake to **Slider 1** (left trigger). Set both axes to the normal direction. Squeeze both triggers and check that both bars move. If the sliders are unavailable, use the native axes listed by LFS.

## Manual installation and builds

With LFS closed, unzip the Windows package and run `./tools/install.ps1 -GameDirectory 'C:\LFS'` from PowerShell in that folder. Add `-Update` for an existing installation, or run `./tools/uninstall.ps1` with the same game directory to remove it. InSim accepts connections from this PC by default. For a remote InSim client, set `AllowRemoteInSim=1` in the `[General]` section of `LfsPad.ini` and restart LFS.

To build from source on Windows, install CMake and Ninja and run these commands from the project folder. The installer appears under `artifacts`.

```powershell
./tools/build.ps1
./tools/build-setup.ps1
```
