# Target library
lib := libfs.a

all: $(lib)

## TODO: Phase 1

disk.o: disk.h disk.c
	gcc -c disk.c

fs.o: fs.h fs.c
	gcc -c fs.c

libfs.a: disk.o fs.o
	ar rcs libfs.a disk.o fs.o

clean:
	rm -f *.a *.o
