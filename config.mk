# sis version
VERSION = 0.1

# Customize below to fit your system

# paths
PREFIX = /usr
MANPREFIX = ${PREFIX}/share/man

# OpenBSD (uncomment)
#MANPREFIX = ${PREFIX}/man

# includes and libs
INCS = -I.
LIBS = -lssl -lcrypto
# flags
CPPFLAGS = -DVERSION=\"${VERSION}\" 
CFLAGS  := -std=c99 -pedantic -Wall -O0 -Wno-gnu-label-as-value -Wno-gnu-zero-variadic-macro-arguments ${INCS} ${CPPFLAGS} 
CFLAGS  := ${CFLAGS} -g
LDFLAGS  = ${LIBS}

# Solaris
#CFLAGS = -fast ${INCS} -DVERSION=\"${VERSION}\"
#LDFLAGS = ${LIBS}

# compiler and linker
CC = cc
