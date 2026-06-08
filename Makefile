CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_XOPEN_SOURCE=700
OBJS = libDisk.o libTinyFS.o tinyFSDemo.o

all: tinyFSDemo
tinyFSDemo: $(OBJS)
	$(CC) $(CFLAGS) -o tinyFSDemo $(OBJS)
libDisk.o: libDisk.c libDisk.h Makefile
	$(CC) $(CFLAGS) -c libDisk.c
libTinyFS.o: libTinyFS.c tinyFS.h tinyFS_errno.h libDisk.h Makefile
	$(CC) $(CFLAGS) -c libTinyFS.c
tinyFSDemo.o: tinyFSDemo.c tinyFS.h tinyFS_errno.h Makefile
	$(CC) $(CFLAGS) -c tinyFSDemo.c
clean:
	rm -f $(OBJS) tinyFSDemo tinyFSDisk

.PHONY: all clean
