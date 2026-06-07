# csc453-proj4
TinyFS project implementation in C.
## Files
- `libDisk.c` / `libDisk.h`: block disk emulator.
- `libTinyFS.c` / `tinyFS.h`: TinyFS interface and implementation.
- `tinyFS_errno.h`: unified negative TinyFS error codes.
- `tinyFSDemo.c`: demo program that includes `tinyFS.h`.
- `Makefile`: builds `tinyFSDemo`.
Build and run:
```sh
make
./tinyFSDemo
```
## Design
Each block is 256 bytes. Byte 0 stores the block type, byte 1 stores the
TinyFS magic number `0x44`, and byte 2 is used as a one-byte link field.
Block 0 is always the superblock. The superblock stores the first free block
and the total block count.
Inode blocks store an 8-character alphanumeric file name, size, first data
block, and read-only flag. File data blocks store 252 bytes of content
beginning at byte 4 and use byte 2 to link to the next extent. Free blocks are
chained through byte 2.
This implementation supports disks up to 255 blocks because the assignment
block format suggests a single-byte block link. The default 10240-byte disk is
40 blocks and works normally.
## Required functionality
Implemented:
- `tfs_mkfs`
- `tfs_mount`
- `tfs_unmount`
- `tfs_openFile`
- `tfs_closeFile`
- `tfs_writeFile`
- `tfs_deleteFile`
- `tfs_readByte`
- `tfs_seek`
## Additional functionality
Implemented:
- Directory listing and rename: `tfs_readdir`, `tfs_rename`
- Read-only files and byte overwrite: `tfs_makeRO`, `tfs_makeRW`,
  `tfs_writeByte`
The demo shows creating files, writing and reading content, renaming, changing
read-only state, rejecting writes to read-only files, overwriting one byte, and
deleting a file.
## Limitations
TinyFS uses a flat namespace only. File names must be 1 to 8 alphanumeric
characters. A file's contents are replaced by `tfs_writeFile`; `tfs_writeByte`
can overwrite an existing byte but does not grow the file.
