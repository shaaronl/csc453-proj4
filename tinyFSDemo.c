#include "tinyFS.h"
#include <stdio.h>
#include <string.h>

static void show_result(char *action, int result, int expected_failure) {
    if (result == TFS_SUCCESS) {
        printf("%s succeeded\n", action);
    } else if (expected_failure) {
        printf("%s failed with error (expected) %d\n", action, result);
    } else {
        printf("%s failed with error %d\n", action, result);
    }
}

int main(void) {
    fileDescriptor alpha;
    fileDescriptor beta;
    char byte;
    char text[400];
    for (int i = 0; i < (int)sizeof(text) - 1; i++) {
        text[i] = (char)('a' + (i % 26));
    }
    memcpy(text, "TinyFS stores this file across blocks, then overwrites one byte.", 64);
    text[sizeof(text) - 1] = '\0';

    show_result("make filesystem", tfs_mkfs(DEFAULT_DISK_NAME, DEFAULT_DISK_SIZE), 0);
    show_result("mount filesystem", tfs_mount(DEFAULT_DISK_NAME), 0);
    show_result("check filesystem", tfs_checkConsistency(), 0);
    alpha = tfs_openFile("alpha1");
    beta = tfs_openFile("beta2");
    printf("open alpha1 fd %d\n", alpha);
    printf("open beta2 fd %d\n", beta);
    show_result("write alpha1", tfs_writeFile(alpha, text, (int)strlen(text)), 0);
    show_result("write beta2", tfs_writeFile(beta, "second file", 11), 0);
    show_result("show alpha1 info", tfs_readFileInfo(alpha), 0);
    show_result("list directory", tfs_readdir(), 0);
    show_result("seek alpha1 to start", tfs_seek(alpha, 0), 0);
    printf("read alpha1 first 12 bytes: ");
    for (int i = 0; i < 12; i++) {
        if (tfs_readByte(alpha, &byte) == TFS_SUCCESS) {
            putchar(byte);
        }
    }
    printf("\n");
    show_result("overwrite one byte", tfs_writeByte(alpha, 8, 'X'), 0);
    show_result("show alpha1 info", tfs_readFileInfo(alpha), 0);
    show_result("seek alpha1 to start", tfs_seek(alpha, 0), 0);
    printf("read alpha1 after writeByte: ");
    for (int i = 0; i < 12; i++) {
        if (tfs_readByte(alpha, &byte) == TFS_SUCCESS) {
            putchar(byte);
        }
    }
    printf("\n");
    show_result("rename alpha1 to gamma3", tfs_rename(alpha, "gamma3"), 0);
    show_result("make beta2 read-only", tfs_makeRO("beta2"), 0);
    show_result("try writing read-only beta2", tfs_writeFile(beta, "no", 2), 1);
    show_result("try deleting read-only beta2", tfs_deleteFile(beta), 1);
    show_result("make beta2 read-write", tfs_makeRW("beta2"), 0);
    show_result("delete beta2", tfs_deleteFile(beta), 0);
    show_result("check filesystem", tfs_checkConsistency(), 0);
    show_result("list directory", tfs_readdir(), 0);
    show_result("close gamma3", tfs_closeFile(alpha), 0);
    show_result("unmount filesystem", tfs_unmount(), 0);
    return 0;
}
