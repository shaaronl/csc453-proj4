#include "tinyFS.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
struct OpenFile {
    int used;
    int inode;
    int fp;
};
static int mounted_disk = -1;
static int block_count = 0;
static struct OpenFile open_files[32];
static int valid_name(char *name) {
    int len;
    if (!name) {
        return 0;
    }
    len = (int)strlen(name);
    if (len < 1 || len > 8) {
        return 0;
    }
    for (int i = 0; i < len; i++) {
        if (!isalnum((unsigned char)name[i])) {
            return 0;
        }
    }
    return 1;
}
static int read_tfs_block(int bNum, unsigned char *block, int type) {
    if (readBlock(mounted_disk, bNum, block) < 0) {
        return TFS_ERR_DISK;
    }
    if (block[0] != type || block[1] != 0x44) {
        return TFS_ERR_BAD_FS;
    }
    return TFS_SUCCESS;
}
static int write_tfs_block(int bNum, unsigned char *block) {
    return writeBlock(mounted_disk, bNum, block) < 0 ? TFS_ERR_DISK : TFS_SUCCESS;
}
static int find_inode(char *name, unsigned char *inode) {
    unsigned char block[BLOCKSIZE];
    if (!valid_name(name)) {
        return TFS_ERR_BAD_NAME;
    }
    for (int i = 1; i < block_count; i++) {
        if (readBlock(mounted_disk, i, block) < 0) {
            return TFS_ERR_DISK;
        }
        if (block[0] == 2 && block[1] == 0x44 && strcmp((char *)&block[4], name) == 0) {
            if (inode) {
                memcpy(inode, block, BLOCKSIZE);
            }
            return i;
        }
    }
    return TFS_ERR_NOT_FOUND;
}
static int alloc_block(void) {
    unsigned char super[BLOCKSIZE];
    unsigned char block[BLOCKSIZE];
    int next;
    if (read_tfs_block(0, super, 1) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }
    if (super[2] == 0) {
        return TFS_ERR_NO_SPACE;
    }
    next = super[2];
    if (read_tfs_block(next, block, 4) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }
    super[2] = block[2];
    if (write_tfs_block(0, super) != TFS_SUCCESS) {
        return TFS_ERR_DISK;
    }
    memset(block, 0, BLOCKSIZE);
    return next;
}
static int free_block(int bNum) {
    unsigned char super[BLOCKSIZE];
    unsigned char block[BLOCKSIZE];
    if (bNum <= 0 || bNum >= block_count) {
        return TFS_ERR_BAD_FS;
    }
    if (read_tfs_block(0, super, 1) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }
    memset(block, 0, BLOCKSIZE);
    block[0] = 4;
    block[1] = 0x44;
    block[2] = super[2];
    super[2] = (unsigned char)bNum;
    if (write_tfs_block(bNum, block) != TFS_SUCCESS) {
        return TFS_ERR_DISK;
    }
    return write_tfs_block(0, super);
}
static int free_extents(unsigned char *inode) {
    unsigned char block[BLOCKSIZE];
    int current = inode[17];
    uint32_t empty_size = 0;
    while (current != 0) {
        if (read_tfs_block(current, block, 3) != TFS_SUCCESS) {
            return TFS_ERR_BAD_FS;
        }
        int next = block[2];
        int result = free_block(current);
        if (result != TFS_SUCCESS) {
            return result;
        }
        current = next;
    }
    inode[17] = 0;
    memcpy(&inode[13], &empty_size, sizeof(empty_size));
    return TFS_SUCCESS;
}
static int check_fd(fileDescriptor FD) {
    if (mounted_disk < 0) {
        return TFS_ERR_NOT_MOUNTED;
    }
    if (FD < 0 || FD >= 32 || !open_files[FD].used) {
        return TFS_ERR_BAD_FD;
    }
    return TFS_SUCCESS;
}
int tfs_mkfs(char *filename, int nBytes) {
    unsigned char block[BLOCKSIZE];
    int disk;
    int blocks;
    if (nBytes == 0) {
        nBytes = DEFAULT_DISK_SIZE;
    }
    blocks = nBytes / BLOCKSIZE;
    if (blocks < 2 || blocks > 255) {
        return TFS_ERR_DISK;
    }
    disk = openDisk(filename, nBytes);
    if (disk < 0) {
        return TFS_ERR_DISK;
    }
    memset(block, 0, BLOCKSIZE);
    block[0] = 1;
    block[1] = 0x44;
    block[2] = blocks > 1 ? 1 : 0;
    uint32_t disk_blocks = (uint32_t)blocks;
    memcpy(&block[4], &disk_blocks, sizeof(disk_blocks));
    if (writeBlock(disk, 0, block) < 0) {
        closeDisk(disk);
        return TFS_ERR_DISK;
    }
    for (int i = 1; i < blocks; i++) {
        memset(block, 0, BLOCKSIZE);
        block[0] = 4;
        block[1] = 0x44;
        block[2] = (unsigned char)(i + 1 < blocks ? i + 1 : 0);
        if (writeBlock(disk, i, block) < 0) {
            closeDisk(disk);
            return TFS_ERR_DISK;
        }
    }
    closeDisk(disk);
    return TFS_SUCCESS;
}
int tfs_mount(char *diskname) {
    unsigned char super[BLOCKSIZE];
    uint32_t disk_blocks;
    if (mounted_disk >= 0) {
        return TFS_ERR_MOUNTED;
    }
    mounted_disk = openDisk(diskname, 0);
    if (mounted_disk < 0) {
        return TFS_ERR_DISK;
    }
    if (read_tfs_block(0, super, 1) != TFS_SUCCESS) {
        closeDisk(mounted_disk);
        mounted_disk = -1;
        return TFS_ERR_BAD_FS;
    }
    memcpy(&disk_blocks, &super[4], sizeof(disk_blocks));
    block_count = (int)disk_blocks;
    if (block_count < 2 || block_count > 255) {
        closeDisk(mounted_disk);
        mounted_disk = -1;
        block_count = 0;
        return TFS_ERR_BAD_FS;
    }
    memset(open_files, 0, sizeof(open_files));
    return TFS_SUCCESS;
}
int tfs_unmount(void) {
    if (mounted_disk < 0) {
        return TFS_ERR_NOT_MOUNTED;
    }
    closeDisk(mounted_disk);
    mounted_disk = -1;
    block_count = 0;
    memset(open_files, 0, sizeof(open_files));
    return TFS_SUCCESS;
}
fileDescriptor tfs_openFile(char *name) {
    unsigned char inode[BLOCKSIZE];
    int inode_block;
    int fd = -1;
    if (mounted_disk < 0) {
        return TFS_ERR_NOT_MOUNTED;
    }
    if (!valid_name(name)) {
        return TFS_ERR_BAD_NAME;
    }
    for (int i = 0; i < 32; i++) {
        if (!open_files[i].used) {
            fd = i;
            break;
        }
    }
    if (fd < 0) {
        return TFS_ERR_NO_SPACE;
    }
    inode_block = find_inode(name, inode);
    if (inode_block == TFS_ERR_NOT_FOUND) {
        inode_block = alloc_block();
        if (inode_block < 0) {
            return inode_block;
        }
        memset(inode, 0, BLOCKSIZE);
        inode[0] = 2;
        inode[1] = 0x44;
        strncpy((char *)&inode[4], name, 8);
        if (write_tfs_block(inode_block, inode) != TFS_SUCCESS) {
            free_block(inode_block);
            return TFS_ERR_DISK;
        }
    } else if (inode_block < 0) {
        return inode_block;
    }
    open_files[fd].used = 1;
    open_files[fd].inode = inode_block;
    open_files[fd].fp = 0;
    return fd;
}
int tfs_closeFile(fileDescriptor FD) {
    int result = check_fd(FD);
    if (result != TFS_SUCCESS) {
        return result;
    }
    open_files[FD].used = 0;
    return TFS_SUCCESS;
}
int tfs_writeFile(fileDescriptor FD, char *buffer, int size) {
    unsigned char inode[BLOCKSIZE];
    unsigned char block[BLOCKSIZE];
    int first = 0;
    int previous = 0;
    int written = 0;
    int result = check_fd(FD);
    if (result != TFS_SUCCESS) {
        return result;
    }
    if (size < 0 || (size > 0 && !buffer)) {
        return TFS_ERR_BAD_OFFSET;
    }
    if (read_tfs_block(open_files[FD].inode, inode, 2) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }
    if (inode[18]) {
        return TFS_ERR_READ_ONLY;
    }
    result = free_extents(inode);
    if (result != TFS_SUCCESS) {
        return result;
    }
    if (write_tfs_block(open_files[FD].inode, inode) != TFS_SUCCESS) {
        return TFS_ERR_DISK;
    }
    while (written < size) {
        int current = alloc_block();
        int chunk = size - written > 252 ? 252 : size - written;
        if (current < 0) {
            if (first != 0) {
                inode[17] = (unsigned char)first;
                free_extents(inode);
            }
            memset(&inode[13], 0, 5);
            write_tfs_block(open_files[FD].inode, inode);
            return current;
        }
        memset(block, 0, BLOCKSIZE);
        block[0] = 3;
        block[1] = 0x44;
        memcpy(&block[4], buffer + written, chunk);
        if (first == 0) {
            first = current;
        }
        if (previous != 0) {
            unsigned char prev_block[BLOCKSIZE];
            if (read_tfs_block(previous, prev_block, 3) != TFS_SUCCESS) {
                return TFS_ERR_BAD_FS;
            }
            prev_block[2] = (unsigned char)current;
            if (write_tfs_block(previous, prev_block) != TFS_SUCCESS) {
                return TFS_ERR_DISK;
            }
        }
        if (write_tfs_block(current, block) != TFS_SUCCESS) {
            return TFS_ERR_DISK;
        }
        previous = current;
        written += chunk;
    }
    inode[17] = (unsigned char)first;
    uint32_t file_size = (uint32_t)size;
    memcpy(&inode[13], &file_size, sizeof(file_size));
    if (write_tfs_block(open_files[FD].inode, inode) != TFS_SUCCESS) {
        return TFS_ERR_DISK;
    }
    open_files[FD].fp = 0;
    return TFS_SUCCESS;
}
int tfs_deleteFile(fileDescriptor FD) {
    unsigned char inode[BLOCKSIZE];
    int inode_block;
    int result = check_fd(FD);
    if (result != TFS_SUCCESS) {
        return result;
    }
    inode_block = open_files[FD].inode;
    if (read_tfs_block(inode_block, inode, 2) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }
    if (inode[18]) {
        return TFS_ERR_READ_ONLY;
    }
    result = free_extents(inode);
    if (result != TFS_SUCCESS) {
        return result;
    }
    result = free_block(inode_block);
    if (result != TFS_SUCCESS) {
        return result;
    }
    for (int i = 0; i < 32; i++) {
        if (open_files[i].used && open_files[i].inode == inode_block) {
            open_files[i].used = 0;
        }
    }
    return TFS_SUCCESS;
}
int tfs_readByte(fileDescriptor FD, char *buffer) {
    unsigned char inode[BLOCKSIZE];
    unsigned char block[BLOCKSIZE];
    uint32_t size;
    int current;
    int result = check_fd(FD);
    if (result != TFS_SUCCESS) {
        return result;
    }
    if (!buffer) {
        return TFS_ERR_BAD_OFFSET;
    }
    if (read_tfs_block(open_files[FD].inode, inode, 2) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }
    memcpy(&size, &inode[13], sizeof(size));
    if ((uint32_t)open_files[FD].fp >= size) {
        return TFS_ERR_EOF;
    }
    current = inode[17];
    for (int i = 0; i < open_files[FD].fp / 252; i++) {
        if (read_tfs_block(current, block, 3) != TFS_SUCCESS) {
            return TFS_ERR_BAD_FS;
        }
        current = block[2];
    }
    if (read_tfs_block(current, block, 3) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }
    *buffer = (char)block[4 + open_files[FD].fp % 252];
    open_files[FD].fp++;
    return TFS_SUCCESS;
}
int tfs_seek(fileDescriptor FD, int offset) {
    unsigned char inode[BLOCKSIZE];
    uint32_t size;
    int result = check_fd(FD);
    if (result != TFS_SUCCESS) {
        return result;
    }
    if (read_tfs_block(open_files[FD].inode, inode, 2) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }
    memcpy(&size, &inode[13], sizeof(size));
    if (offset < 0 || (uint32_t)offset > size) {
        return TFS_ERR_BAD_OFFSET;
    }
    open_files[FD].fp = offset;
    return TFS_SUCCESS;
}
int tfs_rename(fileDescriptor FD, char *newName) {
    unsigned char inode[BLOCKSIZE];
    int result = check_fd(FD);
    if (result != TFS_SUCCESS) {
        return result;
    }
    if (!valid_name(newName)) {
        return TFS_ERR_BAD_NAME;
    }
    if (read_tfs_block(open_files[FD].inode, inode, 2) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }
    result = find_inode(newName, NULL);
    if (result >= 0 && result != open_files[FD].inode) {
        return TFS_ERR_EXISTS;
    }
    memset(&inode[4], 0, 9);
    strncpy((char *)&inode[4], newName, 8);
    return write_tfs_block(open_files[FD].inode, inode);
}
int tfs_readdir(void) {
    unsigned char inode[BLOCKSIZE];
    uint32_t size;
    if (mounted_disk < 0) {
        return TFS_ERR_NOT_MOUNTED;
    }
    printf("Files:\n");
    for (int i = 1; i < block_count; i++) {
        if (readBlock(mounted_disk, i, inode) < 0) {
            return TFS_ERR_DISK;
        }
        if (inode[0] == 2 && inode[1] == 0x44) {
            memcpy(&size, &inode[13], sizeof(size));
            printf("  %-8s %u bytes%s\n", &inode[4], size, inode[18] ? " RO" : " RW");
        }
    }
    return TFS_SUCCESS;
}
int tfs_makeRO(char *name) {
    unsigned char inode[BLOCKSIZE];
    int inode_block;
    if (mounted_disk < 0) {
        return TFS_ERR_NOT_MOUNTED;
    }
    inode_block = find_inode(name, inode);
    if (inode_block < 0) {
        return inode_block;
    }
    inode[18] = 1;
    return write_tfs_block(inode_block, inode);
}
int tfs_makeRW(char *name) {
    unsigned char inode[BLOCKSIZE];
    int inode_block;
    if (mounted_disk < 0) {
        return TFS_ERR_NOT_MOUNTED;
    }
    inode_block = find_inode(name, inode);
    if (inode_block < 0) {
        return inode_block;
    }
    inode[18] = 0;
    return write_tfs_block(inode_block, inode);
}
int tfs_writeByte(fileDescriptor FD, int offset, unsigned int data) {
    unsigned char inode[BLOCKSIZE];
    unsigned char block[BLOCKSIZE];
    uint32_t size;
    int current;
    int result = check_fd(FD);
    if (result != TFS_SUCCESS) {
        return result;
    }
    if (read_tfs_block(open_files[FD].inode, inode, 2) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }
    if (inode[18]) {
        return TFS_ERR_READ_ONLY;
    }
    memcpy(&size, &inode[13], sizeof(size));
    if (offset < 0 || (uint32_t)offset >= size) {
        return TFS_ERR_BAD_OFFSET;
    }
    current = inode[17];
    for (int i = 0; i < offset / 252; i++) {
        if (read_tfs_block(current, block, 3) != TFS_SUCCESS) {
            return TFS_ERR_BAD_FS;
        }
        current = block[2];
    }
    if (read_tfs_block(current, block, 3) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }
    block[4 + offset % 252] = (unsigned char)data;
    return write_tfs_block(current, block);
}
