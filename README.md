hhpc
====

hhpc(1) is a utility that hides the mouse pointer in X11. The method it
uses to hide the cursor was taken from the hhp program found in
xmonad-utils. I found that after a while for some reason the original
hhp stopped hiding my mouse pointer, so I recreated it in C to make it
easier to debug. Should `hhpc` not work out for you, `unclutter` is a
good alternative. I created `hhpc` because `unclutter` doesn't seem to
interoperate properly with hardware accelerated surfaces like those of
video players using VAAPI.

It functions by grabbing your mouse pointer at startup and replacing its
bitmap with an empty one (thus hiding it). It uses the `XGrabPointer`
X11 function. Then, it waits for the X server to notify it when a user
tries to move or click the pointer. At this point, hhpc relinquishes
control with `XUngrabPointer`, replays the action the user just tried to
execute and waits for the specified number of seconds (flag `-i`) before
trying to grab the pointer and hide it again.

Running
-------

```sh
./hhpc [options]

  -i seconds: amount of time to wait before hiding the cursor
  -h: display a help message
  -v: be verbose
```

Runtime Dependencies
--------------------

A running X server.

Build dependencies
------------------

- X.org development files
- A gcc-compatible compiler (haven't tested if the flags works with
  other compilers)
- pkg-config

NOTE: this code compiles by default with the `-std=c99` switch, for
standards-compliance. This standard was implemented completely as of gcc
4.5, but has been quite functional for a long time before that. So
chances are it will compile just fine. However, since hhpc doesn't use
any advanced C99 features, it will probably compile with much older
versions of gcc as well, though I haven't tried. So if you have an
ancient compiler and the build fails, just edit the Makefile and try
leaving out the `-std=c99` flag.

Installing
---------

hhpc(1) is available in the AUR:
[hhpc-git](https://aur.archlinux.org/packages/hhpc-git/).

Building
--------

To get the dependencies on Debian and derivatives.

```sh
$ sudo apt-get install make pkg-config gcc libc6-dev libx11-dev
```

Then clone and build it.

```sh
$ git clone ...
$ cd hhpc
$ make
```

If everything goes well, there should appear a hhpc executable in the
current directory.

Todo
----

- Use XCB instead of Xlib.

Changelog
---------

### v0.3

- Use sync grabbing and make the X11 server replay the drained pointer
  events. This allows clicking while the pointer is hidden, which will
  henceforth register the click to the underlying window. **NOTE**: if
  this is undesired behaviour for some, please log an issue and I will
  reinstate the old behaviour with a commandline switch.
- Minor code, comments and output cleanup

License (3-clause BSD)
----------------------

Copyright (c) 2013 Nicolas Hillegeer <nicolas at hillegeer dot com>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
