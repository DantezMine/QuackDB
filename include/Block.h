#ifndef H_BLOCK
#define H_BLOCK

#include <stdint.h>

#define MAX_ATTRIBUTE_NAME 100

typedef struct Block{
    uint32_t block_size;
    uint32_t block_id;
    uint32_t PCTFREE;
    uint32_t PCTUSED;
    uint32_t used;
    uint32_t table_schema;
    uint32_t table_row;
    uint8_t data[];
} Block;

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

Block* Block_Create(Block *block, uint32_t block_size, uint32_t block_id, float PCTFREE, float PCTUSED,
                    struct Block_Schema);

/* Inserts the given entry into the block pointed to
 * with the address and returns the offset into the Row table.
 * Expects the entry to contain the same number of attributes
 * as declared in block creation.
*/
int Block_Insert_Entry(Block *block, struct Block_Field *entry_values, struct Block_Schema *schema);

/*
 * Return array of entries. Values are stored in the same memory 
 * region as the returned pointer. Must free the returned pointer
 * after use.
*/ 
struct Block_Field* Block_Get(Block *block, int row_index);

void Block_Delete_Entry(Block *block, int row_index);

/*
 * Not Implemented!
 * Write the block to storage (file),
 * free from memory.
*/
void Block_Write(Block *block);

/*
 * Not Implemented!
 * Read the block from storage (file)
 * into memory pointed to by block.
*/
void Block_Read(Block *block, uint32_t id, uint32_t size);

uint32_t Block_Read_Size(uint32_t id);

void Block_Remove(Block *block);

int Is_Attr_Width_Fixed(enum Block_Attribute_t attr_type);

/*
 * Must free the arena in the returned struct when done.
*/
struct Block_Schema Block_Get_Schema(Block *block);

#endif
