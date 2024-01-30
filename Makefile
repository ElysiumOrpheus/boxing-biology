PLATFORM?=linux64-debug

.PHONY: all_stage

RAYLIB_NAME = raylib5-$(PLATFORM)

ifeq ($(PLATFORM), linux64)
EXEC_EXTENSION=
CC=gcc
LDFLAGS+=-lraylib
endif

ifeq ($(PLATFORM), win64)
EXEC_EXTENSION=.exe
CC=x86_64-w64-mingw32-gcc
CFLAGS+=-fPIC
LDFLAGS+=-lraylibdll
endif

ifeq ($(PLATFORM), linux64-debug)
EXEC_EXTENSION=-debug
CC=gcc
LDFLAGS+=-lraylib
CFLAGS+=-g
endif

PROGRAMS=boxing_science

all_stage:
	$(foreach prog, $(PROGRAMS), make $(prog)$(EXEC_EXTENSION);)

CFLAGS+=-Isrc
CFLAGS+=-Ilib/raygui/src
CFLAGS+=-O2
CFLAGS+=-Ilib/boxito_engine/src

CFLAGS+=-Ilib/$(RAYLIB_NAME)/include

LDFLAGS+=-lm
LDFLAGS+=-Llib/$(RAYLIB_NAME)/lib
LDFLAGS+=-Wl,-rpath,lib/$(RAYLIB_NAME)/lib

BOXING_SCIENCE_SOURCES=src/main.c

boxing_science$(EXEC_EXTENSION): $(BOXING_SCIENCE_SOURCES)
# Copy the raylib dll to the root directory because for some reason -Wl,-rpath doesn't work with mingw
	echo "if [[ "${EXEC_EXTENSION}" == ".exe" ]] ;then cp lib/$(RAYLIB_NAME)/lib/raylib.dll .; fi" | bash
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)
