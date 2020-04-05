#ifndef KBDSCR_KBD_H
#define KBDSCR_KBD_H
#include <stddef.h>
#include <cairo/cairo.h>
#include <linux/input-event-codes.h>

// kbd_layout_key_t represents a key on a keyboard layout.
typedef struct {
    int  units;    // the number of units wide the key/spacer is
    char *label;   // the key label to use (if NULL, this key is treated as a spacer)
    int  code; // KEY_* and BTN_* from linux/input-event-codes.h
} kbd_layout_key_t;

// kbd_layout_t represents a keyboard layout. The total number of units must be
// divisible by the number of units per row without splitting keys for the
// rendering to work correctly. Keys spanning multiple rows or with different
// key heights are not supported (although a larger gap can be created with a
// full-width spacer). This struct is only used for storage; all actual
// processing is done by kbd_t.
typedef struct {
    int              units_per_row;  // i.e. grid columns
    int              units_per_base; // i.e. max number of fractions of a standard key
    int              px_per_base;    // i.e. standard key width/height
    size_t           n_keys;
    kbd_layout_key_t *keys;
} kbd_layout_t;

// kbd_t renders keyboard layouts.
typedef struct kbd_t kbd_t;

// kbd_new creates a kbd_t with the specified layout. The majority of the error
// checking is done here, so it is important that this is called. If any errors
// ocurred, the return value will be NULL, and if err is not NULL, its target
// will be set to a string describing the error (which will need to be freed by
// the caller). Otherwise, the return value will be an allocated kbd_t.
kbd_t *kbd_new(kbd_layout_t layout, char **err);

// kbd_free frees a kbd_t.
void kbd_free(kbd_t *kbd);

// kbd_set_redraw_cb sets the callback to be called whenever the keyboard
// requires redrawing. The callback is not called for the initial draw. It
// should block as shortly as possible. The callback may be called from a
// concurrently and/or from a different thread. Pass NULL as the callback to
// disable it.
void kbd_set_redraw_cb(kbd_t *kbd, void (*fn)(void*), void* data);

// kbd_set_state sets the state of a KEY_* or BTN_* to UP (0), DOWN (1), or
// HOLD (2). It safe to call concurrently and/or from multiple threads.
void kbd_set_state(kbd_t *kbd, int key, int state);

// kbd_get_rows gets the number of rows of keys in the kbd_t.
int kbd_get_rows(kbd_t *kbd);

// kbd_get_width gets the expected width of the rendered keyboard in pixels.
int kbd_get_width(kbd_t *kbd);

// kbd_get_height gets the expected height of the rendered keyboard in pixels.
int kbd_get_height(kbd_t *kbd);

// kbd_draw renders the keyboard on to the provided Cairo context.
void kbd_draw(kbd_t *kbd, cairo_t *cr);

#endif