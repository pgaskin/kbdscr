#ifndef KBDSCR_KBD_LAYOUT_H
#define KBDSCR_KBD_LAYOUT_H
#include <linux/input-event-codes.h>
#include "kbd.h"

// KBD_LAYOUT is an hacky way to initialize a kbd_layout_t at compile time
// without needing to manually count the number of keys (which is tedious and
// error-prone).
#define KBD_LAYOUT(units_per_row, units_per_base, px_per_base, ...) (kbd_layout_t){ \
    units_per_row,                                                                  \
    units_per_base,                                                                 \
    px_per_base,                                                                    \
    sizeof((kbd_layout_key_t[]){__VA_ARGS__}) / sizeof(kbd_layout_key_t),           \
    (kbd_layout_key_t[]){__VA_ARGS__},                                              \
}

// KBD_LAYOUTS calls a macro X(layout, id, desc) for each built-in layout.
#define KBD_LAYOUTS \
    X(KBD_LAYOUT_US_MOUSE,   "km-us-en",    "US English Keyboard, plus three standard mouse buttons") \
    X(KBD_LAYOUT_MOUSE_M570, "m-logi-m570", "Logitech M570 mouse")

// KBD_LAYOUT_US_MOUSE is a US English keyboard, plus three mouse buttons.
#define KBD_LAYOUT_US_MOUSE KBD_LAYOUT( \
    74, 4, 24, \
    {4, "Esc", KEY_ESC}, {6}, {4, "F1", KEY_F1}, {1}, {4, "F2", KEY_F2}, {1}, {4, "F3", KEY_F3}, {1}, {4, "F4", KEY_F4}, {4}, {4, "F5", KEY_F5}, {1}, {4, "F6", KEY_F6}, {1}, {4, "F7", KEY_F7}, {1}, {4, "F8", KEY_F8}, {3}, {4, "F9", KEY_F9}, {1}, {4, "F10", KEY_F10}, {1}, {4, "F11", KEY_F11}, {1}, {4, "F12", KEY_F12}, \
    {4, "`", KEY_GRAVE}, {1}, {4, "1", KEY_1}, {1}, {4, "2", KEY_2}, {1}, {4, "3", KEY_3}, {1}, {4, "4", KEY_4}, {1}, {4, "5", KEY_5}, {1}, {4, "6", KEY_6}, {1}, {4, "7", KEY_7}, {1}, {4, "8", KEY_8}, {1}, {4, "9", KEY_9}, {1}, {4, "0", KEY_0}, {1}, {4, "-", KEY_MINUS}, {1}, {4, "=", KEY_EQUAL}, {1}, {9, "Bksp", KEY_BACKSPACE}, \
    {6, "Tab", KEY_TAB}, {1}, {4, "Q", KEY_Q}, {1}, {4, "W", KEY_W}, {1}, {4, "E", KEY_E}, {1}, {4, "R", KEY_R}, {1}, {4, "T", KEY_T}, {1}, {4, "Y", KEY_Y}, {1}, {4, "U", KEY_U}, {1}, {4, "I", KEY_I}, {1}, {4, "O", KEY_O}, {1}, {4, "P", KEY_P}, {1}, {4, "[", KEY_LEFTBRACE}, {1}, {4, "]", KEY_RIGHTBRACE}, {1}, {7, "\"", KEY_BACKSLASH}, \
    {9, "Caps", KEY_CAPSLOCK}, {1}, {4, "A", KEY_A}, {1}, {4, "S", KEY_S}, {1}, {4, "D", KEY_D}, {1}, {4, "F", KEY_F}, {1}, {4, "G", KEY_G}, {1}, {4, "H", KEY_H}, {1}, {4, "J", KEY_J}, {1}, {4, "K", KEY_K}, {1}, {4, "L", KEY_L}, {1}, {4, ";", KEY_SEMICOLON}, {1}, {4, "'", KEY_APOSTROPHE}, {1}, {9, "Enter", KEY_ENTER}, \
    {12, "Shift", KEY_LEFTSHIFT}, {1}, {4, "Z", KEY_Z}, {1}, {4, "X", KEY_X}, {1}, {4, "C", KEY_C}, {1}, {4, "V", KEY_V}, {1}, {4, "B", KEY_B}, {1}, {4, "N", KEY_N}, {1}, {4, "M", KEY_M}, {1}, {4, ",", KEY_COMMA}, {1}, {4, ".", KEY_DOT}, {1}, {4, "/", KEY_SLASH}, {1}, {11, "Shift", KEY_RIGHTSHIFT}, \
    {5, "Ctrl", KEY_LEFTCTRL}, {1}, {5, "Sup", KEY_LEFTMETA}, {1}, {5, "Alt", KEY_LEFTALT}, {1}, {32, "Space", KEY_SPACE}, {1}, {5, "Alt", KEY_RIGHTALT}, {1}, {5, "Sup", KEY_RIGHTMETA}, {1}, {5, "Fn", KEY_FN}, {1}, {5, "Ctrl", KEY_RIGHTCTRL}, \
    {12}, {16, "Mouse Left", BTN_LEFT}, {1}, {16, "Mouse Middle", BTN_MIDDLE}, {1}, {16, "Mouse Right", BTN_RIGHT}, {12}, \
)

// KBD_LAYOUT_US_MOUSE is a Logitech M570 trackball.
#define KBD_LAYOUT_MOUSE_M570 KBD_LAYOUT( \
    48, 4, 24, \
    {4, "<", BTN_SIDE}, {1}, {12, "Left", BTN_LEFT}, {1}, {12, "Middle", BTN_MIDDLE}, {1}, {12, "Right", BTN_RIGHT}, {1}, {4, ">", BTN_EXTRA}, \
)

#endif