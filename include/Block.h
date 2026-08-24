#ifndef H_BLOCK
#define H_BLOCK

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define MAX_ATTRIBUTE_NAME 100

typedef struct Block{
    uint32_t block_size;
    uint32_t block_ID;
    uint8_t dirty_flag;
    uint8_t unused[3];
    uint8_t data[];
} Block;


/*
 * Allocates memory for the block.
*/
void Block_Initialize(Block *block, uint32_t block_size, uint32_t block_id);

/*
 * Write the block to file.
 * Returns false in case of error.
*/
bool Block_Write_To_File(Block *block, FILE *fd);

/*
 * Read the block from file to block in memory.
 * Block pointer must have enough space.
 * Should only be called by the memory manager.
*/
bool Block_Read_From_File(Block *block, FILE *fd);

uint32_t Block_Get_Size_From_File(FILE *fd);

FILE *Block_Get_File(uint32_t block_ID, const char *dir, const char *mode);

uint32_t Block_Get_Size(Block *block);

uint32_t Block_Get_ID(Block *block);

bool Block_Is_Dirty(Block *block);

void *Block_Get_Data(Block *block);

/*
 * Deletes the block from persistent storage (if it exists).
*/
bool Block_Delete_File(uint32_t block_ID, const char *dir);

void Block_Free(Block *block);

#endif
