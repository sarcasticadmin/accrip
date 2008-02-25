CC = gcc
CFLAGS = -fPIC -Wall -pedantic -O3

all: libarflac.so accuraterip-crcgen

accuraterip-crcgen: accuraterip-crcgen.c
	$(CC) $(CFLAGS) -I. -L. -larflac -o accuraterip-crcgen accuraterip-crcgen.c

libarflac.so: libarflac.h libarflac.c
	$(CC) $(CFLAGS) -shared -lFLAC -o libarflac.so libarflac.c
