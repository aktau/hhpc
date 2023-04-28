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
#include <errno.h>

static int gIdleTimeout = 1;
static int gDisplayHelp = 0;
static int gVerbose     = 0;

static volatile sig_atomic_t working;

static void signalHandler(int signo) {
    working = 0;
}

static int setupSignals() {
    struct sigaction act;

    memset(&act, 0, sizeof(act));

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

    while (nanosleep(&sleep, &sleep) != 0) {
        if (errno == EINTR)
            continue;
        else
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
    const char data[] = { 0 };

    Pixmap pixmap = XCreateBitmapFromData(dpy, dw, data, 1, 1);
    Cursor cursor = XCreatePixmapCursor(dpy, pixmap, pixmap, &color, &color, 0, 0);

    XFreePixmap(dpy, pixmap);

    return cursor;
}

/**
 * returns 0 for failure, 1 for success
 */
static int grabPointer(Display *dpy, Window win, Cursor cursor, unsigned int mask) {
    int rc;

    /* retry until we actually get the pointer (with a suitable delay)
     * or we get an error we can't recover from. */
    while (working) {
        rc = XGrabPointer(dpy, win, True, mask, GrabModeSync, GrabModeAsync, None, cursor, CurrentTime);

        switch (rc) {
            case GrabSuccess:
                if (gVerbose) fprintf(stderr, "hhpc: succesfully grabbed mouse pointer\n");
                return 1;

            case AlreadyGrabbed:
                if (gVerbose) fprintf(stderr, "hhpc: XGrabPointer: already grabbed mouse pointer, retrying with delay\n");
                delay(0, 500);
                break;

            case GrabFrozen:
                if (gVerbose) fprintf(stderr, "hhpc: XGrabPointer: grab was frozen, retrying after delay\n");
                delay(0, 500);
                break;

            case GrabNotViewable:
                fprintf(stderr, "hhpc: XGrabPointer: grab was not viewable, exiting\n");
                return 0;

            case GrabInvalidTime:
                fprintf(stderr, "hhpc: XGrabPointer: invalid time, exiting\n");
                return 0;

            default:
                fprintf(stderr, "hhpc: XGrabPointer: could not grab mouse pointer (%d), exiting\n", rc);
                return 0;
        }
    }

    return 0;
}

static void waitForMotion(Display *dpy, Window win, int timeout) {
    struct timeval sleep;
    sleep.tv_sec = (timeout <= 1) ? 3 : timeout;

    int ready = 0;
    int xfd   = ConnectionNumber(dpy);

    const unsigned int mask = PointerMotionMask | ButtonPressMask;

    fd_set fds;

    XEvent event;
    Cursor emptyCursor = nullCursor(dpy, win);

    working = 1;

    if (!setupSignals()) {
        fprintf(stderr, "hhpc: could not register signals, program will not exit cleanly\n");
    }

    while (working && grabPointer(dpy, win, emptyCursor, mask)) {
        /* we grab in sync mode, which stops pointer events from processing,
         * so we explicitly have to re-allow it with XAllowEvents. The old
         * method was to just grab in async mode so we wouldn't need this,
         * but that disables replaying the pointer events */
        XAllowEvents(dpy, SyncPointer, CurrentTime);

        /* syncing is necessary, otherwise the X11 FD will never receive an
         * event (and thus will never be ready, strangely enough) */
        XSync(dpy, False);

        /* add the X11 fd to the fdset so we can poll/select on it */
        FD_ZERO(&fds);
        FD_SET(xfd, &fds);

        /* we poll on the X11 fd to see if an event has come in, select()
         * is interruptible by signals, which allows ctrl+c to work. If we
         * were to just use XNextEvent() (which blocks), ctrl+c would not
         * work. */
        if ((ready = select(xfd + 1, &fds, NULL, NULL, &sleep)) == -1) {
            if (working) perror("hhpc: error while select()'ing");
        }

        if (ready == 0) {
            if (gVerbose) fprintf(stderr, "hhpc: timeout\n");
        }

        if (ready > 0) {
            if (gVerbose) fprintf(stderr, "hhpc: event received, ungrabbing and sleeping\n");

            /* event received, replay event, release mouse, drain, sleep, regrab */
            XAllowEvents(dpy, ReplayPointer, CurrentTime);
            XUngrabPointer(dpy, CurrentTime);

            /* drain events */
            while (XPending(dpy)) {
                XMaskEvent(dpy, mask, &event);

                if (gVerbose) fprintf(stderr, "hhpc: draining event\n");
            }

            delay(timeout, 0);
        }
    }

    XUngrabPointer(dpy, CurrentTime);
    XFreeCursor(dpy, emptyCursor);
}

// Returns 0 on failure, 1 on success
static int parseOptions(int argc, char *argv[]) {
    int option = 0;

    while ((option = getopt(argc, argv, "i:hv")) != -1) {
        switch (option) {
            case 'i': gIdleTimeout = atoi(optarg); break;
            case 'h': gDisplayHelp = 1; break;
            case 'v': gVerbose = 1; break;
            default: return 0;
        }
    }

    if (optind == argc) {
        return 1;
    }
    else {
        fprintf(stderr, "%s: wrong number of arguments -- expected none, found:", argv[0]);
        for (size_t i = optind; i < argc; ++i) {
            fprintf(stderr, " '%s'", argv[i]);
        }
        fprintf(stderr, "\n");

        return 0;
    }
}

// Return the length of the longest string in optionDescs.
// Assumes optionDescs is terminated with a null string "".
static size_t maxStrlen(char *optionDescs[]) {
    size_t maxLen = 0;
    for (size_t i = 0; *optionDescs[i] != '\0'; i += 2) {
        size_t nextLen = strlen(optionDescs[i]);
        maxLen = maxLen >= nextLen ? maxLen : nextLen;
    }

    return maxLen;
}

static void usage(char *progName) {
    // Pairs of option forms and their descriptions.
    // Terminated by a null string "".
    static char *optionDescs[] = {
        "-i seconds", "amount of time to wait before hiding the cursor",
        "-h",         "display this help message",
        "-v",         "be verbose",
        ""
    };

    // This could be made static as well, but since usage() is only ever called
    // once that's more trouble than it's worth.
    size_t maxLen = maxStrlen(optionDescs);

    printf("usage: %s [-hv] [-i seconds]\n", progName);
    for (size_t i = 0; *optionDescs[i] != '\0'; i += 2) {
        printf("    %-*s    %s\n", maxLen, optionDescs[i], optionDescs[i+1]);
    }
}

int main(int argc, char *argv[]) {
    if (!parseOptions(argc, argv)) {
        usage(argv[0]);

        return 1;
    }
    if (gDisplayHelp) {
        usage(argv[0]);

        return 0;
    }

    char *displayName = getenv("DISPLAY");

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        if (!displayName || strlen(displayName) == 0) {
            fprintf(stderr, "hhpc: could not open display, DISPLAY environment variable not set, are you sure the X server is started?\n");
            return 2;
        }
        else {
            fprintf(stderr, "hhpc: could not open display %s, check if your X server is running and/or the DISPLAY environment value is correct\n", displayName);
            return 1;
        }
    }

    int scr        = DefaultScreen(dpy);
    Window rootwin = RootWindow(dpy, scr);

    if (gVerbose) fprintf(stderr, "hhpc: got root window, screen = %d, display = %p, rootwin = %d\n", scr, (void *) dpy, (int) rootwin);

    waitForMotion(dpy, rootwin, gIdleTimeout);

    XCloseDisplay(dpy);

    return 0;
}
