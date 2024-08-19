# sis - simple imap server
# See LICENSE file for copyright and license details.

include config.mk

SRC = sis.c imap.c
HDR = config.def.h
OBJ = ${SRC:.c=.o}

all: options sis

options:
	@echo sis build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

config.h:
	cp config.def.h $@

sis: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f sis ${OBJ} sis-${VERSION}.tar.gz

dist: clean
	mkdir -p sis-${VERSION}
	cp -R LICENSE Makefile README config.mk\
		sis.1 ${HDR} ${SRC} sis-${VERSION}
	tar -cf sis-${VERSION}.tar sis-${VERSION}
	gzip sis-${VERSION}.tar
	rm -rf sis-${VERSION}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f sis ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/sis
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < sis.1 > ${DESTDIR}${MANPREFIX}/man1/sis.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/sis.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/dwm\
		${DESTDIR}${MANPREFIX}/man1/dwm.1

.PHONY: all options clean dist install uninstall
