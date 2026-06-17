#ifndef H_BLOCK
#define H_BLOCK

#include <stdint.h>

#define MAX_ATTRIBUTE_NAME 100

enum Block_t {
    BLK_T_TABLE,
};

enum Block_Attribute_t {
    BLK_A_INT,
    BLK_A_VARCHAR,
};

struct Block_Field {
    enum Block_Attribute_t attr_type;
    uint32_t attr_width;
    void *value;
};

struct Block_Schema {
    int attr_count;
    enum Block_Attribute_t *attr_types;
    int *var_sizes; // ignored for fixed-width attributes.
    char **attr_names;
};


static const char* Block_Attribute_t_Name[] = {
    "INT",
    "VARCHAR",
};

static const int Block_Attribute_t_Width[] = {
    4,
    -1,
};

void* Block_Create(uint32_t blk_size, float PCTFREE, float PCTUSED,
                    struct Block_Schema);

/* Inserts the given entry into the block pointed to
 * with the address and returns the offset (not a pointer) used
 * for RID, along with the Block ID.
 * Expects the entry to contain the same number of attributes
 * as declared in block creation.
*/
int Block_Insert(void *block, struct Block_Field *entry_values);

/*
 * Write the block to storage (file),
 * free from memory and return its name.
*/
char* Block_Write(void *block);

void Block_Free(void *block);

void* Block_Read(char *blk_name);

int Is_Attr_Width_Fixed(enum Block_Attribute_t attr_type);

struct Block_Schema Block_Get_Schema(void *block);

#endif
