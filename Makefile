SHELL         = /bin/bash -o pipefail

# configuration (if doing more than just cleaning or generating the gitignore)
# note: anything here can be safely overridden, but it must stay consistent
# between build and install# warning: you must use make clean when you change
# these values

# note: this should really have been done in a separate configure
# script, but it is simple enough to do it this way (and we don't really need to
# be portable, as kbdscr only supports Linux), and it's easy enough to move it
# out if needed (all configure stuff, and only that, is done in this section)

ifneq ($(if $(MAKECMDGOALS),$(if $(filter-out clean gitignore,$(MAKECMDGOALS)),YES,NO),YES),YES)
 $(info -- Skipping configuration)
else
CROSS_COMPILE =
CC            = $(CROSS_COMPILE)gcc
PKGCONFIG     = $(CROSS_COMPILE)pkg-config
GZIP          = gzip

ifneq ($(CROSS_COMPILE),)
 $(info -- Cross-compiling with toolchain prefix $(CROSS_COMPILE))
endif

DESTDIR =
PREFIX  = /usr/local

$(info -- Compiling for prefix $(PREFIX))
ifneq ($(filter install uninstall,$(MAKECMDGOALS)),)
 ifneq ($(DESTDIR),)
  $(info -- Installing into destdir $(DESTDIR))
 endif
endif

BINDIR              := $(PREFIX)/bin
DATAROOTDIR         := $(PREFIX)/share
DATADIR             := $(PREFIX)/share
MANDIR              := $(DATAROOTDIR)/man
MAN1DIR             := $(MANDIR)/man1
XDG_APPLICATIONSDIR := $(DATAROOTDIR)/applications

ifeq ($(origin POLKIT_ACTIONSDIR),undefined)
 POLKIT_ACTIONSDIR := /usr/share/polkit-1/actions
 $(info -- Using default polkit actions dir: $(POLKIT_ACTIONSDIR))
else
 ifneq ($(POLKIT_ACTIONSDIR),)
  $(info -- Using provided polkit actions dir: $(BASH_COMPLETIONSDIR))
 else
  $(info -- Skipping polkit actions)
 endif
endif

ifeq ($(origin BASH_COMPLETIONSDIR),undefined)
 ifneq ($(shell $(PKGCONFIG) --exists bash-completion >/dev/null 2>/dev/null && echo y),)
  BASH_COMPLETIONSDIR := $(shell $(PKGCONFIG) --silence-errors --variable=completionsdir bash-completion)
  $(info -- Found bash completions dir with pkg-config: $(BASH_COMPLETIONSDIR))
 else
  BASH_COMPLETIONSDIR := /etc/bash_completion.d
  $(info -- Using fallback bash completions dir: $(BASH_COMPLETIONSDIR))
 endif
else
 ifneq ($(BASH_COMPLETIONSDIR),)
  $(info -- Using provided bash completions dir: $(BASH_COMPLETIONSDIR))
 else
  $(info -- Skipping bash completions)
 endif
endif

CFLAGS  = -Wall -Wextra -Wno-missing-field-initializers -Wpointer-arith -Wshadow -Werror -std=gnu11
LDFLAGS =
$(info -- CFLAGS = $(CFLAGS))
$(info -- LDFLAGS = $(LDFLAGS))

PTHREAD_CFLAGS := -pthread
PTHREAD_LIBS   := -pthread

define pkgconf =
 $(if $(filter-out undefined,$(origin $(1)_CFLAGS) $(origin $(1)_LIBS))
 ,$(info -- Using provided CFLAGS and LIBS for $(2))
 ,$(if $(shell $(PKGCONFIG) --exists $(2) >/dev/null 2>/dev/null && echo y)
  ,$(info -- Found $(2) ($(shell $(PKGCONFIG) --modversion $(2))) with pkg-config)
   $(eval $(1)_CFLAGS := $(shell $(PKGCONFIG) --silence-errors --cflags $(2)))
   $(eval $(1)_LIBS   := $(shell $(PKGCONFIG) --silence-errors --libs $(2)))
   $(if $(3)
   ,$(if $(shell $(PKGCONFIG) $(3) $(2) >/dev/null 2>/dev/null && echo y)
	,$(info .. Satisfies constraint $(3))
    ,$(info .. Does not satisfy constraint $(3))
	 $(error Dependencies do not satisfy constraints))
   ,)
  ,$(info -- Could not automatically detect $(2) with pkg-config. Please specify $(1)_CFLAGS and/or $(1)_LIBS manually)
   $(error Missing dependencies)))
endef

$(call pkgconf,XCB,xcb)
$(call pkgconf,CAIRO,cairo,--atleast-version=1.6.4)
endif

# version info

# note: change this to a normal version for tagged commits and set the
# debian/changelog entry to stable, then go back to version-dev afterwards, and
# put another entry in debian/changelog with UNRELEASED as the dist

override VERSION := v0.0.0-dev
ifneq ($(wildcard .git/.),)
 override VERSION_GIT := $(shell git describe --tags --always)
 ifneq ($(VERSION_GIT),)
  override VERSION := $(VERSION_GIT)
 endif
endif
override CFLAGS += -DKBDSCR_VERSION='"$(VERSION)"'

# everything

all: src/kbdscr res/kbdscr

install: all
	install -Dm755 src/kbdscr $(DESTDIR)$(BINDIR)/kbdscr
	install -Dm644 res/kbdscr.1.gz $(DESTDIR)$(MAN1DIR)/kbdscr.1.gz
	install -Dm644 res/kbdscr.desktop $(DESTDIR)$(XDG_APPLICATIONSDIR)/net.pgaskin.kbdscr.desktop
ifneq ($(POLKIT_ACTIONSDIR),)
	install -Dm644 res/kbdscr.policy $(DESTDIR)$(POLKIT_ACTIONSDIR)/net.pgaskin.kbdscr.policy
endif
ifneq ($(BASH_COMPLETIONSDIR),)
	install -Dm644 res/kbdscr.bash_completion $(DESTDIR)$(BASH_COMPLETIONSDIR)/kbdscr
endif

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/kbdscr
	rm -f $(DESTDIR)$(MAN1DIR)/kbdscr.1.gz
	rm -f $(DESTDIR)$(XDG_APPLICATIONSDIR)/kbdscr.desktop
ifneq ($(POLKIT_ACTIONSDIR),)
	rm -f $(DESTDIR)$(POLKIT_ACTIONSDIR)/net.pgaskin.kbdscr.policy
endif
ifneq ($(BASH_COMPLETIONSDIR),)
	rm -f $(DESTDIR)$(BASH_COMPLETIONSDIR)/kbdscr
endif

clean:
	rm -f $(GENERATED)

gitignore:
	echo '# make gitignore' > .gitignore
	echo '$(GENERATED)' | \
		sed 's/ /\n/g' | \
		sed 's/^./\/&/' >> .gitignore

.PHONY: all install uninstall clean gitignore

# kbdscr

src/kbdscr: override CFLAGS  += $(PTHREAD_CFLAGS) $(XCB_CFLAGS) $(CAIRO_CFLAGS)
src/kbdscr: override LDFLAGS += $(PTHREAD_LIBS) $(XCB_LIBS) $(CAIRO_LIBS)

src/kbdscr: src/evdev.o src/kbd.o src/main.o src/win.o
res/kbdscr: res/kbdscr.1.gz res/kbdscr.bash_completion res/kbdscr.desktop res/kbdscr.policy

override EXECUTABLES += src/kbdscr
override GENERATED += src/kbdscr
.PHONY: res/kbdscr

# common

define patw =
 $(foreach dir,src res,$(dir)/*$(1))
endef

define rpatw =
 $(patsubst %$(1),%$(2),$(foreach w,$(call patw,$(1)),$(wildcard $(w))))
endef

%.gz: %
	$(GZIP) --force --keep $<

$(EXECUTABLES): %:
	$(CC) -o $@ $^ $(LDFLAGS)

$(call rpatw,.c,.o): %.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

$(call rpatw,.in,): %: %.in src/kbd_layout.h
	sed 's/@\([a-zA-Z_]*\)@/$$___\1/g' $< | \
	    ___layouts="$$(sed -n '/^#define KBD_LAYOUTS/,/^$$/{//b;p}' src/kbd_layout.h | cut -d '"' -f2,4 | sed 's/^\(.*\)"\(.*\)$$/.PP\n\\fB\1\\fR\n.RS 4\n\2\\\&.\n.RE/g' | sed 's/"/\\"/g')" \
	    ___bindir="$(BINDIR)" \
		envsubst '$$___layouts $$___bindir' > $@

override GENERATED += $(call patw,.gz) $(call rpatw,.c,.o) $(call rpatw,.in,)
