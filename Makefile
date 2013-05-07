# Copyright (c) 2013 Nicolas Hillegeer <nicolas at hillegeer dot com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# makefile adjusted as per: http://stackoverflow.com/questions/1079832/how-can-i-configure-my-makefile-for-debug-and-release-builds
# $@ The name of the target file (the one before the colon)
# $< The name of the first (or only) prerequisite file (the first one after the colon)
# $^ The names of all the prerequisite files (space separated)
# $* The stem (the bit which matches the % wildcard in the rule definition.

CFLAGS_COMMON = \
	-D_XOPEN_SOURCE \
	-Wall \
	-Wextra \
	-Wcast-align \
	-Wcast-qual \
	-Wfloat-equal \
	-Wredundant-decls \
	-Wundef \
	-Wdisabled-optimization \
	-Wshadow \
	-Wmissing-braces \
	-Wstrict-aliasing=2 \
	-Wstrict-overflow=5 \
	-Wconversion \
	-Wno-unused-parameter \
	-pedantic \
	-std=c11

CC = cc $(CFLAGS)

EXECUTABLE = hhpc
OBJECTS    = hhpc.o

all: debug

debug: CFLAGS = $(CFLAGS_COMMON) \
	-g \
	-O \
	-DDEBUG
debug: $(EXECUTABLE)

release: CFLAGS = $(CFLAGS_COMMON) \
	-g \
	-O2 \
	-march=core2 \
	-mtune=native \
	-ftree-vectorize \
	-flto \
	-DDEBUG
release: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(shell pkg-config --libs x11) -o $@ $^

%.o: %.c
	$(CC) $(shell pkg-config --cflags x11) -c $<

clean:
	rm -f $(EXECUTABLE) $(OBJECTS)
