#ifndef KBDSCR_EVDEV_H
#define KBDSCR_EVDEV_H
#include <stddef.h>
#include "kbd.h"

typedef struct evdev_watch_key_t evdev_watch_key_t;

// evdev_watch_key starts a new thread which watches the provided evdev devices
// and calls keystate_cb for each key event.
evdev_watch_key_t *evdev_watch_key_start(
    void (*keystate_cb)(void* data, int code, int state),
    void (*error_cb)(void* data, const char* err),
    void *data,
    const char **devs, size_t n_dev,
    char **err
);

// evdev_watch_key_stop stops the watcher and closes the FDs.
void evdev_watch_key_stop(evdev_watch_key_t *w);

#endif