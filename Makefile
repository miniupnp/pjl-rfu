CFLAGS ?= -g
CFLAGS += -Wall
CFLAGS += -Wextra

LIBZ = $(shell pkg-config --libs-only-l zlib)

all:	pjl_decode clj2840_decode clj2840_strings

pjl_decode:	pjl_decode.o

clj2840_decode:	clj2840_decode.o

clj2840_strings:	clj2840_strings.o

