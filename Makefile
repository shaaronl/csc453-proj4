CC = gcc
CFLAGS = -Wall -Wextra -std=c99
OBJS = libDisk.o libTinyFS.o tinyFSDemo.o
tinyFSDemo: $(OBJS)
	$(CC) $(CFLAGS) -o tinyFSDemo $(OBJS)
libDisk.o: libDisk.c libDisk.h
	$(CC) $(CFLAGS) -c libDisk.c
libTinyFS.o: libTinyFS.c tinyFS.h tinyFS_errno.h libDisk.h
	$(CC) $(CFLAGS) -c libTinyFS.c
tinyFSDemo.o: tinyFSDemo.c tinyFS.h tinyFS_errno.h
	$(CC) $(CFLAGS) -c tinyFSDemo.c
clean:
	rm -f $(OBJS) tinyFSDemo tinyFSDisk
