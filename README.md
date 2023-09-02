# zmk-config2
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
questions:zmk- to open a favorite tab
1. is there a way to open a specific website, by just pressing a key?
Option 1:
Create a macro which types something like Win+R, the URL, Enter


Option 2;
Bind the key to something you don't normally use, like an F13-24 key. Set up AutoHotkey or a similar program to listen for that key and run a script.

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
