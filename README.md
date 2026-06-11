CSC 453 Project 4 - TinyFS

Sharon Liang

Build and Run

make
./tinyFSDemo

Design Overview

Each block is 256 bytes. Byte 0 stores the block type, and byte 1 stores the TinyFS magic number 0x44.

Block 0 is always the superblock, which stores the total block count and the first free block. Free blocks are linked together through byte 2. File data blocks are also linked together through byte 2. A byte-2 value of 0 means the end of the chain.

Inode blocks store an 8-character alphanumeric file name, file size, first data block, read-only flag, and timestamps. File data blocks store 252 bytes of content beginning at byte 4.

This implementation supports disks up to 255 blocks because block links are stored in one byte. The default 10240-byte disk is 40 blocks and works normally.

Design Tradeoffs

I chose a linked-list allocation scheme because it closely follows the structure suggested in the assignment and keeps the implementation simple. The superblock only tracks the first free block, while each free or data block only stores a pointer to the next block.

This design works well for TinyFS because the default disk size is small. The main tradeoff is that accessing data far into a file requires traversing earlier blocks in the chain, making random access less efficient than indexed allocation schemes.

Additional Functionality Implemented:
- Directory listing and file renaming: tfs_readdir, tfs_rename
- Read-only and writeByte support: tfs_makeRO, tfs_makeRW, tfs_writeByte
- Timestamps: creation, modification, and access timestamps are stored in inode blocks and displayed with tfs_readFileInfo.

Demonstration

The demo program shows:
- Creating and opening files
- Writing and reading multi-block files
- Renaming files
- Listing files
- Enabling and disabling read-only mode
- Rejecting writes and deletes on read-only files
- Overwriting a single byte with tfs_writeByte
- Displaying file metadata and timestamps
- Deleting files

Limitations
- Flat namespace only, no hierarchical directories.
- File names are limited to 1-8 alphanumeric characters.
- Maximum filesystem size is 255 blocks due to one-byte block pointers.
- Timestamp storage uses the platform's native time_t representation, so it is not very portable
