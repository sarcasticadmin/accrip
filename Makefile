CC = gcc
CFLAGS = -fPIC -Wall -O3 -pedantic -std=gnu99
PKG_CFLAGS := $(shell pkg-config --cflags flac)
PKG_LIBS := $(shell pkg-config --libs flac)

all: libarflac.o accuraterip-crcgen.o accuraterip-crcgen

accuraterip-crcgen: accuraterip-crcgen.o libarflac.o
	$(CC) $(CFLAGS) $(PKG_CFLAGS) accuraterip-crcgen.o libarflac.o $(PKG_LIBS) -o accuraterip-crcgen

accuraterip-crcgen.o: accuraterip-crcgen.c libarflac.h
	$(CC) $(CFLAGS) -c -o accuraterip-crcgen.o accuraterip-crcgen.c

libarflac.o: libarflac.h libarflac.c
	$(CC) $(CFLAGS) -c -o libarflac.o libarflac.c
