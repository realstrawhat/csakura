CC      ?= cc
PREFIX  ?= /usr/local
BINDIR   = $(DESTDIR)$(PREFIX)/bin
UNAME_S := $(shell uname -s 2>/dev/null || echo unknown)

CFLAGS  += -std=c99 -O2 -Wall -Wextra -pedantic
LDLIBS  += -lm

# Prefer pkg-config (Homebrew ncurses, Linux ncursesw, …)
NCURSES_CFLAGS := $(shell pkg-config --cflags ncursesw 2>/dev/null || pkg-config --cflags ncurses 2>/dev/null)
NCURSES_LIBS   := $(shell pkg-config --libs ncursesw 2>/dev/null || pkg-config --libs ncurses 2>/dev/null)

ifneq ($(NCURSES_LIBS),)
  CFLAGS += $(NCURSES_CFLAGS)
  LDLIBS += $(NCURSES_LIBS)
else ifeq ($(UNAME_S),Darwin)
  # stock macOS / Xcode ncurses (wide enough for UTF-8 glyphs)
  LDLIBS += -lncurses
else
  LDLIBS += -lncursesw
endif

all: csakura

csakura: src/csakura.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

install: csakura
	mkdir -p "$(BINDIR)"
	install -m 755 csakura "$(BINDIR)/csakura"

uninstall:
	rm -f "$(BINDIR)/csakura"

clean:
	rm -f csakura

.PHONY: all install uninstall clean
