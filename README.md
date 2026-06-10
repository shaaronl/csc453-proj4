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
Each block is 256 bytes. Byte 0 stores the block type, and byte 1 stores the
TinyFS magic number `0x44`.
- Block 0 is always the superblock, which stores the total block count and the
  first free block.
- Free blocks are linked together through byte 2.
- File data blocks are also linked together through byte 2.
- A byte-2 value of 0 means the end of the chain.

Inode blocks store an 8-character alphanumeric file name, file size, first data
block, and read-only flag. File data blocks store 252 bytes of content
beginning at byte 4.

This implementation supports disks up to 255 blocks because block links are
stored in one byte. The default 10240-byte disk is 40 blocks and works normally.
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
- Timestamps and file info: creation, modification, and access times stored in
  each inode and displayed by `tfs_readFileInfo`
- File system consistency checks: `tfs_checkConsistency`, also run during
  `tfs_mount`

These features were chosen because they build directly on the existing flat
file/inode design without requiring hierarchical directories or a major disk
format redesign.

Tradeoffs:
- Directory support is intentionally a flat listing of all inode blocks rather
  than hierarchical directories. This keeps path handling simple and preserves
  the original one-inode-per-file layout.
- `tfs_rename` requires the file to already be open, matching the assignment
  interface. It rejects invalid names and duplicate names so the flat namespace
  stays unambiguous.
- The read-only flag is stored in the inode block. This makes permission checks
  simple and persistent across unmount/mount, but it only supports one mode bit:
  read-only or read-write.
- `tfs_writeByte` overwrites an existing byte at an offset. It does not grow the
  file because growing would require allocating/linking more data blocks and
  updating file size semantics beyond a simple byte overwrite.
- Timestamps are stored as `time_t` values in unused inode bytes.
- Consistency checking catches bad magic/type values, invalid block links,
  duplicate block ownership, mismatched file sizes, and allocated blocks that
  are not reachable from an inode.

The demo shows creating files, writing and reading multi-block content,
renaming, changing read-only state, rejecting writes to read-only files,
overwriting one byte, printing timestamped file info, deleting a file, and
running consistency checks.
## Limitations
TinyFS uses a flat namespace only. File names must be 1 to 8 alphanumeric
characters. A file's contents are replaced by `tfs_writeFile`; `tfs_writeByte`
can overwrite an existing byte but does not grow the file. Block links are
stored in one byte, so this is for small TinyFS disk images rather than large
production-scale filesystems.
