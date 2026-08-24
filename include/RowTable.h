#ifndef H_ROWTABLE_H
#define H_ROWTABLE_H

#include "Block.h"
#include <stdint.h>

typedef struct {
    uint32_t size;
    uint32_t entries[];
} Row_Table;

uint32_t Row_Table_New_Entry(Block *block, Row_Table *row_table, uint32_t entry_size);

uint32_t Row_Table_Find_Free_Entry(Block *block, Row_Table *row_table, uint32_t entry_size);

#endif
