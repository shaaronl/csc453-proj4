#include "tinyFS.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
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
    fileDescriptor file1;
    fileDescriptor file2;
    char byte;
    char text[400] = "demo stores this file across blocks, then overwrites one byte.";
    int start = (int)strlen(text);
    for (int i = start; i < (int)sizeof(text) - 1; i++) {
        text[i] = (char)('a' + (i % 26));
    }
    text[sizeof(text) - 1] = '\0';
    printf("\n- setup -\n");
    show_result("make filesystem", tfs_mkfs(DEFAULT_DISK_NAME, DEFAULT_DISK_SIZE), 0);
    show_result("mount filesystem", tfs_mount(DEFAULT_DISK_NAME), 0);
    show_result("try bad filename", tfs_openFile("bad-name"), 1);
    file1 = tfs_openFile("file1");
    file2 = tfs_openFile("file2");
    printf("open file1 fd %d\n", file1);
    printf("open file2 fd %d\n", file2);
    printf("\n- write and list files - \n");
    show_result("write file1", tfs_writeFile(file1, text, (int)strlen(text)), 0);
    show_result("write file2", tfs_writeFile(file2, "second file", 11), 0);
    printf("\n");
    show_result("show file1 info", tfs_readFileInfo(file1), 0);
    printf("\n");
    show_result("list directory", tfs_readdir(), 0);
    printf("\n- read and overwrite -\n");
    sleep(1);
    show_result("try seek past EOF", tfs_seek(file1, 9999), 1);
    show_result("move file1 to start", tfs_seek(file1, 0), 0);
    printf("read file1 first 12 bytes: ");
    for (int i = 0; i < 12; i++) {
        if (tfs_readByte(file1, &byte) == TFS_SUCCESS) {
            putchar(byte);
        }
    }
    printf("\n");
    show_result("show file1 info after read", tfs_readFileInfo(file1), 0);
    printf("\n");
    sleep(3);
    show_result("overwrite one byte", tfs_writeByte(file1, 8, 'X'), 0);
    show_result("show file1 info after overwrite", tfs_readFileInfo(file1), 0);
    printf("\n");
    show_result("move file1 to start", tfs_seek(file1, 0), 0);
    printf("read file1 after writeByte: ");
    for (int i = 0; i < 12; i++) {
        if (tfs_readByte(file1, &byte) == TFS_SUCCESS) {
            putchar(byte);
        }
    }
    printf("\n");
    printf("\n- rename and read-only test -\n");
    show_result("rename file1 to file3", tfs_rename(file1, "file3"), 0);
    show_result("make file2 read-only", tfs_makeRO("file2"), 0);
    show_result("try writing read-only file2", tfs_writeFile(file2, "no", 2), 1);
    show_result("try deleting read-only file2", tfs_deleteFile(file2), 1);
    show_result("make file2 read-write", tfs_makeRW("file2"), 0);
    show_result("delete file2", tfs_deleteFile(file2), 0);
    show_result("list directory", tfs_readdir(), 0);
    printf("\n- cleanup - \n");
    show_result("close file3", tfs_closeFile(file1), 0);
    show_result("unmount filesystem", tfs_unmount(), 0);
    return 0;
}
