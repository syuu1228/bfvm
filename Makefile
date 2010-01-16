all: bfc bfip
bfc: bfc.o
bfip: bfip.o
clean:
	rm -f *.o bfc bfip
