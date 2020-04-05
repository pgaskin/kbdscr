#ifndef KBDSCR_WIN_H
#define KBDSCR_WIN_H
#include <cairo/cairo.h>

// x11win_t is a simple wrapper for using cairo with an XCB window.
typedef struct x11win_t x11win_t;

// x11win_new creates a new window with the specified title and a fixed size. If
// any errors ocurred, the return value will be NULL, and if err is not NULL,
// its target will be set to a string describing the error (which will need to
// be freed by the caller). Otherwise, the return value will be an allocated
// x11win_t.
x11win_t *x11win_new(const char* title, const char* class, int width, int height, char **err);

// x11win_main runs the main event loop for the window and returns when the
// WM_DELETE_WINDOW is sent. It also returns any error which occurs.
int x11win_main(x11win_t *x, void (*draw)(void *data, cairo_t *s), void *data, char **err);

// x11win_free destroys the window and any allocated resources.
void x11win_free(x11win_t *x);

// x11win_redraw forces the window to be redrawn. It can be safely called from
// another thread.
void x11win_redraw(x11win_t *x);
#endif