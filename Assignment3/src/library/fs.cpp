// fs.cpp: File System

#include "sfs/fs.h"

#include <algorithm>

#include <assert.h>
#include <stdio.h>
#include <string>
#include <cmath>
#include <cstring>

using namespace std;

// Debug file system -----------------------------------------------------------

void FileSystem::debug(Disk *disk)
{
    Block block;
    Block block_indirect;
    unsigned int inode_block_counter = 0;
    string direct_blocks = "", indirect_blocks = "";

    // Read Superblock
    disk->read(0, block.Data);

    printf("SuperBlock:\n");
    (block.Super.MagicNumber == MAGIC_NUMBER) ? printf("    magic number is valid\n") : printf("    magic number is invalid\n");
    printf("    %u blocks\n", block.Super.Blocks);
    printf("    %u inode blocks\n", block.Super.InodeBlocks);
    printf("    %u inodes\n", block.Super.Inodes);

    // Read Inode blocks
    inode_block_counter = block.Super.InodeBlocks;

    for (unsigned int i = 0; i < inode_block_counter; i++)
    {
        disk->read(i + 1, block.Data);
        for (unsigned int j = 0; j < INODES_PER_BLOCK; j++)
        {
            direct_blocks = "";
            indirect_blocks = "";

            if (block.Inodes[j].Valid)
            {
                for (unsigned int k = 0; k < POINTERS_PER_INODE; k++)
                {
                    if (block.Inodes[j].Direct[k] != 0)
                        direct_blocks += " " + to_string(block.Inodes[j].Direct[k]);
                }

                if (block.Inodes[j].Indirect != 0)
                {
                    disk->read(block.Inodes[j].Indirect, block_indirect.Data);
                    for (unsigned int k = 0; k < POINTERS_PER_BLOCK; k++)
                    {
                        if (block_indirect.Pointers[k] != 0)
                            indirect_blocks += " " + to_string(block_indirect.Pointers[k]);
                    }
                }

                printf("Inode %u:\n", j);
                printf("    size: %u bytes\n", block.Inodes[j].Size);
                printf("    direct blocks: %s\n", direct_blocks.c_str());
                if (indirect_blocks != "")
                {
                    printf("    indirect block: %u\n", block.Inodes[j].Indirect);
                    printf("    indirect data blocks: %s\n", indirect_blocks.c_str());
                }
            }
        }
    }
}

// Format file system ----------------------------------------------------------

bool FileSystem::format(Disk *disk)
{
    // Check if mounted
    if (disk->mounted())
        return false;

    // Write superblock
    Block block;
    memset(block.Data, 0, disk->BLOCK_SIZE);
    block.Super.MagicNumber = MAGIC_NUMBER;
    block.Super.Blocks = disk->size();
    block.Super.InodeBlocks = ceil(block.Super.Blocks * 0.1);
    block.Super.Inodes = INODES_PER_BLOCK * block.Super.InodeBlocks;

    disk->write(0, block.Data);

    // Clear all other blocks
    char clear[BUFSIZ] = {0};
    for (unsigned int i = 0; i < block.Super.Blocks; i++)
        disk->write(i, clear);
    return true;
}

// Mount file system -----------------------------------------------------------

bool FileSystem::mount(Disk *disk)
{
    if (disk->mounted())
        return false;

    // Read superblock
    Block block;
    disk->read(0, block.Data);

    // Check conditions
    if (block.Super.MagicNumber != MAGIC_NUMBER)
        return false;

    if (block.Super.Blocks < 0)
        return false;

    if (block.Super.InodeBlocks != ceil(block.Super.Blocks * 0.1))
        return false;

    if (block.Super.Inodes != block.Super.InodeBlocks * INODES_PER_BLOCK)
        return false;

    // Set device and mount
    disk->mount();

    // Copy metadata
    this->total_blocks = block.Super.Blocks;
    this->total_inode_blocks = block.Super.InodeBlocks;
    this->total_inodes = block.Super.Inodes;
    this->disk = disk;

    // Allocate free block bitmap
    free_blk_bitmap = vector<int>(this->total_blocks, 1);

    free_blk_bitmap[0] = 0;

    for (unsigned int i = 0; i < this->total_inode_blocks; i++)
        free_blk_bitmap[i + 1] = 0;

    for (unsigned int i = 0; i < this->total_inode_blocks; i++)
    {
        Block temp;
        disk->read(i + 1, block.Data);

        for (unsigned int j = 0; j < INODES_PER_BLOCK; j++)
        {
            if (block.Inodes[j].Valid)
            {
                unsigned int num_blocks = ceil(temp.Inodes[j].Size / (double)disk->BLOCK_SIZE);

                for (unsigned int k = 0; k < POINTERS_PER_INODE; k++)
                    free_blk_bitmap[temp.Inodes[j].Direct[k]] = 0;

                if (num_blocks > POINTERS_PER_INODE)
                {
                    Block indirect;
                    disk->read(temp.Inodes[j].Indirect, indirect.Data);
                    free_blk_bitmap[temp.Inodes[j].Indirect] = 0;
                    for (unsigned int pointer = 0; pointer < num_blocks - POINTERS_PER_INODE; pointer++)
                        free_blk_bitmap[indirect.Pointers[pointer]] = 0;
                }
            }
        }
    }

    return true;
}

// Create inode ----------------------------------------------------------------

ssize_t FileSystem::create()
{
    ssize_t inode_num = -1;

    // Locate free inode in inode table
    for (unsigned int i = 0; i < total_inode_blocks; i++)
    {
        Block temp;
        disk->read(i + 1, temp.Data);

        for (unsigned int j = 0; j < INODES_PER_BLOCK; i++)
        {
            if (!temp.Inodes[j].Valid)
            {
                inode_num = i * INODES_PER_BLOCK + j;
                break;
            }
        }

        if (inode_num != -1)
            break;
    }

    // Record inode if found
    if (inode_num == -1)
        return inode_num;

    Inode temp;
    temp.Valid = true;
    temp.Size = 0;
    for (unsigned int i = 0; i < POINTERS_PER_INODE; i++)
        temp.Direct[i] = 0;
    temp.Indirect = 0;

    save_inode(inode_num, temp);

    return inode_num;
}

// Remove inode ----------------------------------------------------------------

bool FileSystem::remove(size_t inumber)
{
    // Load inode information

    // Free direct blocks

    // Free indirect blocks

    // Clear inode in inode table
    return true;
}

// Inode stat ------------------------------------------------------------------

ssize_t FileSystem::stat(size_t inumber)
{
    // Load inode information
    return 0;
}

// Read from inode -------------------------------------------------------------

ssize_t FileSystem::read(size_t inumber, char *data, size_t length, size_t offset)
{
    // Load inode information

    // Adjust length

    // Read block and copy to data
    return 0;
}

// Write to inode --------------------------------------------------------------

ssize_t FileSystem::write(size_t inumber, char *data, size_t length, size_t offset)
{
    // Load inode

    // Write block and copy to data
    return 0;
}

// Load inode --------------------------------------------------------------
bool FileSystem::load_inode(size_t inumber, Inode &node)
{
    size_t block_number = 1 + (inumber / INODES_PER_BLOCK);
    size_t inode_offset = inumber % INODES_PER_BLOCK;

    if (inumber >= total_inodes)
        return false;

    Block block;
    disk->read(block_number, block.Data);

    node = block.Inodes[inode_offset];

    return true;
}

// Save inode --------------------------------------------------------------
bool FileSystem::save_inode(size_t inumber, Inode &node)
{

    size_t block_number = 1 + inumber / INODES_PER_BLOCK;
    size_t inode_offset = inumber % INODES_PER_BLOCK;

    if (inumber >= total_inodes)
        return false;

    Block block;
    disk->read(block_number, block.Data);
    block.Inodes[inode_offset] = node;
    disk->write(block_number, block.Data);

    return true;
}