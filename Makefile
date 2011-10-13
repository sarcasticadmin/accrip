CC = gcc
#CFLAGS = -fPIC -Wall -g -O0 -std=gnu99
CFLAGS = -fPIC -Wall -O3 -pedantic -std=gnu99
LIBDIR = /usr/lib/x86_64-linux-gnu
all: libarflac.o accuraterip-crcgen.o accuraterip-crcgen

accuraterip-crcgen: accuraterip-crcgen.o libarflac.o
#one of the following two should work...multiarch in Debian is problematic at the moment
#	$(CC) $(CFLAGS) -lm -lFLAC -logg accuraterip-crcgen.o libarflac.o -o accuraterip-crcgen
	$(CC) $(CFLAGS) accuraterip-crcgen.o libarflac.o $(LIBDIR)/libFLAC.a $(LIBDIR)/libogg.a $(LIBDIR)/libm.a -o accuraterip-crcgen

accuraterip-crcgen.o: accuraterip-crcgen.c libarflac.h
	$(CC) $(CFLAGS) -c -o accuraterip-crcgen.o accuraterip-crcgen.c

libarflac.o: libarflac.h libarflac.c
	$(CC) $(CFLAGS) -c -o libarflac.o libarflac.c
