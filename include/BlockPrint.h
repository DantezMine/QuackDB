#ifndef H_BLOCKPRINT
#define H_BLOCKPRINT

#include "Block.h"
#include <stdio.h>

struct Head_Cell_Data{
    int width;
    char *attr_name;
    int attr_name_len;
    char *type_name;
    int type_name_len;
    int type_width;
    int type_width_len;
    int width_from_start;
};

struct Cell_Data{
    int width;
    char *text;
};
/*
 * The returned struct Cell_Data must be freed.
*/
struct Head_Cell_Data* Block_Print_Get_Header_Cells(struct Block_Schema *schema);

struct Row_Cell_Data* Block_Print_Get_Row_Cells(struct Block_Schema *schema);

void Block_Print_Head(FILE *file, void *block, struct Head_Cell_Data *head);

void Block_Print_Rows(FILE *file, void *block, struct Block_Field **rows, int num_rows);

#endif
