NAME=pls
VERSION = 0.0.1

PREFIX  ?= /usr/local
UNAME := $(shell uname)

CCVERSION := $(shell ${CC} --version)

CFLAGS += -Os -pedantic -std=c99 -Wall -Wextra -Wno-unused-result $(shell pcre-config --cflags)

# Force colour output
ifneq (,$(findstring LLVM,${CCVERSION}))
	CFLAGS   += -fcolor-diagnostics
endif

# OS X ships with the PCRE library but not the development headers
# Linking against the included library allows the binary to be redistributed
ifneq (${UNAME},Darwin)
	LDFLAGS  += $(shell pcre-config --libs)
endif

CPPFLAGS += -DVERSION=\"${VERSION}\" -D_POSIX_C_SOURCE=200112L
LDFLAGS += -lpcre

SOURCES=input.c patterns.c
HEADERS=parse.h input.h patterns.h editor.h

all: ${NAME} test

${NAME}: pls.c ${SOURCES} ${HEADERS}
	${CC} ${CFLAGS} $< ${SOURCES} -o $@ ${LDFLAGS} ${CPPFLAGS}

.test: test.c ${SOURCES} ${HEADERS}
	${CC} ${CFLAGS} $< ${SOURCES} -o $@ ${LDFLAGS} ${CPPFLAGS}

test: .test
	./.test
.PHONY: test

clean:
	rm ${NAME}
	rm .test

install: ${NAME}
	@echo "${NAME} -> ${PREFIX}/bin/${NAME}"
	@mkdir -p "${PREFIX}/bin"
	@cp -f ${NAME} "${PREFIX}/bin"
	@chmod 755 "${PREFIX}/bin/${NAME}"
	@echo "${NAME}.1 -> ${PREFIX}/share/man/man1/${NAME}.1"
	@mkdir -p "${PREFIX}/share/man/man1"
	@cp -f ${NAME}.1 "${PREFIX}/share/man/man1/${NAME}.1"
	@chmod 644 "${PREFIX}/share/man/man1/${NAME}.1"

.PHONY: clean install
