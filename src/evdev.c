#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>

#include <linux/input.h>
#include <linux/input-event-codes.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>

#include "evdev.h"

struct evdev_watch_key_t {
    int        cancel_fd;
    thrd_t     thread;
    void       (*keystate_cb)(void* data, int code, int state);
    void       (*error_cb)(void* data, const char* err);
    void       *data;
    size_t     n_dev;
    const char **devs;
};

static int evdev_watch_key_thread(evdev_watch_key_t *opts);

evdev_watch_key_t *evdev_watch_key_start(void (*keystate_cb)(void* data, int code, int state), void (*error_cb)(void* data, const char* err), void *data, const char **devs, size_t n_dev, char **err) {
    evdev_watch_key_t *w = calloc(1, sizeof(evdev_watch_key_t));
    w->keystate_cb = keystate_cb;
    w->error_cb    = error_cb;
    w->data        = data;
    w->n_dev       = n_dev;
    w->devs        = devs;

    if ((w->cancel_fd = eventfd(0, 0)) == -1) {
        if (err)
            asprintf(err, "could not create cancellation eventfd: %s", strerror(errno));
        return NULL;
    }

    if (thrd_create(&w->thread, (int(*)(void*))(evdev_watch_key_thread), w) != thrd_success) {
        if (err)
            asprintf(err, "could not start thread");
        return NULL;
    }

    if (err)
        *err = NULL;
    return w;
}

void evdev_watch_key_stop(evdev_watch_key_t *w) {
    uint64_t i = 1;
    assert(write(w->cancel_fd, &i, sizeof(i)) == sizeof(i));
    thrd_join(w->thread, NULL); // wait for the FDs to be closed
    close(w->cancel_fd);
    free(w);
}

static int evdev_watch_key_thread(evdev_watch_key_t *w) {
    #define evdev_watch_key_err(format, ...) do {  \
        if (w->error_cb) {                         \
            char *msg;                             \
            asprintf(&msg, format, ##__VA_ARGS__); \
            w->error_cb(w->data, msg);             \
            free(msg);                             \
        }                                          \
    } while (0)

    int efd;
    if ((efd = epoll_create1(0)) == -1) {
        evdev_watch_key_err("create epoll fd: %s", strerror(errno));
        return 1;
    }

    if (epoll_ctl(efd, EPOLL_CTL_ADD, w->cancel_fd, &(struct epoll_event){
        .data   = { .u32 = 0xFFFFFFFF },
        .events = EPOLLIN,
    })) {
        evdev_watch_key_err("add cancellation eventfd to epoll: %s", strerror(errno));
        return 1;
    }

    int fds[w->n_dev];
    for (size_t i = 0; i < w->n_dev; i++) {
        if ((fds[i] = open(w->devs[i], O_RDONLY)) == -1) {
            evdev_watch_key_err("open device '%s': %s", w->devs[i], strerror(errno));
            continue;
        }
        if (epoll_ctl(efd, EPOLL_CTL_ADD, fds[i], &(struct epoll_event){
            .data = { .u32 = i },
            .events = EPOLLIN, // note: EPOLLERR and EPOLLHUP are implied
        })) {
            evdev_watch_key_err("open device '%s': add fd %d to epoll: %s", w->devs[i], fds[i], strerror(errno));
            continue;
        }
    }

    int n;
    struct epoll_event events[10]; // an arbitrary limit
    for (;;) {
        if ((n = epoll_wait(efd, events, sizeof(events), -1)) == -1) {
            evdev_watch_key_err("wait for epoll event: %s", strerror(errno));
            continue;
        }

        for (int i = 0; i < n; i++)
            if (events[i].data.u32 == 0xFFFFFFFF)
                goto cancel;

        for (int i = 0; i < n; i++) {
            int fd = fds[events[i].data.u32];
            const char* dev = w->devs[i];

            if (events[i].events & EPOLLHUP) {
                evdev_watch_key_err("handle epoll event (fd: %d, dev: %s): EPOLLHUP, removing fd from epoll", fd, dev);
                if (epoll_ctl(efd, EPOLL_CTL_DEL, fd, NULL))
                    evdev_watch_key_err("remove fd %d (%s) from epoll: %s", fd, dev, strerror(errno));
                continue;
            }
            if (events[i].events & EPOLLERR) {
                evdev_watch_key_err("handle epoll event (fd: %d, dev: %s): EPOLLERR", fd, dev);
                continue;
            }
            if (!(events[i].events & EPOLLIN)) {
                evdev_watch_key_err("handle epoll event (fd: %d, dev: %s): unknown event", fd, dev);
                continue;
            }

            int m;
            struct input_event ev;
            if ((m = read(fd, &ev, sizeof(ev))) == -1) {
                evdev_watch_key_err("handle epoll event (fd: %d, dev: %s): read evdev event struct: %s", fd, dev, strerror(errno));
                continue;
            } else if (m != sizeof(ev)) {
                evdev_watch_key_err("handle epoll event (fd: %d, dev: %s): read evdev event struct: wrong size: wanted %zu, got %d", fd, dev, sizeof(ev), m);
                continue;
            }

            if (ev.type == EV_KEY && w->keystate_cb)
                w->keystate_cb(w->data, ev.code, ev.value);
        }
    }

cancel:
    for (size_t i = 0; i < w->n_dev; i++)
        if (fds[i] != -1)
            close(fds[i]);
    close(efd);
    return 0;

    #undef evdev_watch_key_err
}