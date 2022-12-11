// fs.cpp: File System

#include "sfs/fs.h"

#include <algorithm>
#include <assert.h>
#include <stdio.h>
#include <string>
#include <cstring>
#include <cmath>

using namespace std;

// Debug file system -----------------------------------------------------------
void FileSystem::debug(Disk *disk)
{
    Block block;
    Block block_indirect;
    unsigned int inode_block_counter = 0;
    string direct_blocks, indirect_blocks;

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
                printf("    direct blocks:%s\n", direct_blocks.c_str());
                if (indirect_blocks != "")
                {
                    printf("    indirect block: %u\n", block.Inodes[j].Indirect);
                    printf("    indirect data blocks:%s\n", indirect_blocks.c_str());
                }
            }
        }
    }
}

// Format file system ----------------------------------------------------------
bool FileSystem::format(Disk *disk)
{
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

    for (unsigned int i = 1; i < block.Super.Blocks; i++)
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

    if (block.Super.Inodes != block.Super.InodeBlocks * INODES_PER_BLOCK ||
        block.Super.MagicNumber != MAGIC_NUMBER ||
        block.Super.Blocks < 0 ||
        block.Super.InodeBlocks != ceil(.1 * block.Super.Blocks))
        return false;

    // Set device and mount
    disk->mount();

    // Copy metadata
    this->num_blocks = block.Super.Blocks;
    this->num_inode_blocks = block.Super.InodeBlocks;
    this->num_inodes = block.Super.Inodes;
    this->disk = disk;

    // Allocate free block bitmap
    free_bitmap = vector<int>(num_blocks, 1);

    free_bitmap[0] = 0;

    for (unsigned int i = 0; i < num_inode_blocks; i++)
        free_bitmap[1 + i] = 0;

    for (unsigned int inode_block = 0; inode_block < num_inode_blocks; inode_block++)
    {
        Block b;
        disk->read(1 + inode_block, b.Data);

        // reads each inode
        for (unsigned int inode = 0; inode < INODES_PER_BLOCK; inode++)
        {
            if (!b.Inodes[inode].Valid)
                continue;

            unsigned int n_blocks = (unsigned int)ceil(b.Inodes[inode].Size / (double)disk->BLOCK_SIZE);

            for (unsigned int pointer = 0; pointer < POINTERS_PER_INODE && pointer < n_blocks; pointer++)
                free_bitmap[b.Inodes[inode].Direct[pointer]] = 0;

            if (n_blocks > POINTERS_PER_INODE)
            {
                Block indirect;
                disk->read(b.Inodes[inode].Indirect, indirect.Data);
                free_bitmap[b.Inodes[inode].Indirect] = 0;
                for (unsigned int pointer = 0; pointer < n_blocks - POINTERS_PER_INODE; pointer++)
                    free_bitmap[indirect.Pointers[pointer]] = 0;
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
    for (unsigned int i = 0; i < this->num_inode_blocks; i++)
    {
        Block temp;
        disk->read(i + 1, temp.Data);

        for (unsigned int j = 0; j < INODES_PER_BLOCK; j++)
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

    // Return inode if found
    if (inode_num == -1)
        return inode_num;

    Inode temp;
    temp.Valid = true;
    temp.Size = 0;
    for (unsigned int i = 0; i < POINTERS_PER_INODE; i++)
        temp.Direct[i] = 0;
    temp.Indirect = 0;

    save_inode(inode_num, &temp);

    return inode_num;
}

// Remove inode ----------------------------------------------------------------
bool FileSystem::remove(size_t inumber)
{
    Inode node;

    // Load inode information
    if (!load_inode(inumber, &node) || !node.Valid)
        return false;

    // Free direct blocks
    for (unsigned int i = 0; i < POINTERS_PER_INODE; i++)
    {
        if (node.Direct[i] != 0)
        {
            free_bitmap[node.Direct[i]] = 1;
            node.Direct[i] = 0;
        }
    }

    // Free indirect blocks
    if (node.Indirect != 0)
    {
        Block b;
        disk->read(node.Indirect, b.Data);

        for (unsigned int i = 0; i < POINTERS_PER_BLOCK; i++)
        {
            if (b.Pointers[i] != 0)
                free_bitmap[b.Pointers[i]] = 1;
        }

        free_bitmap[node.Indirect] = 1;
    }

    // Clear inode in inode table
    node.Indirect = 0;
    node.Valid = 0;
    node.Size = 0;

    if (!save_inode(inumber, &node))
        return false;

    return true;
}

// Inode stat ------------------------------------------------------------------
ssize_t FileSystem::stat(size_t inumber)
{
    Inode i;

    // Load inode information
    if (!load_inode(inumber, &i) || !i.Valid)
        return -1;

    return i.Size;
}

// Read from inode -------------------------------------------------------------
ssize_t FileSystem::read(size_t inumber, char *data, size_t length, size_t offset)
{
    // Load inode information
    Inode inode;
    if (!load_inode(inumber, &inode) || offset > inode.Size || !inode.Valid)
        return -1;

    // Adjust length
    length = min(length, inode.Size - offset);

    unsigned int start_block = offset / disk->BLOCK_SIZE;

    // Read block and copy to data
    Block indirect;
    if ((offset + length) / disk->BLOCK_SIZE > POINTERS_PER_INODE)
    {
        if (inode.Indirect == 0)
            return -1;

        disk->read(inode.Indirect, indirect.Data);
    }

    size_t read = 0;
    for (unsigned int block_num = start_block; read < length; block_num++)
    {
        size_t block_to_read;
        if (block_num < POINTERS_PER_INODE)
            block_to_read = inode.Direct[block_num];

        else
            block_to_read = indirect.Pointers[block_num - POINTERS_PER_INODE];

        if (block_to_read == 0)
            return -1;

        Block b;
        disk->read(block_to_read, b.Data);
        size_t read_offset;
        size_t read_length;

        if (read == 0)
        {
            read_offset = offset % disk->BLOCK_SIZE;
            read_length = min(disk->BLOCK_SIZE - read_offset, length);
        }

        else
        {
            read_offset = 0;
            read_length = min(disk->BLOCK_SIZE - 0, length - read);
        }

        memcpy(data + read, b.Data + read_offset, read_length);
        read += read_length;
    }

    return read;
}

// Write to inode --------------------------------------------------------------
ssize_t FileSystem::write(size_t inumber, char *data, size_t length, size_t offset)
{
    // Load inode
    Inode inode;
    if (!load_inode(inumber, &inode) || offset > inode.Size)
        return -1;

    size_t MAX_FILE_SIZE = disk->BLOCK_SIZE * (POINTERS_PER_INODE * POINTERS_PER_BLOCK);

    length = min(length, MAX_FILE_SIZE - offset);

    unsigned int start_block = offset / disk->BLOCK_SIZE;
    Block indirect;
    bool read_indirect = false;

    bool modified_inode = false;
    bool modified_indirect = false;

    // Write block and copy data
    size_t written = 0;
    for (unsigned int block_num = start_block; written < length && block_num < POINTERS_PER_INODE + POINTERS_PER_BLOCK; block_num++)
    {
        size_t block_to_write;
        if (block_num < POINTERS_PER_INODE)
        {
            if (inode.Direct[block_num] == 0)
            {
                ssize_t allocated_block = allocate_free_block();
                if (allocated_block == -1)
                    break;

                inode.Direct[block_num] = allocated_block;
                modified_inode = true;
            }
            block_to_write = inode.Direct[block_num];
        }

        else
        {
            if (inode.Indirect == 0)
            {
                ssize_t allocated_block = allocate_free_block();
                if (allocated_block == -1)
                    return written;

                inode.Indirect = allocated_block;
                modified_indirect = true;
            }

            if (!read_indirect)
            {
                disk->read(inode.Indirect, indirect.Data);
                read_indirect = true;
            }

            if (indirect.Pointers[block_num - POINTERS_PER_INODE] == 0)
            {
                ssize_t allocated_block = allocate_free_block();
                if (allocated_block == -1)
                    break;

                indirect.Pointers[block_num - POINTERS_PER_INODE] = allocated_block;
                modified_indirect = true;
            }
            block_to_write = indirect.Pointers[block_num - POINTERS_PER_INODE];
        }

        size_t write_offset;
        size_t write_length;

        if (written == 0)
        {
            write_offset = offset % disk->BLOCK_SIZE;
            write_length = min(disk->BLOCK_SIZE - write_offset, length);
        }
        else
        {
            write_offset = 0;
            write_length = min(disk->BLOCK_SIZE - 0, length - written);
        }

        char write_buffer[disk->BLOCK_SIZE];

        if (write_length < disk->BLOCK_SIZE)
            disk->read(block_to_write, (char *)write_buffer);

        memcpy(write_buffer + write_offset, data + written, write_length);
        disk->write(block_to_write, (char *)write_buffer);
        written += write_length;
    }

    unsigned int new_size = max((size_t)inode.Size, written + offset);
    if (new_size != inode.Size)
    {
        inode.Size = new_size;
        modified_inode = true;
    }

    if (modified_inode)
        save_inode(inumber, &inode);

    if (modified_indirect)
        disk->write(inode.Indirect, indirect.Data);

    return written;
}

// Allocate free block --------------------------------------------------------------
ssize_t FileSystem::allocate_free_block()
{
    int block = -1;
    for (unsigned int i = 0; i < num_blocks; i++)
    {
        if (free_bitmap[i])
        {
            free_bitmap[i] = 0;
            block = i;
            break;
        }
    }

    if (block != -1)
    {
        char data[disk->BLOCK_SIZE];
        memset(data, 0, disk->BLOCK_SIZE);
        disk->write(block, (char *)data);
    }

    return block;
}

// Load inode --------------------------------------------------------------
bool FileSystem::load_inode(size_t inumber, Inode *node)
{
    size_t block_number = inumber / INODES_PER_BLOCK;
    size_t inode_offset = inumber % INODES_PER_BLOCK;

    if (inumber >= num_inodes)
        return false;

    Block block;
    disk->read(block_number + 1, block.Data);

    *node = block.Inodes[inode_offset];

    return true;
}

// Save inode --------------------------------------------------------------
bool FileSystem::save_inode(size_t inumber, Inode *node)
{

    size_t block_number = inumber / INODES_PER_BLOCK;
    size_t inode_offset = inumber % INODES_PER_BLOCK;

    if (inumber >= num_inodes)
        return false;

    Block block;
    disk->read(block_number + 1, block.Data);
    block.Inodes[inode_offset] = *node;
    disk->write(block_number + 1, block.Data);

    return true;
}