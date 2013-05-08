/**
 * Copyright (c) 2013 Nicolas Hillegeer <nicolas at hillegeer dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <X11/X.h>
#include <X11/Xlib.h>

#include <sys/select.h>
#include <sys/time.h>

#include <signal.h>
#include <time.h>
#include <getopt.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int gIdleTimeout = 1;
static int gVerbose     = 0;

static sig_atomic_t working;

static void signalHandler(int signo) {
    working = 0;
}

static int setupSignals() {
    struct sigaction act;

    memset(&act, 0, sizeof(struct sigaction));

    /* Use the sa_sigaction field because the handles has two additional parameters */
    act.sa_handler = signalHandler;
    act.sa_flags   = 0;
    sigemptyset(&act.sa_mask);

    if (sigaction(SIGTERM, &act, NULL) == -1) {
        perror("hhpc: could not register SIGTERM");
        return 0;
    }

    if (sigaction(SIGHUP, &act, NULL) == -1) {
        perror("hhpc: could not register SIGHUP");
        return 0;
    }

    if (sigaction(SIGINT, &act, NULL) == -1) {
        perror("hhpc: could not register SIGINT");
        return 0;
    }

    if (sigaction(SIGQUIT, &act, NULL) == -1) {
        perror("hhpc: could not register SIGQUIT");
        return 0;
    }

    return 1;
}

/**
 * milliseconds over 1000 will be ignored
 */
static void delay(time_t sec, long msec) {
    struct timespec sleep;

    sleep.tv_sec  = sec;
    sleep.tv_nsec = (msec % 1000) * 1000 * 1000;

    if (nanosleep(&sleep, NULL) == -1) {
        signalHandler(0);
    }
}

/**
 * generates an empty cursor,
 * don't forget to destroy the cursor with XFreeCursor
 *
 * do we need to use XAllocColor or will it always just work
 * as I've observed?
 */
static Cursor nullCursor(Display *dpy, Drawable dw) {
    XColor color  = { 0 };
    Pixmap pixmap = XCreatePixmap(dpy, dw, 1, 1, 1);
    Cursor cursor = XCreatePixmapCursor(dpy, pixmap, pixmap, &color, &color, 0, 0);

    XFreePixmap(dpy, pixmap);

    return cursor;
}

/**
 * returns 0 for failure, 1 for success
 */
static int grabPointer(Display *dpy, Window win, Cursor cursor, unsigned int mask) {
    int rc;

    /**
     * retry until we actually get the pointer (with a suitable delay)
     * or we get an error we can't recover from.
     */
    while (1) {
        rc = XGrabPointer(dpy, win, False, mask, GrabModeAsync, GrabModeAsync, win, cursor, CurrentTime);

        switch (rc) {
            case GrabSuccess:
                if (gVerbose) printf("hhpc: succesfully grabbed mouse pointer\n");
                return 1;

            case AlreadyGrabbed:
                if (gVerbose) fprintf(stderr, "hhpc: XGrabPointer: already grabbed mouse pointer, retrying with delay\n");

                delay(0, 500);

                break;

            case GrabFrozen:
                fprintf(stderr, "hhpc: XGrabPointer: grab was frozen, exiting\n");
                return 0;

            case GrabInvalidTime:
                fprintf(stderr, "hhpc: XGrabPointer: invalid time, exiting\n");
                return 0;

            default:
                fprintf(stderr, "hhpc: XGrabPointer: could not grab mouse pointer (%d), exiting\n", rc);
                return 0;
        }
    }
}

static void waitForMotion(Display *dpy, Window win, int timeout) {
    int ready = 0;
    int xfd   = ConnectionNumber(dpy);

    unsigned int mask = PointerMotionMask | ButtonPressMask; /* ButtonPressMask */

    fd_set fds;

    XEvent event;
    Cursor emptyCursor = nullCursor(dpy, win);

    working = 1;

    if (!setupSignals()) {
        fprintf(stderr, "hhpc: could not register signals, program will not exit cleanly\n");
    }

    while (working) {
        if (!grabPointer(dpy, win, emptyCursor, mask)) {
            return;
        }

        /* add the X11 fd to the fdset so we can poll/select on it */
        FD_ZERO(&fds);
        FD_SET(xfd, &fds);

        ready = select(xfd + 1, &fds, NULL, NULL, NULL);

        if (ready > 0) {
            if (gVerbose) printf("hhpc: event received\n");

            /* event received, release mouse, sleep, and try to grab again */
            XUngrabPointer(dpy, CurrentTime);

            /* drain events */
            while (XPending(dpy)) {
                /* XNextEvent(dpy, &event); */
                XMaskEvent(dpy, mask, &event);

                if (gVerbose) printf("hhpc: draining event\n");
            }

            if (gVerbose) printf("hhpc: ungrabbing and sleeping\n");

            delay(timeout, 0);
        }
        else if (ready == 0) {
            if (gVerbose) printf("hhpc: timeout\n");
        }
        else {
            perror("hhpc: error while select()'ing, retrying");
        }
    }

    XUngrabPointer(dpy, CurrentTime);
    XFreeCursor(dpy, emptyCursor);
}

static int parseOptions(int argc, char *argv[]) {
    int option = 0;

    while ((option = getopt(argc, argv, "i:v")) != -1) {
        switch (option) {
            case 'i': gIdleTimeout = atoi(optarg); break;
            case 'v': gVerbose = 1; break;
            default: return 0;
        }
    }

    return 1;
}

static void usage() {
    printf("hhpc [-i] seconds [-v]\n");
}

int main(int argc, char *argv[]) {
    if (!parseOptions(argc, argv)) {
        usage();

        return 1;
    }

    Display *dpy   = XOpenDisplay(NULL);
    int scr        = DefaultScreen(dpy);
    Window rootwin = RootWindow(dpy, scr);

    if (gVerbose) printf("hhpc: got root window, screen = %d, display = %p, rootwin = %d\n", scr, (void *) dpy, (int) rootwin);

    waitForMotion(dpy, rootwin, gIdleTimeout);

    XCloseDisplay(dpy);

    if (gVerbose) printf("hhpc: resources released, exiting...\n");
    return 0;
}
