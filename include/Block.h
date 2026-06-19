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
    void *arena;
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
int Block_Insert_Entry(void *block, struct Block_Field *entry_values);

/*
 * Return array of entries. Values are stored in the same memory 
 * region as the returned pointer. Must free the returned pointer
 * after use.
*/ 
struct Block_Field* Block_Get(void *block, int row_index);

void Block_Delete_Entry(void *block, int row_index);

/*
 * Write the block to storage (file),
 * free from memory.
*/
void Block_Write(void *block);

/*
 * Read the block from storage (file)
 * into memory.
*/
void* Block_Read(char *blk_name);

void Block_Remove(void *block);

int Is_Attr_Width_Fixed(enum Block_Attribute_t attr_type);

/*
 * Must free the arena in the returned struct when done.
*/
struct Block_Schema Block_Get_Schema(void *block);

#endif
