#include "libDisk.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
struct DiskEntry {
    int fd;
    int blocks;
};
static struct DiskEntry disks[16];
int openDisk(char *filename, int nBytes) {
    int fd;
    struct stat st;
    if (!filename || nBytes < 0) {
        return -1;
    }
    if (nBytes == 0) {
        fd = open(filename, O_RDWR);
        if (fd < 0 || fstat(fd, &st) < 0 || st.st_size < BLOCKSIZE) {
            if (fd >= 0) {
                close(fd);
            }
            return -1;
        }
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
        if (disks[i].fd <= 0) {
            disks[i].fd = fd + 1;
            disks[i].blocks = (int)(st.st_size / BLOCKSIZE);
            return i;
        }
    }
    close(fd);
    return -1;
}
int closeDisk(int disk) {
    if (disk < 0 || disk >= 16 || disks[disk].fd <= 0) {
        return -1;
    }
    close(disks[disk].fd - 1);
    disks[disk].fd = 0;
    disks[disk].blocks = 0;
    return 0;
}
int readBlock(int disk, int bNum, void *block) {
    if (disk < 0 || disk >= 16 || disks[disk].fd <= 0 || !block ||
        bNum < 0 || bNum >= disks[disk].blocks) {
        return -1;
    }
    if (lseek(disks[disk].fd - 1, (off_t)bNum * BLOCKSIZE, SEEK_SET) < 0) {
        return -1;
    }
    return read(disks[disk].fd - 1, block, BLOCKSIZE) == BLOCKSIZE ? 0 : -1;
}
int writeBlock(int disk, int bNum, void *block) {
    if (disk < 0 || disk >= 16 || disks[disk].fd <= 0 || !block ||
        bNum < 0 || bNum >= disks[disk].blocks) {
        return -1;
    }
    if (lseek(disks[disk].fd - 1, (off_t)bNum * BLOCKSIZE, SEEK_SET) < 0) {
        return -1;
    }
    return write(disks[disk].fd - 1, block, BLOCKSIZE) == BLOCKSIZE ? 0 : -1;
}
