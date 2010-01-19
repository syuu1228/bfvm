CFLAGS = -m32
LDFLAGS = -m32

all: bfc bfip bfjit
bfc: bfc.o
bfip: bfip.o
bfjit: bfjit.o
clean:
	rm -f *.o bfc bfip bfjit
