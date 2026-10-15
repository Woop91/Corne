# Corne: nice!nano v2 + nice!view

The default branch builds both halves with nice!view support on ZMK v0.3.0.
The keymap and Keymap Editor layout are preserved from the last successful
`5keys` build (`c39cc1a97939837799f0c55b44cdab1bba6e49f8`). The keymap has
42 positions; the historical branch name does not select a 36-key layout.

## Build and flash

1. Open **Actions → Build Corne firmware**, select the branch, and run the
   workflow. Keymap commits also start a build.
2. Download the successful run's **firmware** artifact and unzip it.
3. Connect the left half by USB and double-tap its reset button. Copy
   `corne-left-niceview.uf2` to its `NICENANO` drive. The drive disappears when
   the firmware boots.
4. Repeat for the right half using `corne-right-niceview.uf2`.
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

Both build targets include `nice_view_adapter nice_view`; display support is
enabled in `config/corne.conf`. A successful build cannot confirm physical
wiring: if a display remains blank after flashing, check its connection and
that its chip-select wiring matches the standard nice!view adapter.

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
