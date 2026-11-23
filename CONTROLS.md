# LfsPad controller guide

[Türkçe](KONTROLLER.md) · **English**

The **Controller icons** setting shows or hides the routine corner prompts. Binding, shortcut and confirmation prompts still use plain text when icons are off.

## Menus and text entry

| Action | Default control |
|---|---|
| Move between items | D-pad or left stick |
| Select / go back | A / B |
| Change tabs or turn a page | LB / RB |
| Open the race menu | Start |
| Open LfsPad settings | Hold LT and RT together in a menu |
| Switch to the pointer | Click the right stick. Move with the right stick, click with A, right-click with X, or hold A to drag. Scroll with D-pad up/down. |
| Type | Y opens the keyboard or changes case. X deletes a character. Start submits the text. |

On PlayStation, use Cross, Circle, Square and Triangle for A, B, X and Y. L1/R1 are the shoulder buttons and L2/R2 are the triggers. Use Options for Start and Share or Create for Back. Sixaxis and DualShock 3 use Start and Select.

Open **Options → LfsPad**, or hold both triggers in a menu. Use **LB/RB** to change categories. In **General → Language**, press left/right to switch between Turkish and English. Press A to save or B to discard.

The keyboard opens when you select **Search** or **Author** in Mods. **Done** applies the filter and **B** closes the editor. You can click the keys with the mouse. Text appears in the game's own field, which handles password masking.

Use left/right in **General → Player profile** to choose from three profiles. The first time you select a profile, it copies your current LfsPad controller settings. Button bindings, stick deadzone, cursor speed, vibration, icons and menu shortcuts are stored per profile. Press **X** to rename with the on-screen keyboard, **Start** to accept the name, then **A** to save. **B** on the settings screen discards unsaved changes. Manage driving bindings in LFS's own **Options → Controls** menu.

## Shortcuts

Hold Back, press a direction, then release Back and press A to open the highlighted item.

| Screen | Up | Down | Left | Right |
|---|---|---|---|---|
| Main menu | Single player | Options | Replays | Multiplayer |
| Single-player setup | Garage | Go | Track | Driver |
| Garage | Setups | OK | Colours | Viewer |
| Options | Controls | OK | Display | Graphics |
| Pause menu | Resume | Pit | Options | Show replay |

In **Shortcuts → Custom menu shortcuts**, move up/down to pick a row and left/right to change its target. Assigning a target already in use swaps the two directions. Choose **Off** to clear one, or press X to restore defaults for that screen. Press A to apply and A again in settings to save. Unavailable targets are dimmed.

Assign **Axes / FF** to any direction in the main or pause menu to go straight to the pedal axes settings. In the garage, assign **Skin list** to open the list from any tab. LFS requires an editable colour for skin selection. Pick the direction and press A to open the favorite.

To use the buttons one at a time, save **Tap then choose** under **Shortcuts → Shortcut input**. Tap and release Back, choose a direction, then press A. Tap Back again or press B to dismiss the panel. If Back is bound to the D-pad, use the left stick to reach all four directions.

To change a button, highlight its action in the **Controls** tab, press X, release all buttons and press the new button. You can assign any of the four D-pad directions. Hold both triggers to cancel an assignment. Use the left stick to navigate when a direction is assigned to an action. A and B always save and cancel on the settings screen.

For driving bindings, open the game's **Options → Controls → Buttons 1 / 2**. Select the controller binding for the action, release the button that opened it, then press a D-pad direction.

## Garage and lists

Change tabs with LB/RB. Pick a setup, colour or skin with up/down, then press A. Hold a direction at the edge to scroll the selected list. On a colour slider, press A, adjust with left/right, then press A or B to leave the slider. In the viewer, rotate the car with the right stick, zoom out with LT or in with RT, and return to the garage with B. After switching windows or reconnecting, release the stick and triggers before continuing.

In replay, server and mod lists, hold up/down to scroll faster or use LB/RB to turn a page.

For RFS, open LFS Options and press **LB + RB** together. Move the pointer to the **RFS** tab with the right stick and press **A** to open it. Hold A to drag or use D-pad up/down to scroll. Press **B** to leave pointer mode.

## On track

Hold Back and point the right stick up for the camera, left for mirrors or right for pit information. Release Back to apply. Center the stick before releasing to cancel. The quick menu can also use L3 or R3, or be turned off. Hold the confirm button for about 0.7 seconds to end a race. A single press resumes from the pause menu.

If the active controller disconnects during a solo race, reconnect and confirm to resume. An online race continues. During a replay, A pauses or plays, LT/RT seek five seconds backward/forward, and B or Start opens the replay menu.

## Vibration and input display

Open **LT + RT → General → Test my controller** to check each D-pad direction and both triggers one at a time. Both trigger readings are shown live. Press A for a short pulse at each vibration step, then use A if you felt it or X if you didn't. A failed vibration command appears as a separate result. Use X to skip a step or B to go back to settings.

In **Driving → Driving vibration**, adjust strength with left/right, test with X and save with A. The default is 50%. Set it to 0% to turn it off. On the F710, enable vibration on the controller itself. The test shows whether the driver accepted the vibration command. With several gamepads in **Auto**, vibration follows the one sending steering or pedal input. Choose a fixed source on the **Controller** row if needed. XInput and SDL gamepad vibration are supported. A single unmapped DirectInput gamepad can receive driving vibration when its driver supports force feedback. For PlayStation, use USB for vibration. Engine speed, gear shifts, sliding and impacts drive the vibration. OutSim adds wheel and suspension detail when available.

Enable **Live input display** to compare your controller input in the lower-left corner with what LFS receives in the lower-right corner. Both displays have transparent backgrounds. An amber outline marks a filtered button. If LFS doesn't show separate pedals, follow the axis setup in the [installation guide](README.en.md#separate-throttle-and-brake).

## Steering and clutch

Open **LT + RT → Driving → Steering assist**. Choose **Balanced**, **Strong**, or **Off**, then press A to save. The default **Balanced** setting keeps full lock for parking and reversing. Small stick movements stay precise at speed. Large movements are softened further when the front tyres begin to scrub. **Strong** adds more assistance.

LfsPad compares stick movement in both directions with the steering angle reported by LFS. When the angle is unavailable, it learns from steady corners. Countersteer fades as the slide settles. A deliberate correction into the slide opens up the steering range so you can catch it yourself.

**Steering curve** is a separate setting in the same tab. **Soft** and **Extra soft** reduce small movements around the middle of the left stick. You can use both settings together, and both are saved in your player profile. They apply while driving with one connected gamepad using the left stick's horizontal axis for steering.

From the pause menu, hold LT and RT and select **Driving → Auto clutch**. Press A to focus LFS's own switch in **Options → Driver**, then press A to turn it on or off. In sequential mode, you still choose gears yourself. LFS shows this switch when the car and gearbox support it.

## Older controllers

With Sony's `sixaxis.sys` driver, set `PlayStation3SonyDriver=1` under `[Input]` in `LfsPad.ini` and restart LFS. If several controllers are connected, use the Controller row in settings to choose the input source. Separate synthetic sliders require a single active controller matched to the game's DirectInput device.
