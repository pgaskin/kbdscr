#define _GNU_SOURCE
#include <assert.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cairo/cairo.h>
#include <linux/input-event-codes.h>

#include "kbd.h"

struct kbd_t {
    kbd_layout_t layout;
    atomic_int   state[KEY_CNT]; // each item is the last evdev key event value (0=UP, 1=DOWN, 2=HOLD) for each KEY_* and BTN_*
    void         (*redraw_cb)(void*);
    void         *redraw_cb_data;
};

kbd_t *kbd_new(kbd_layout_t layout, char **err) {
    #define kbd_new_assert(cond, format, ...) do {    \
        if (!(cond)) {                                \
            if (err)                                  \
                asprintf(err, format, ##__VA_ARGS__); \
            return NULL;                              \
        }                                             \
    } while (0)

    kbd_t *kbd = calloc(1, sizeof(kbd_t));
    kbd->layout = layout;

    kbd_new_assert(kbd->layout.units_per_row > 0, "units per row must be at least 1, is %d", kbd->layout.units_per_row);
    kbd_new_assert(kbd->layout.units_per_base > 0, "units per base must be at least 1, is %d", kbd->layout.units_per_base);
    kbd_new_assert(kbd->layout.px_per_base > 0, "pixels per base must be at least 1, is %d", kbd->layout.px_per_base);
    kbd_new_assert(kbd->layout.px_per_base%kbd->layout.units_per_base == 0, "pixels per base (%d) must divide into units per base (%d) without any remainder for layout to work correctly (to prevent rounding issues and blurriness in cell layout)", kbd->layout.px_per_base, kbd->layout.units_per_base);
    kbd_new_assert(kbd->layout.px_per_base%8 == 0, "pixels per base (%d) must be divisible by 8 for layout to work correctly (e.g. font size is /2, padding is /8)", kbd->layout.px_per_base);

    int n = 0;
    for (size_t i = 0; i < kbd->layout.n_keys; i++) {
        kbd_layout_key_t *key = &kbd->layout.keys[i];
        int dn = key->units;
        kbd_new_assert(dn > 0, "key %zu: must be 1 or more units wide, is %d", i, dn);
        kbd_new_assert(dn <= kbd->layout.units_per_row, "key %zu: must fit in %d units, is %d", i, kbd->layout.units_per_row, dn);
        kbd_new_assert(dn <= (kbd->layout.units_per_row-n), "key %zu: too large for remaining space in row, wanted %d units, %d used, %d available", i, dn, n, kbd->layout.units_per_row-n);
        n += dn;
        assert(n <= kbd->layout.units_per_row);
        if (n == kbd->layout.units_per_row)
            n = 0;
    }
    kbd_new_assert(n == 0, "expected more keys to fill row, got none, %d units missing", kbd->layout.units_per_row-n);

    if (err)
        *err = NULL;
    return kbd;

    #undef kbd_new_assert
}

void kbd_free(kbd_t *kbd) {
    free(kbd);
}

void kbd_set_redraw_cb(kbd_t *kbd, void (*fn)(void*), void* data) {
    kbd->redraw_cb = fn;
    kbd->redraw_cb_data = data;
}

static inline int kbd_get_state(kbd_t *kbd, int key) {
    assert(key > 0 && key <= KEY_MAX);
    return atomic_load(&kbd->state[key]);
}

void kbd_set_state(kbd_t *kbd, int key, int state) {
    int old = kbd_get_state(kbd, key);
    while (!atomic_compare_exchange_strong(&kbd->state[key], &old, state));
    if (old != state && kbd->redraw_cb)
        kbd->redraw_cb(kbd->redraw_cb_data);
}

static inline int kbd_get_px_per_unit(kbd_t *kbd) { return kbd->layout.px_per_base / kbd->layout.units_per_base; }
static inline int kbd_get_gap(kbd_t *kbd)         { return kbd_get_px_per_unit(kbd); }
static inline int kbd_get_padding(kbd_t *kbd)     { return kbd->layout.px_per_base/8; }
static inline int kbd_get_font_size(kbd_t *kbd)   { return kbd->layout.px_per_base/2; }
static inline int kbd_get_curve(kbd_t *kbd)       { return kbd->layout.px_per_base/2; }

int kbd_get_rows(kbd_t *kbd) {
    int n = 0;
    for (size_t i = 0; i < kbd->layout.n_keys; i++)
        n += kbd->layout.keys[i].units;
    return n / kbd->layout.units_per_row;
}

int kbd_get_width(kbd_t *kbd)  { return kbd_get_gap(kbd)*2 + kbd_get_px_per_unit(kbd)*kbd->layout.units_per_row; }
int kbd_get_height(kbd_t *kbd) { return kbd_get_gap(kbd) + kbd_get_rows(kbd)*(kbd->layout.px_per_base + kbd_get_gap(kbd)); }

static void cairoext_rectangle_curved(cairo_t *cr, double x, double y, double w, double h, double r);

void kbd_draw(kbd_t *kbd, cairo_t *cr) {
    #define RGB(r, g, b) (double)(r)/255.0l, (double)(g)/255.0l, (double)(b)/255.0l

    int tw, th;
    tw = kbd_get_width(kbd);
    th = kbd_get_height(kbd);

    cairo_set_line_width(cr, 1);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);

    cairo_font_extents_t ef;
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, kbd_get_font_size(kbd));
    cairo_font_extents(cr, &ef);

    cairo_rectangle(cr, 0, 0, tw, th);
    cairo_set_source_rgb(cr, RGB(244, 239, 239));
    cairo_fill(cr);

    int cx, cy, cn;
    cx = kbd_get_gap(kbd);
    cy = kbd_get_gap(kbd);
    cn = 0;

    for (size_t i = 0; i < kbd->layout.n_keys; i++) {
        kbd_layout_key_t *key = &kbd->layout.keys[i];

        int kn, kw, kh;
        kn = key->units;
        kw = kn * kbd_get_px_per_unit(kbd);
        kh = kbd->layout.px_per_base;

        if (key->label) {
            cairoext_rectangle_curved(cr, cx, cy, kw, kh, kbd_get_curve(kbd));

            cairo_set_source_rgb(cr, RGB(0, 0, 0));
            cairo_stroke_preserve(cr);

            switch (kbd_get_state(kbd, key->code)) {
            case 0: cairo_set_source_rgb(cr, RGB(255, 255, 255)); break; // UP
            case 1: cairo_set_source_rgb(cr, RGB(214, 194, 194)); break; // DOWN
            case 2: cairo_set_source_rgb(cr, RGB(194, 163, 163)); break; // HOLD
            default: assert(0);
            }
            cairo_fill(cr);

            cairo_text_extents_t et;
            cairo_text_extents(cr, key->label, &et);
            cairo_move_to(cr,
                cx + 0.5 + kw/2 - et.x_bearing - et.width/2,
                cy + 0.5 + kh/2 + kbd_get_padding(kbd) - ef.descent + et.height/2
            );
            cairo_set_source_rgb(cr, RGB(0, 0, 0));
            cairo_show_text(cr, key->label);
        }

        assert(cn+kn <= kbd->layout.units_per_row);
        if (cn+kn == kbd->layout.units_per_row) {
            cx = kbd_get_gap(kbd);
            cy += kbd->layout.px_per_base + kbd_get_gap(kbd);
            cn = 0;
        } else {
            cx += kw;
            cn += kn;
        }
        assert(cx <= tw);
        assert(cy <= th);
    }

    #undef RGB
}

void cairoext_rectangle_curved(cairo_t *cr, double x, double y, double w, double h, double r) {
    assert(w > 0);
    assert(h > 0);
    assert(r > 0);
    if (w/2 < r) {
        if (h/2 < r) {
            cairo_move_to(cr, x, (y + y+h)/2);
            cairo_curve_to(cr, x, y, x, y, (x + x+w)/2, y);
            cairo_curve_to(cr, x+w, y, x+w, y, x+w, (y + y+h)/2);
            cairo_curve_to(cr, x+w, y+h, x+w, y+h, (x+w + x)/2, y+h);
            cairo_curve_to(cr, x, y+h, x, y+h, x, (y + y+h)/2);
        } else {
            cairo_move_to(cr, x, y + r);
            cairo_curve_to(cr, x, y, x, y, (x + x+w)/2, y);
            cairo_curve_to(cr, x+w, y, x+w, y, x+w, y + r);
            cairo_line_to(cr, x+w, y+h - r);
            cairo_curve_to(cr, x+w, y+h, x+w, y+h, (x+w + x)/2, y+h);
            cairo_curve_to(cr, x, y+h, x, y+h, x, y+h - r);
        }
    } else {
        if (h/2 < r) {
            cairo_move_to(cr, x, (y + y+h)/2);
            cairo_curve_to(cr, x, y, x, y, x + r, y);
            cairo_line_to(cr, x+w - r, y);
            cairo_curve_to(cr, x+w, y, x+w, y, x+w, (y + y+h)/2);
            cairo_curve_to(cr, x+w, y+h, x+w, y+h, x+w - r, y+h);
            cairo_line_to(cr, x + r, y+h);
            cairo_curve_to(cr, x, y+h, x, y+h, x, (y + y+h)/2);
        } else {
            cairo_move_to(cr, x, y + r);
            cairo_curve_to(cr, x, y, x, y, x + r, y);
            cairo_line_to(cr, x+w - r, y);
            cairo_curve_to(cr, x+w, y, x+w, y, x+w, y + r);
            cairo_line_to(cr, x+w , y+h - r);
            cairo_curve_to(cr, x+w, y+h, x+w, y+h, x+w - r, y+h);
            cairo_line_to(cr, x + r, y+h);
            cairo_curve_to(cr, x, y+h, x, y+h, x, y+h- r);
        }
    }
    cairo_close_path(cr);
}
