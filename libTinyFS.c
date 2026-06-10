#include "tinyFS.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define MAGIC 0x44
#define SUPER 1
#define INODE 2
#define DATA 3
#define FREE 4

struct OpenFile {
    int used;
    int inode;
    int fp;
};

static int mounted_disk = -1;
static int block_count = 0;
static struct OpenFile open_files[32];

/* This implementation follows the assignment's suggested byte-2 linked-list
   design. Superblock byte 2 is the first free block. Free/data blocks use byte
   2 as their next pointer. Inodes use bytes 4-12 for name, 13-16 for size,
   byte 17 for first data block, byte 18 for read-only, and bytes 24, 32, 40
   for create, modify, and access times. */

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

static int read_type(int block_num, unsigned char *block, int type) {
    if (readBlock(mounted_disk, block_num, block) < 0) {
        return TFS_ERR_DISK;
    }
    if (block[0] != type || block[1] != MAGIC) {
        return TFS_ERR_BAD_FS;
    }
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

static int find_inode(char *name, unsigned char *inode) {
    unsigned char block[BLOCKSIZE];
    char block_name[9];

    if (!valid_name(name)) {
        return TFS_ERR_BAD_NAME;
    }
    for (int i = 1; i < block_count; i++) {
        if (readBlock(mounted_disk, i, block) < 0) {
            return TFS_ERR_DISK;
        }
        if (block[0] == INODE && block[1] == MAGIC) {
            memcpy(block_name, block + 4, 8);
            block_name[8] = '\0';
            if (strcmp(block_name, name) == 0) {
                if (inode) {
                    memcpy(inode, block, BLOCKSIZE);
                }
                return i;
            }
        }
    }
    return TFS_ERR_NOT_FOUND;
}

static int alloc_block(int type) {
    unsigned char super[BLOCKSIZE];
    unsigned char block[BLOCKSIZE];
    int next;

    if (read_type(0, super, SUPER) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }
    next = super[2];
    if (next == 0) {
        return TFS_ERR_NO_SPACE;
    }
    if (read_type(next, block, FREE) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }
    super[2] = block[2];
    if (writeBlock(mounted_disk, 0, super) < 0) {
        return TFS_ERR_DISK;
    }
    memset(block, 0, BLOCKSIZE);
    block[0] = (unsigned char)type;
    block[1] = MAGIC;
    if (writeBlock(mounted_disk, next, block) < 0) {
        return TFS_ERR_DISK;
    }
    return next;
}

static int free_block(int block_num) {
    unsigned char super[BLOCKSIZE];
    unsigned char block[BLOCKSIZE];

    if (block_num < 1 || block_num >= block_count) {
        return TFS_ERR_BAD_FS;
    }
    if (read_type(0, super, SUPER) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }
    memset(block, 0, BLOCKSIZE);
    block[0] = FREE;
    block[1] = MAGIC;
    block[2] = super[2];
    super[2] = (unsigned char)block_num;
    if (writeBlock(mounted_disk, block_num, block) < 0) {
        return TFS_ERR_DISK;
    }
    return writeBlock(mounted_disk, 0, super) < 0 ? TFS_ERR_DISK : TFS_SUCCESS;
}

static int free_chain(unsigned char *inode) {
    unsigned char block[BLOCKSIZE];
    int seen[BLOCKSIZE] = {0};
    int current = inode[17];

    while (current != 0) {
        int next;

        if (current < 1 || current >= block_count || seen[current]) {
            return TFS_ERR_BAD_FS;
        }
        seen[current] = 1;
        if (read_type(current, block, DATA) != TFS_SUCCESS) {
            return TFS_ERR_BAD_FS;
        }
        next = block[2];
        if (free_block(current) != TFS_SUCCESS) {
            return TFS_ERR_DISK;
        }
        current = next;
    }
    inode[17] = 0;
    uint32_t empty_size = 0;
    memcpy(inode + 13, &empty_size, sizeof(empty_size));
    return TFS_SUCCESS;
}

static int data_block_at(int first, int skip) {
    unsigned char block[BLOCKSIZE];
    int current = first;

    for (int i = 0; i < skip; i++) {
        if (current < 1 || current >= block_count) {
            return TFS_ERR_BAD_FS;
        }
        if (read_type(current, block, DATA) != TFS_SUCCESS) {
            return TFS_ERR_BAD_FS;
        }
        current = block[2];
    }
    return current < 1 || current >= block_count ? TFS_ERR_BAD_FS : current;
}

int tfs_mkfs(char *filename, int nBytes) {
    unsigned char block[BLOCKSIZE];
    int disk;
    int blocks;
    uint32_t disk_blocks;

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
    block[0] = SUPER;
    block[1] = MAGIC;
    block[2] = blocks > 1 ? 1 : 0;
    disk_blocks = (uint32_t)blocks;
    memcpy(block + 4, &disk_blocks, sizeof(disk_blocks));
    if (writeBlock(disk, 0, block) < 0) {
        closeDisk(disk);
        return TFS_ERR_DISK;
    }

    for (int i = 1; i < blocks; i++) {
        memset(block, 0, BLOCKSIZE);
        block[0] = FREE;
        block[1] = MAGIC;
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
    int result;

    if (mounted_disk >= 0) {
        return TFS_ERR_MOUNTED;
    }
    mounted_disk = openDisk(diskname, 0);
    if (mounted_disk < 0) {
        return TFS_ERR_DISK;
    }
    if (read_type(0, super, SUPER) != TFS_SUCCESS) {
        closeDisk(mounted_disk);
        mounted_disk = -1;
        return TFS_ERR_BAD_FS;
    }
    memcpy(&disk_blocks, super + 4, sizeof(disk_blocks));
    block_count = (int)disk_blocks;
    if (block_count < 2 || block_count > 255) {
        closeDisk(mounted_disk);
        mounted_disk = -1;
        block_count = 0;
        return TFS_ERR_BAD_FS;
    }
    memset(open_files, 0, sizeof(open_files));
    result = tfs_checkConsistency();
    if (result != TFS_SUCCESS) {
        closeDisk(mounted_disk);
        mounted_disk = -1;
        block_count = 0;
        return result;
    }
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
    time_t now;
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
        inode_block = alloc_block(INODE);
        if (inode_block < 0) {
            return inode_block;
        }
        memset(inode, 0, BLOCKSIZE);
        inode[0] = INODE;
        inode[1] = MAGIC;
        memset(inode + 4, 0, 9);
        strncpy((char *)inode + 4, name, 8);
        now = time(NULL);
        memcpy(inode + 24, &now, sizeof(now));
        memcpy(inode + 32, &now, sizeof(now));
        memcpy(inode + 40, &now, sizeof(now));
        if (writeBlock(mounted_disk, inode_block, inode) < 0) {
            free_block(inode_block);
            return TFS_ERR_DISK;
        }
    } else if (inode_block < 0) {
        return inode_block;
    } else {
        now = time(NULL);
        memcpy(inode + 40, &now, sizeof(now));
        if (writeBlock(mounted_disk, inode_block, inode) < 0) {
            return TFS_ERR_DISK;
        }
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
    uint32_t file_size;
    time_t now;
    int first = 0;
    int prev = 0;
    int written = 0;
    int needed;
    int free_count = 0;
    int current_free;
    int seen[BLOCKSIZE] = {0};
    int result = check_fd(FD);

    if (result != TFS_SUCCESS) {
        return result;
    }
    if (size < 0 || (size > 0 && !buffer)) {
        return TFS_ERR_BAD_OFFSET;
    }
    if (read_type(open_files[FD].inode, inode, INODE) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }
    if (inode[18]) {
        return TFS_ERR_READ_ONLY;
    }

    result = free_chain(inode);
    if (result != TFS_SUCCESS) {
        return result;
    }
    needed = (size + 251) / 252;
    if (read_type(0, block, SUPER) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }
    current_free = block[2];
    while (current_free != 0) {
        if (current_free < 1 || current_free >= block_count || seen[current_free]) {
            return TFS_ERR_BAD_FS;
        }
        seen[current_free] = 1;
        if (read_type(current_free, block, FREE) != TFS_SUCCESS) {
            return TFS_ERR_BAD_FS;
        }
        free_count++;
        current_free = block[2];
    }
    if (free_count < needed) {
        writeBlock(mounted_disk, open_files[FD].inode, inode);
        return TFS_ERR_NO_SPACE;
    }

    while (written < size) {
        int current = alloc_block(DATA);
        int chunk = size - written > 252 ? 252 : size - written;

        if (current < 0) {
            if (first) {
                inode[17] = (unsigned char)first;
                free_chain(inode);
            }
            writeBlock(mounted_disk, open_files[FD].inode, inode);
            return current;
        }

        memset(block, 0, BLOCKSIZE);
        block[0] = DATA;
        block[1] = MAGIC;
        memcpy(block + 4, buffer + written, chunk);
        if (writeBlock(mounted_disk, current, block) < 0) {
            return TFS_ERR_DISK;
        }
        if (prev) {
            unsigned char prev_block[BLOCKSIZE];

            if (read_type(prev, prev_block, DATA) != TFS_SUCCESS) {
                return TFS_ERR_BAD_FS;
            }
            prev_block[2] = (unsigned char)current;
            if (writeBlock(mounted_disk, prev, prev_block) < 0) {
                return TFS_ERR_DISK;
            }
        }
        if (!first) {
            first = current;
        }
        prev = current;
        written += chunk;
    }

    inode[17] = (unsigned char)first;
    file_size = (uint32_t)size;
    memcpy(inode + 13, &file_size, sizeof(file_size));
    now = time(NULL);
    memcpy(inode + 32, &now, sizeof(now));
    memcpy(inode + 40, &now, sizeof(now));
    if (writeBlock(mounted_disk, open_files[FD].inode, inode) < 0) {
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
    if (read_type(inode_block, inode, INODE) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }
    if (inode[18]) {
        return TFS_ERR_READ_ONLY;
    }
    result = free_chain(inode);
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
    time_t now;
    int current;
    int result = check_fd(FD);

    if (result != TFS_SUCCESS) {
        return result;
    }
    if (!buffer) {
        return TFS_ERR_BAD_OFFSET;
    }
    if (read_type(open_files[FD].inode, inode, INODE) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }
    memcpy(&size, inode + 13, sizeof(size));
    if ((uint32_t)open_files[FD].fp >= size) {
        return TFS_ERR_EOF;
    }
    current = data_block_at(inode[17], open_files[FD].fp / 252);
    if (current < 0 || read_type(current, block, DATA) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }
    *buffer = (char)block[4 + open_files[FD].fp % 252];
    open_files[FD].fp++;
    now = time(NULL);
    memcpy(inode + 40, &now, sizeof(now));
    writeBlock(mounted_disk, open_files[FD].inode, inode);
    return TFS_SUCCESS;
}

int tfs_seek(fileDescriptor FD, int offset) {
    unsigned char inode[BLOCKSIZE];
    uint32_t size;
    time_t now;
    int result = check_fd(FD);

    if (result != TFS_SUCCESS) {
        return result;
    }
    if (read_type(open_files[FD].inode, inode, INODE) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }
    memcpy(&size, inode + 13, sizeof(size));
    if (offset < 0 || (uint32_t)offset > size) {
        return TFS_ERR_BAD_OFFSET;
    }
    open_files[FD].fp = offset;
    now = time(NULL);
    memcpy(inode + 40, &now, sizeof(now));
    return writeBlock(mounted_disk, open_files[FD].inode, inode) < 0 ? TFS_ERR_DISK : TFS_SUCCESS;
}

int tfs_rename(fileDescriptor FD, char *newName) {
    unsigned char inode[BLOCKSIZE];
    time_t now;
    int result = check_fd(FD);

    if (result != TFS_SUCCESS) {
        return result;
    }
    if (!valid_name(newName)) {
        return TFS_ERR_BAD_NAME;
    }
    if (read_type(open_files[FD].inode, inode, INODE) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }
    result = find_inode(newName, NULL);
    if (result >= 0 && result != open_files[FD].inode) {
        return TFS_ERR_EXISTS;
    }
    memset(inode + 4, 0, 9);
    strncpy((char *)inode + 4, newName, 8);
    now = time(NULL);
    memcpy(inode + 32, &now, sizeof(now));
    return writeBlock(mounted_disk, open_files[FD].inode, inode) < 0 ? TFS_ERR_DISK : TFS_SUCCESS;
}

int tfs_readdir(void) {
    unsigned char inode[BLOCKSIZE];
    char name[9];
    uint32_t size;

    if (mounted_disk < 0) {
        return TFS_ERR_NOT_MOUNTED;
    }
    printf("Files:\n");
    for (int i = 1; i < block_count; i++) {
        if (readBlock(mounted_disk, i, inode) < 0) {
            return TFS_ERR_DISK;
        }
        if (inode[0] == INODE && inode[1] == MAGIC) {
            memcpy(name, inode + 4, 8);
            name[8] = '\0';
            memcpy(&size, inode + 13, sizeof(size));
            printf("%s: %u bytes, %s\n", name, size, inode[18] ? "read-only" : "read-write");
        }
    }
    return TFS_SUCCESS;
}

int tfs_makeRO(char *name) {
    unsigned char inode[BLOCKSIZE];
    time_t now;
    int inode_block;

    if (mounted_disk < 0) {
        return TFS_ERR_NOT_MOUNTED;
    }
    inode_block = find_inode(name, inode);
    if (inode_block < 0) {
        return inode_block;
    }
    inode[18] = 1;
    now = time(NULL);
    memcpy(inode + 32, &now, sizeof(now));
    return writeBlock(mounted_disk, inode_block, inode) < 0 ? TFS_ERR_DISK : TFS_SUCCESS;
}

int tfs_makeRW(char *name) {
    unsigned char inode[BLOCKSIZE];
    time_t now;
    int inode_block;

    if (mounted_disk < 0) {
        return TFS_ERR_NOT_MOUNTED;
    }
    inode_block = find_inode(name, inode);
    if (inode_block < 0) {
        return inode_block;
    }
    inode[18] = 0;
    now = time(NULL);
    memcpy(inode + 32, &now, sizeof(now));
    return writeBlock(mounted_disk, inode_block, inode) < 0 ? TFS_ERR_DISK : TFS_SUCCESS;
}

int tfs_writeByte(fileDescriptor FD, int offset, unsigned int data) {
    unsigned char inode[BLOCKSIZE];
    unsigned char block[BLOCKSIZE];
    uint32_t size;
    time_t now;
    int current;
    int result = check_fd(FD);

    if (result != TFS_SUCCESS) {
        return result;
    }
    if (read_type(open_files[FD].inode, inode, INODE) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }
    if (inode[18]) {
        return TFS_ERR_READ_ONLY;
    }
    memcpy(&size, inode + 13, sizeof(size));
    if (offset < 0 || (uint32_t)offset >= size) {
        return TFS_ERR_BAD_OFFSET;
    }
    current = data_block_at(inode[17], offset / 252);
    if (current < 0 || read_type(current, block, DATA) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }
    block[4 + offset % 252] = (unsigned char)data;
    if (writeBlock(mounted_disk, current, block) < 0) {
        return TFS_ERR_DISK;
    }
    now = time(NULL);
    memcpy(inode + 32, &now, sizeof(now));
    memcpy(inode + 40, &now, sizeof(now));
    return writeBlock(mounted_disk, open_files[FD].inode, inode) < 0 ? TFS_ERR_DISK : TFS_SUCCESS;
}

int tfs_readFileInfo(fileDescriptor FD) {
    unsigned char inode[BLOCKSIZE];
    char name[9];
    uint32_t size;
    time_t created;
    time_t modified;
    time_t accessed;
    int result = check_fd(FD);

    if (result != TFS_SUCCESS) {
        return result;
    }
    if (read_type(open_files[FD].inode, inode, INODE) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }
    memcpy(name, inode + 4, 8);
    name[8] = '\0';
    memcpy(&size, inode + 13, sizeof(size));
    memcpy(&created, inode + 24, sizeof(created));
    memcpy(&modified, inode + 32, sizeof(modified));
    memcpy(&accessed, inode + 40, sizeof(accessed));
    printf("File info for %s\n", name);
    printf("size: %u bytes\n", size);
    printf("first block: %u\n", inode[17]);
    printf("mode: %s\n", inode[18] ? "read-only" : "read-write");
    printf("created: %s", ctime(&created));
    printf("modified: %s", ctime(&modified));
    printf("accessed: %s", ctime(&accessed));
    return TFS_SUCCESS;
}

int tfs_checkConsistency(void) {
    unsigned char super[BLOCKSIZE];
    unsigned char block[BLOCKSIZE];
    char names[BLOCKSIZE][9];
    int owner[BLOCKSIZE] = {0};
    int name_count = 0;
    int current;

    if (mounted_disk < 0) {
        return TFS_ERR_NOT_MOUNTED;
    }
    if (read_type(0, super, SUPER) != TFS_SUCCESS) {
        return TFS_ERR_BAD_FS;
    }

    current = super[2];
    while (current != 0) {
        if (current < 1 || current >= block_count || owner[current]) {
            return TFS_ERR_BAD_FS;
        }
        if (read_type(current, block, FREE) != TFS_SUCCESS) {
            return TFS_ERR_BAD_FS;
        }
        owner[current] = -1;
        current = block[2];
    }

    for (int i = 1; i < block_count; i++) {
        char name[9];
        uint32_t size;
        int blocks_needed;
        int blocks_seen = 0;

        if (readBlock(mounted_disk, i, block) < 0) {
            return TFS_ERR_DISK;
        }
        if (block[1] != MAGIC || block[0] < INODE || block[0] > FREE) {
            return TFS_ERR_BAD_FS;
        }
        if (block[0] != INODE) {
            continue;
        }
        if (owner[i]) {
            return TFS_ERR_BAD_FS;
        }
        owner[i] = i;
        memcpy(name, block + 4, 8);
        name[8] = '\0';
        if (!valid_name(name)) {
            return TFS_ERR_BAD_FS;
        }
        for (int j = 0; j < name_count; j++) {
            if (strcmp(names[j], name) == 0) {
                return TFS_ERR_BAD_FS;
            }
        }
        strcpy(names[name_count++], name);

        memcpy(&size, block + 13, sizeof(size));
        blocks_needed = (int)((size + 251) / 252);
        current = block[17];
        if ((size == 0 && current != 0) ||
            (size > 0 && (current < 1 || current >= block_count))) {
            return TFS_ERR_BAD_FS;
        }

        while (current != 0) {
            int next;

            if (current < 1 || current >= block_count || owner[current]) {
                return TFS_ERR_BAD_FS;
            }
            if (read_type(current, block, DATA) != TFS_SUCCESS) {
                return TFS_ERR_BAD_FS;
            }
            owner[current] = i;
            blocks_seen++;
            next = block[2];
            current = next;
        }
        if (blocks_seen != blocks_needed) {
            return TFS_ERR_BAD_FS;
        }
    }

    for (int i = 1; i < block_count; i++) {
        if (owner[i] == 0) {
            return TFS_ERR_BAD_FS;
        }
    }
    return TFS_SUCCESS;
}
