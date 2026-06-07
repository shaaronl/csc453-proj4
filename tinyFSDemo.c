#include "tinyFS.h"
#include <stdio.h>
#include <string.h>
static void show_result(char *label, int result) {
    printf("%-28s %s (%d)\n", label, result == TFS_SUCCESS ? "ok" : "error", result);
}
int main(void) {
    fileDescriptor alpha;
    fileDescriptor beta;
    char byte;
    char text[] = "TinyFS stores this file across blocks, then overwrites one byte.";
    show_result("mkfs", tfs_mkfs(DEFAULT_DISK_NAME, DEFAULT_DISK_SIZE));
    show_result("mount", tfs_mount(DEFAULT_DISK_NAME));
    alpha = tfs_openFile("alpha1");
    beta = tfs_openFile("beta2");
    printf("open alpha1                  fd %d\n", alpha);
    printf("open beta2                   fd %d\n", beta);
    show_result("write alpha1", tfs_writeFile(alpha, text, (int)strlen(text)));
    show_result("write beta2", tfs_writeFile(beta, "second file", 11));
    show_result("directory listing", tfs_readdir());
    show_result("seek alpha1", tfs_seek(alpha, 0));
    printf("read alpha1 first 12 bytes    ");
    for (int i = 0; i < 12; i++) {
        if (tfs_readByte(alpha, &byte) == TFS_SUCCESS) {
            putchar(byte);
        }
    }
    printf("\n");
    show_result("writeByte alpha1", tfs_writeByte(alpha, 8, 'X'));
    show_result("seek alpha1", tfs_seek(alpha, 0));
    printf("read alpha1 after writeByte  ");
    for (int i = 0; i < 12; i++) {
        if (tfs_readByte(alpha, &byte) == TFS_SUCCESS) {
            putchar(byte);
        }
    }
    printf("\n");
    show_result("rename alpha1", tfs_rename(alpha, "gamma3"));
    show_result("make beta2 read-only", tfs_makeRO("beta2"));
    show_result("write read-only beta2", tfs_writeFile(beta, "no", 2));
    show_result("delete read-only beta2", tfs_deleteFile(beta));
    show_result("make beta2 read-write", tfs_makeRW("beta2"));
    show_result("delete beta2", tfs_deleteFile(beta));
    show_result("directory listing", tfs_readdir());
    show_result("close gamma3", tfs_closeFile(alpha));
    show_result("unmount", tfs_unmount());
    return 0;
}
