# Corne: nice!nano v2 + nice!view

The default branch builds both nice!nano v2 halves on ZMK v0.3.0, with
nice!view support on the left half only. The right half has no display.
The keymap and Keymap Editor layout are preserved from the last successful
`5keys` build (`c39cc1a97939837799f0c55b44cdab1bba6e49f8`). The keymap has
42 positions; the historical branch name does not select a 36-key layout.

Two physical keyboard sets are supported. Keep their firmware files separate:

| Set | Left firmware | Right firmware |
| --- | --- | --- |
| First: left nice!view, no right display | `corne-left-niceview.uf2` | `corne-right.uf2` |
| Second: standard four-pin SSD1306 OLEDs on both halves | `corne-left-oled.uf2` | `corne-right-oled.uf2` |

The OLED builds use standard Corne I2C display wiring. For the reported
half-inch displays, `corne_oled_compact` selects a 64x32 panel with a 32-column
offset. This geometry is a hardware assumption to verify on the actual panels.
The compact left OLED shows the left battery, right battery, and active
Bluetooth profile on separate rows. The right shows its battery and split link
state. `+` beside a percentage means USB power is present. As on the nice!view,
an unknown right battery displays `--`; disconnecting retains the last reading.
Both sets use the same keymap and profile names. USB Studio editing is enabled
on each set's left half. The OLED builds do not include the nice!view driver.
For a confirmed 128x32 OLED, omit `corne_oled_compact` from that target's shield
list; the wider layout also shows output/connection state and active layer.

## Build and flash

1. Open **Actions → Build Corne firmware**, select the branch, and run the
   workflow. Keymap commits also start a build.
2. Download the successful run's **firmware** artifact and unzip it.
3. Connect the left half by USB and double-tap its reset button. Copy
   `corne-left-niceview.uf2` to its `NICENANO` drive. The drive disappears when
   the firmware boots.
4. Repeat for the right half using `corne-right.uf2`.
5. Power both halves and check the displays and keys. Use firmware from the
   same run for both halves.

These are application updates. Do not erase settings, flash a settings-reset
image, or update the bootloader during a routine update. Existing Bluetooth
pairings and Studio settings should be preserved.

## Edit the layout visually

- [Keymap Editor](https://nickcoutsos.github.io/keymap-editor/): sign in with
  GitHub, select `Woop91/Corne` and the desired branch, then save and build.
- [ZMK Studio](https://zmk.studio/): connect the left half by USB to edit live.
  The left firmware includes the USB transport. As in the previous config,
  Studio locking is disabled; only connect it to trusted computers.

Studio's saved layout takes precedence over the compiled keymap. Use Studio's
**Restore Stock Settings** only when you intentionally want to replace that
saved layout with the repository's keymap.

## Display configuration

The left build includes `nice_view_adapter nice_view`, which enables display
support and disables idle blanking. The right does not include a display
driver. A successful build cannot confirm physical
wiring: if a display remains blank after flashing, check its connection and
that its chip-select wiring matches the standard nice!view adapter.

The custom left display shows the left battery (**L**) with the most recently
reported right battery (**R**) directly below it, replacing the WPM graph.
The right row shows `--` until the right half reports a level; a real 0% reading
while connected is shown as `0%`. Disconnecting the right half keeps its last
reading rather than interpreting ZMK's disconnect notification as a flat battery.
The left battery shows a bolt while USB-powered. The USB/Bluetooth indicator
and active layer remain. The middle
shows the active Bluetooth slot as **BT 1** through **BT 5**, its device label,
and whether that slot is online, offline, or unpaired.
Only the left firmware needs updating for this display change. The right half
must be powered on and connected wirelessly for its battery reports to arrive.

The widget in `src/status.c` is adapted from ZMK v0.3.0's nice!view widget;
its original license is retained in `src/LICENSE.zmk`. Shared rendering helpers
come from the same pinned ZMK checkout. `CONFIG_CORNE_BATTERY_DISPLAY` enables
right-half battery fetching and replaces the stock widget, without enabling WPM.

The configured labels are **1 = WST (this workstation), 2 = LNV (Lenovo),
3 = MBP (MacBook), 4 = DGX (DGX Spark), 5 = IPD (iPad)**. These labels describe
the existing profile slots; they do not re-pair devices or change slot assignments.

ZMK does not automatically learn a paired computer or phone's friendly name.
Edit the matching `CORNE_BT_PROFILE_1_LABEL` through
`CORNE_BT_PROFILE_5_LABEL` default string in `Kconfig` to change these labels.
Configured labels take precedence over any stored profile name. If a label is
empty, a stored name is used when available; otherwise the display says
`Unnamed`. The first 15 characters fit over two lines. Label numbers are
one-based, matching the display.

Keep the ZMK revision in `config/west.yml` and the reusable workflow version
in `.github/workflows/build.yml` aligned when upgrading.

## Original keymap notes

https://zmk.dev/docs/features/keymaps
============================================================================
ZMK- Mod_Tap
Mod-Tap Behavior
The Mod-Tap behavior sends a different keypress, depending on whether it's held or tapped.
⦁	If you hold the key for longer than 200ms, the first keycode ("mod") is sent.
⦁	If you tap the key (release before 200ms), the second keycode ("tap") is sent.
⦁	If you press another key within the 200ms, the 'mod' behavior is also activated

(&mt )

(&mt B A) 

⦁	if the key is pressed momentary, B will be the output
⦁	so if the key is held, the keyboard will output A 

============================================================================

Macro
to open a favorite tab

Option 1: Create a macro which types something like Win+R, the URL, Enter


Option 2: Bind the key to something you don't normally use, like an F13-24 key. Set up AutoHotkey or a similar program to listen for that key and run a script.

============================================================================

Modifier Funtions
2. how can I press one key to simulate two keys being pressed, such as Control + P to print something?
For the second, if one of the keys is a modifier like control, see the modifiers section on the docs website. Control+P would be &kp LC(P)

⦁	Thanks, that clarifies a lot for me. That's a modifier function right

keys:
&kp LC(P) = print
&kp LC(C) = Cut
&kp LC(P) = Copy
&kp LC(V) = Paste


============================================================================
https://zmk.dev/docs/features/combos

ZMK Combo
Combos
Combo keys are a way to combine multiple keypresses to output a different key. For example, you can hit the Q and W keys on your keyboard to output escape.
EXAMPLE - A combo could be used to switch layers when I press two others keys. Like to switch to a specialty layer

/ {
combos {
compatible = "zmk,combos";
combo_esc {
timeout-ms = <50>;
key-positions = <60 61>;               add the two key location number of the two keys I want to use
bindings = <&kp ENTER>;               Replace ENTER for the key I want
    };
  };
};


========================================================================================
Mod-Morph Behavior
The Mod-Morph behavior sends a different keypress, depending on whether a specified modifier is being held during the keypress.
⦁	If you tap the key by itself, the first keycode is sent.
⦁	If you tap the key while holding the specified modifier, the second keycode is sent.
Mod-Morph
The Mod-Morph behavior acts as one of two keycodes, depending on if the required modifier is being held during the keypress.
