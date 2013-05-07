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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void waitForMotion(Display *dpy, Window win, int timeout) {
	int ready = 0;
	int xfd   = ConnectionNumber(dpy);
	long mask = PointerMotionMask;

	fd_set fds;
	struct timeval tv;

	XEvent event;

	XSelectInput(dpy, win, mask);

	/**
	 * flush the commands we just sent because we're not going to use a
	 * conventional XNextEvent-based loop (which would flush automatically)
	 */
	XFlush(dpy);

	printf("flushed!\n");

	while (1) {
		/* add the X11 fd to the fdset so we can poll/select on it */
		FD_ZERO(&fds);
		FD_SET(xfd, &fds);

		/* poll with a timeout */
		tv.tv_usec = 0;
		tv.tv_sec  = timeout;

		ready = select(xfd + 1, &fds, NULL, NULL, &tv);

		if (ready > 0) {
			printf("event received\n");
		}
		else if (ready == 0) {
			printf("timeout\n");
		}
		else {
			perror("error while select()'ing");
		}

		/* drain events */
		while (XPending(dpy)) {
			XNextEvent(dpy, &event);

			printf("-> could start processing event...\n");
		}
	}
}

int main(void) {
  Display *dpy = XOpenDisplay(NULL);
  int scr = DefaultScreen(dpy);

  Window rootwin = RootWindow(dpy, scr);

  printf("Got root window, screen = %d, display = %p, rootwin = %d\n", scr, (void *) dpy, (int) rootwin);

  waitForMotion(dpy, rootwin, 1);

  XCloseDisplay(dpy);

  return 0;
}
