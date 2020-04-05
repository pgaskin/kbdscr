#define _GNU_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>
#include <xcb/xcb.h>

#include "win.h"

struct x11win_t {
    xcb_connection_t *conn;
    xcb_screen_t *scr;
    xcb_window_t win;
    xcb_atom_t wmdel;
    cairo_surface_t *s;
    cairo_t *cr;
    int width, height; // read-only, not updated
};

static xcb_void_cookie_t xcbext_set_win_fixed_size_checked(xcb_connection_t *c, xcb_window_t window, uint32_t width, uint32_t height);
static xcb_atom_t xcbext_get_intern_atom(xcb_connection_t *c, const char* name);
static xcb_visualtype_t *xcbext_get_visualtype(xcb_connection_t *c, xcb_visualid_t visualid);

x11win_t *x11win_new(const char* title, const char* class, int width, int height, char **err) {
    #define x11win_init_err(format, ...) do {         \
        if (format) {                                 \
            if (err)                                  \
                asprintf(err, format, ##__VA_ARGS__); \
            xcb_disconnect(x->conn);                  \
            return NULL;                              \
        } else {                                      \
            if (err)                                  \
                *err = NULL;                          \
            return x;                                 \
        }                                             \
    } while (0)

    x11win_t *x = calloc(1, sizeof(x11win_t));

    int errc;
    xcb_generic_error_t *errx;
    xcb_void_cookie_t ck;

    x->width = width;
    x->height = height;

    x->conn = xcb_connect(NULL, NULL);
    if ((errc = xcb_connection_has_error(x->conn)))
        x11win_init_err("could not open display: %d", errc);

    x->scr = xcb_setup_roots_iterator(xcb_get_setup(x->conn)).data;
    if (!x->scr)
        x11win_init_err("could not open screen");

    x->win = xcb_generate_id(x->conn);

    ck = xcb_create_window_checked(
        x->conn, XCB_COPY_FROM_PARENT, x->win, x->scr->root,
        100, 100, x->width, x->height, 0,
        XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT,
        XCB_CW_BACK_PIXEL | XCB_CW_BACKING_STORE | XCB_CW_EVENT_MASK,
        (uint32_t[]){x->scr->black_pixel, XCB_BACKING_STORE_WHEN_MAPPED, XCB_EVENT_MASK_EXPOSURE}
    );
    if ((errx = xcb_request_check(x->conn, ck)))
        x11win_init_err("could not create window: %d", errx->error_code);
    
    ck = xcb_change_property_checked(x->conn, XCB_PROP_MODE_REPLACE, x->win, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(title), title);
    if ((errx = xcb_request_check(x->conn, ck)))
        x11win_init_err("could not set window name: %d", errx->error_code);
    
    ck = xcb_change_property_checked(x->conn, XCB_PROP_MODE_REPLACE, x->win, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, strlen(class), class);
    if ((errx = xcb_request_check(x->conn, ck)))
        x11win_init_err("could not set window class: %d", errx->error_code);
    
    ck = xcbext_set_win_fixed_size_checked(x->conn, x->win, x->width, x->height);
    if ((errx = xcb_request_check(x->conn, ck)))
        x11win_init_err("could not set window size hints: %d", errx->error_code);

    x->wmdel = xcbext_get_intern_atom(x->conn, "WM_DELETE_WINDOW");
    ck = xcb_change_property_checked(x->conn, XCB_PROP_MODE_REPLACE, x->win, xcbext_get_intern_atom(x->conn, "WM_PROTOCOLS"), XCB_ATOM_ATOM, 32, 1, &x->wmdel);
    if ((errx = xcb_request_check(x->conn, ck)))
        x11win_init_err("could not set WM_PROTOCOLS: %d", errx->error_code);

    ck = xcb_map_window_checked(x->conn, x->win);
    if ((errx = xcb_request_check(x->conn, ck)))
        x11win_init_err("could not create window: %d", errx->error_code);
    
    if (xcb_flush(x->conn) <= 0)
        x11win_init_err("could not flush conn: %d", errx->error_code);

    xcb_visualtype_t *vt = xcbext_get_visualtype(x->conn, x->scr->root_visual);
    if (!vt)
        x11win_init_err("could not get root screen visualtype");
    x->s = cairo_xcb_surface_create(x->conn, x->win, vt, x->width, x->height);
    x->cr = cairo_create(x->s);

    x11win_init_err(NULL);
    #undef x11win_init_err
}

int x11win_main(x11win_t *x, void (*draw)(void *data, cairo_t *s), void *data, char **err) {
    #define x11win_main_err(format, ...) do {         \
        if (format) {                                 \
            if (err)                                  \
                asprintf(err, format, ##__VA_ARGS__); \
            xcb_disconnect(x->conn);                  \
            return 1;                                 \
        } else {                                      \
            if (err)                                  \
                *err = NULL;                          \
            return 0;                                 \
        }                                             \
    } while (0)

    cairo_surface_t *bufs = cairo_surface_create_similar(x->s, CAIRO_CONTENT_COLOR, x->width, x->height);
    cairo_t *bufcr;

    xcb_generic_event_t *evt;
    xcb_expose_event_t *evt_expose;
    xcb_client_message_event_t *evt_client_message;

    while ((evt = xcb_wait_for_event(x->conn))) {
        switch (evt->response_type & ~0x80) {
        case XCB_EXPOSE:
            evt_expose = (xcb_expose_event_t*)(evt);
            if (evt_expose->count != 0)
                break;
            bufcr = cairo_create(bufs);
            draw(data, bufcr);
            cairo_set_source_surface(x->cr, bufs, 0, 0);
            cairo_paint(x->cr);
            cairo_destroy(bufcr);
            xcb_flush(x->conn);
            break;
        case XCB_CLIENT_MESSAGE:
            evt_client_message = (xcb_client_message_event_t*)(evt);
            if (evt_client_message->data.data32[0] == x->wmdel)
                x11win_main_err(NULL);
            break;
        }
    }
    x11win_main_err(evt ? NULL : "io error waiting for event");

    #undef x11win_main_err
}

void x11win_redraw(x11win_t *x) {
    // different events are different sizes, but the full size needs to be provided
    xcb_expose_event_t *evt = (xcb_expose_event_t*)(&(xcb_raw_generic_event_t){});
    evt->response_type = XCB_EXPOSE;
    evt->window = x->win;
    evt->width = x->width;
    evt->height = x->height;
    xcb_send_event(x->conn, false, x->win, XCB_EVENT_MASK_EXPOSURE, (char*)(evt));
    xcb_flush(x->conn);
}

void x11win_free(x11win_t *x) {
    cairo_surface_destroy(x->s);
    xcb_disconnect(x->conn);
    free(x);
}

static xcb_void_cookie_t xcbext_set_win_fixed_size_checked(xcb_connection_t *c, xcb_window_t window, uint32_t width, uint32_t height) {
    // https://cgit.freedesktop.org/xcb/util-wm/tree/icccm/xcb_icccm.h?id=177d933f04d822deb7ec0a7bb13148701eec3e55#n527
    struct {
        uint32_t flags;
        int32_t  obsolete[4];
        int32_t  min_width, min_height;
        int32_t  max_width, max_height;
        int32_t  width_inc, height_inc;
        int32_t  min_aspect_num, min_aspect_den;
        int32_t  max_aspect_num, max_aspect_den;
        int32_t  base_width, base_height;
        uint32_t win_gravity;
    } hints = {0};

    hints.flags = (1<<4) + (1<<5); // P_MIN_SIZE | P_MAX_SIZE
    hints.min_width = width;
    hints.min_height = height;
    hints.max_width = width;
    hints.max_height = height;

    return xcb_change_property_checked(c, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NORMAL_HINTS, XCB_ATOM_WM_SIZE_HINTS, 32, sizeof(hints)>>2, &hints);
}

static xcb_atom_t xcbext_get_intern_atom(xcb_connection_t *c, const char* name) {
    xcb_intern_atom_reply_t *r = xcb_intern_atom_reply(c, xcb_intern_atom_unchecked(c, 1, strlen(name), name), NULL);
    xcb_atom_t a = r->atom;
    free(r);
    return a;
}

static xcb_visualtype_t *xcbext_get_visualtype(xcb_connection_t *c, xcb_visualid_t visualid) {
    for (xcb_screen_iterator_t screen = xcb_setup_roots_iterator(xcb_get_setup(c)); screen.rem; xcb_screen_next(&screen))
        for (xcb_depth_iterator_t depth = xcb_screen_allowed_depths_iterator(screen.data); depth.rem; xcb_depth_next(&depth))
            for (xcb_visualtype_iterator_t visualtype = xcb_depth_visuals_iterator(depth.data); visualtype.rem; xcb_visualtype_next(&visualtype))
                if (visualtype.data->visual_id == visualid)
                    return visualtype.data;
    return NULL;
}
