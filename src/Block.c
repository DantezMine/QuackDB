#include "Block.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void Block_To_Path(uint32_t block_ID, char *fname, size_t buf_len,
        const char *dir);

void Block_Initialize(Block *block, uint32_t block_size, uint32_t block_id) {
    block->header.block_size = block_size & ~0x3; // force four byte alignment
    block->header.block_ID = block_id;
    block->header.dirty_flag = 0;
    return;
}


uint32_t Block_Get_Size(Block *block) {
    return block->header.block_size;
}

uint32_t Block_Get_ID(Block *block) {
    return block->header.block_ID;
}

void *Block_Get_Data(Block *block) {
    return block->data;
}

bool Block_Is_Dirty(Block *block) {
    return block->header.dirty_flag & 0x1;
}

bool Block_Write_To_File(Block *block, FILE *fd) { 
    size_t n = fwrite(block, Block_Get_Size(block), 1, fd);
    // n corresponds to number of elements, not bytes, written
    if (n != 1) {
        perror("Block_Write_To_File::");
        return false;
    }
    return true;
}

// Buffer must have enough space for directory name, a slash and
// 10 digits and null-terminator.
// uint32_t is at most 10 characters in decimal
static void Block_To_Path(uint32_t block_ID, char *path, size_t buf_len,
        const char *dir) {
    if (buf_len < strlen(dir) + 12) {
        fprintf(stderr, "Block_To_Path:: Given buffer length is too short.\n");
        return;
    }
    sprintf(path, "%s/%u", dir, block_ID);
    return;
}

bool Block_Delete_File(uint32_t block_ID, const char *dir) {
    size_t dir_len = strlen(dir);
    char path[12+dir_len];
    Block_To_Path(block_ID, path, 12+dir_len, dir);
    if (remove(path) != 0) {
        return false;
    }
    return true;
}

void Block_Free(Block *block) {
    free(block);
}

uint32_t Block_Get_Size_From_File(FILE *fd) {
    // Assumes meaningful support for SEEK_END
    fseek(fd, 0, SEEK_END);
    uint32_t size = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    return size;
}

FILE *Block_Get_File(uint32_t block_ID, const char *dir, const char *mode) {
    size_t dir_len = strlen(dir);
    char path[12+dir_len];
    Block_To_Path(block_ID, path, 12+dir_len, dir);
    FILE *fd = fopen(path, mode);
    if (fd == NULL) {
        fprintf(stderr, "Block_Get_File:: Could not open file \"%s\".\n", path);
        return NULL;
    }
    return fd;
}

bool Block_Read_From_File(Block *block, FILE *fd) {
    uint32_t size = Block_Get_Size_From_File(fd);
    if (fread(block, size, 1, fd) != 1) { 
        fprintf(stderr, "Block_Read_From_File:: Did not read full file.\n");
        return false;
    }
    return true;
}
