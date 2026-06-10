#include "libDisk.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
struct DiskEntry {
    int fd;
    int blocks;
    int in_use;
};

static struct DiskEntry disks[16];
int openDisk(char *filename, int nBytes) {
    int fd;
    struct stat st;

    if (!filename || nBytes < 0) {
        return -1;
    }
    // open an existing disk file
    if (nBytes == 0) {
        fd = open(filename, O_RDWR);
        if (fd < 0 || fstat(fd, &st) < 0 || st.st_size < BLOCKSIZE) {
            if (fd >= 0) {
                close(fd);
            }
            return -1;
        }
    // new disk
    } else {
        nBytes = (nBytes / BLOCKSIZE) * BLOCKSIZE;
        if (nBytes < BLOCKSIZE) {
            return -1;
        }
        fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0666);
        if (fd < 0 || ftruncate(fd, nBytes) < 0 || fstat(fd, &st) < 0) {
            if (fd >= 0) {
                close(fd);
            }
            return -1;
        }
    }
    for (int i = 0; i < 16; i++) {
        if (!disks[i].in_use) {
            disks[i].fd = fd;
            disks[i].blocks = (int)(st.st_size / BLOCKSIZE);
            disks[i].in_use = 1;
            return i;
        }
    }
    close(fd);
    return -1;
}

int closeDisk(int disk) {
    if (disk < 0 || disk >= 16 || !disks[disk].in_use) {
        return -1;
    }
    close(disks[disk].fd);
    disks[disk].fd = 0;
    disks[disk].blocks = 0;
    disks[disk].in_use = 0;
    return 0;
}

int readBlock(int disk, int bNum, void *block) {
    // reject invalid disk handles, null buffers, and out-of-range block numbers.
    if (disk < 0 || disk >= 16 || !disks[disk].in_use || !block ||
        bNum < 0 || bNum >= disks[disk].blocks) {
        return -1;
    }
    // move to the requested block
    if (lseek(disks[disk].fd, (off_t)bNum * BLOCKSIZE, SEEK_SET) < 0) {
        return -1;
    }
    return read(disks[disk].fd, block, BLOCKSIZE) == BLOCKSIZE ? 0 : -1;
}

int writeBlock(int disk, int bNum, void *block) {
    if (disk < 0 || disk >= 16 || !disks[disk].in_use || !block ||
        bNum < 0 || bNum >= disks[disk].blocks) {
        return -1;
    }
    if (lseek(disks[disk].fd, (off_t)bNum * BLOCKSIZE, SEEK_SET) < 0) {
        return -1;
    }
    return write(disks[disk].fd, block, BLOCKSIZE) == BLOCKSIZE ? 0 : -1;
}
