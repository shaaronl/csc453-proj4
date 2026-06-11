# CSC 453 Project 4 - TinyFS

Sharon Liang

## Build and Run

```sh
make
./tinyFSDemo
```

## Design Overview

TinyFS is implemented on top of the provided disk emulator using 256-byte blocks. Every block stores a block type in byte 0 and the TinyFS magic number (`0x44`) in byte 1.

## Disk Layout

- Block 0: Superblock
- Blocks 1+: Free blocks, inode blocks, or data blocks

The superblock stores:

- Total number of blocks
- Pointer to the first free block

Free blocks are maintained as a linked list through byte 2. File data blocks are also linked together through byte 2, with a value of `0` indicating the end of the chain.

Each inode stores:

- File name, up to 8 alphanumeric characters
- File size
- First data block
- Read-only flag
- Creation, modification, and access timestamps

File data begins at byte 4 of each data block, providing 252 bytes of usable storage per block.

## Design Tradeoffs

I chose a linked-list allocation scheme because it closely follows the structure suggested in the assignment and keeps the implementation simple. The superblock only tracks the first free block, while each free or data block only stores a pointer to the next block.

This design works well for TinyFS because the default disk size is small, 40 blocks. The main tradeoff is that accessing data far into a file requires traversing earlier blocks in the chain, making random access less efficient than indexed allocation schemes.

Block links are stored in a single byte, limiting TinyFS to 255 blocks, which is sufficient for the project requirements.

## Additional Functionality

Implemented features:

## Directory Listing and Rename

- `tfs_readdir`
- `tfs_rename`

## Read-Only Files and Byte Overwrite

- `tfs_makeRO`
- `tfs_makeRW`
- `tfs_writeByte`

## Timestamps and File Information

- Creation, modification, and access timestamps stored in inode blocks
- `tfs_readFileInfo`

## Demonstration

The demo program shows:

- Creating and opening files
- Writing and reading multi-block files
- Renaming files
- Listing files
- Enabling and disabling read-only mode
- Rejecting writes and deletes on read-only files
- Overwriting a single byte with `tfs_writeByte`
- Displaying file metadata and timestamps
- Deleting files

## Limitations

- Flat namespace only, no hierarchical directories
- File names are limited to 1-8 alphanumeric characters
- `tfs_writeFile` replaces the entire file contents
- `tfs_writeByte` overwrites existing bytes but does not extend files
- Maximum filesystem size is 255 blocks due to one-byte block pointers
- Timestamp storage uses the platform's native `time_t` representation
