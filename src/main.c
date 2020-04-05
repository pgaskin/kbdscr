#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "evdev.h"
#include "kbd.h"
#include "kbd_layout.h"
#include "win.h"

void handle_error(void* data __attribute__((unused)), const char *msg) {
    printf("Warning: %s\n", msg);
}

int main(int argc, char **argv) {
    if (argc < 3 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
        fprintf(stderr, "Usage: %s layout input_event_evdev_path...\n", argv[0]);
        fprintf(stderr, "Layouts:\n");
        #define X(_, id, desc) \
            fprintf(stderr, "    %-16s %s\n", id, desc);
            KBD_LAYOUTS
        #undef X
        fprintf(stderr, "Example: sudo %s km-us-en /dev/input/event*\n", argv[0]);
        return EXIT_SUCCESS;
    }

    bool found = false;
    kbd_layout_t layout;
    #define X(layout_, id, desc) \
        if (!strcmp(argv[1], id)) { \
            layout = layout_; \
            found = true; \
        }
        KBD_LAYOUTS
    #undef X
    if (!found) {
        printf("Error: initialize keyboard layout: could not find layout %s.\n", argv[1]);
        return EXIT_FAILURE;
    }

    char *err;

    kbd_t *kbd = kbd_new(layout, &err);
    if (err) {
        printf("Error: initialize keyboard layout: %s.\n", err);
        free(err);
        return EXIT_FAILURE;
    }

    x11win_t *x = x11win_new("kbdscr", "net.pgaskin.kbdscr", kbd_get_width(kbd), kbd_get_height(kbd), &err);
    if (err) {
        printf("Error: create window: %s.\n", err);
        free(err);
        kbd_free(kbd);
        return EXIT_FAILURE;
    }
    kbd_set_redraw_cb(kbd, (void(*)(void*))(x11win_redraw), x);

    evdev_watch_key_t *w = evdev_watch_key_start((void(*)(void*, int, int))(kbd_set_state), handle_error, kbd, (const char**)(&argv[2]), argc-2, &err);
    if (err) {
        printf("Error: start evdev watcher: %s.\n", err);
        free(err);
        x11win_free(x);
        kbd_free(kbd);
        return EXIT_FAILURE;
    }

    x11win_main(x, (void(*)(void*, cairo_t*))(kbd_draw), kbd, &err);
    if (err) {
        printf("Error: run window main loop: %s.\n", err);
        free(err);
        evdev_watch_key_stop(w);
        x11win_free(x);
        kbd_free(kbd);
        return EXIT_FAILURE;
    }

    printf("Cleaning up.\n");
    evdev_watch_key_stop(w);
    x11win_free(x);
    kbd_free(kbd);
    return EXIT_SUCCESS;
}
