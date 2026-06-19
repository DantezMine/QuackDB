#include "Block.h"
#include <complex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WRITE_W(block, offs, val) ( *(uint32_t*)(((uint8_t*)block)+offs) = val )
#define WRITE_B(block, offs, val) ( ((uint8_t*)block)[offs] = val )
#define TO_W(val) ( *((uint32_t*)&val) )

#define ADDR(block, offs) ( ((uint8_t*)block)+offs )

// Returns offset to start of schema table
#define BLOCK_SCHEMA_TABLE(block)   ( *(((uint8_t*)block)+16) )
#define BLOCK_ROW_TABLE(block)      ( *(((uint8_t*)block)+20) )
#define BLOCK_SIZE(block)   ( *(uint32_t*)block )
#define BLOCK_USED(block)   ( *(uint32_t*)((uint8_t*)block+12) )

// First entry in the block, acquired through footer at fixed end of block.
#define ENTRY_FIRST(block) BLOCK_SIZE(block)-4-(*(uint32_t*)ADDR(block, BLOCK_SIZE(block)-4) & 0xfffffffe)

#define ENTRY_FREE(entry) ( *(uint32_t*)(((uint8_t*)entry)-4) & 0x1)
#define ENTRY_SIZE(entry) ( *(uint32_t*)(((uint8_t*)entry)-4) & 0xfffffffe)

#define ENTRY_NEXT_FREE(entry) ( *(uint32_t*)(((uint8_t*)entry)-8) & 0x1)
#define ENTRY_NEXT_SIZE(entry) ( *(uint32_t*)(((uint8_t*)entry)-8) & 0xfffffffe)

#define ENTRY_PREV_FREE(entry) ( *(uint32_t*)(((uint8_t*)entry)+ENTRY_SIZE(entry)+4) & 0x1)
#define ENTRY_PREV_SIZE(entry) ( *(uint32_t*)(((uint8_t*)entry)+ENTRY_SIZE(entry)+4) & 0xfffffffe)

#define TABLE_ROW_INDX(block)       ( *(uint32_t*)ADDR(block,BLOCK_ROW_TABLE(block)-4) ) 
#define TABLE_ROW_GET(block, indx)  ( *(uint32_t*)ADDR(block,BLOCK_ROW_TABLE(block)+4*indx) )

#define SPLIT_MARGIN 24


static int Find_Free_Entry(void *block, uint32_t start, int entry_size);
static int Allocate_Entry(void *block, uint32_t entry, uint32_t target_size);
static void Deallocate_Entry(void *block, uint32_t entry);

void* Block_Create(uint32_t blk_size, float PCTFREE, float PCTUSED,
                    struct Block_Schema schema) {
    blk_size &= ~0x1; // force two-byte alignment
    void *block = malloc(blk_size);


    WRITE_W(block, 0, blk_size);
    WRITE_W(block, 4, TO_W(PCTFREE));
    WRITE_W(block, 8, TO_W(PCTUSED));
    WRITE_W(block, 12, 0); // Used bytes

    // create directories table
    uint32_t table_dir    = 16;
    uint32_t table_schema = table_dir+12;

    // write schema into directory
    // table_schema: holds first byte after schema table
    WRITE_W(block, table_schema-4, (uint32_t)schema.attr_count);
    for (int i=0; i<schema.attr_count; i++) {
        int len = strlen(schema.attr_names[i]);
        
        if (len > MAX_ATTRIBUTE_NAME) {
            fprintf(stderr, "Block_Create:: Attribute name exceeded %d characters.\n",
                    MAX_ATTRIBUTE_NAME);
            return NULL;
        }

        int var = 4*!Is_Attr_Width_Fixed(schema.attr_types[i]);

        WRITE_B(   block, table_schema       , (uint8_t)schema.attr_types[i]);
        WRITE_B(   block, table_schema+1+var , (uint8_t)len);
        strcpy((char*)ADDR(block, table_schema+2+var), schema.attr_names[i]);

        if (var) {
            WRITE_W(block, table_schema+1, (uint32_t)schema.var_sizes[i]);
        }

        table_schema += len+3+var; // includes null-character
    }

    uint32_t table_row = table_schema + 4;

    // write start of directories into directory table
    WRITE_W(block, table_dir  , table_dir+12);
    WRITE_W(block, table_dir+4, table_row);

    // Track row directory index
    WRITE_W(block, table_row-4, (uint32_t)0);


    // Create large free entry
    uint32_t footer = blk_size-4;
    uint32_t header = BLOCK_ROW_TABLE(block) + 4*TABLE_ROW_INDX(block);
    uint32_t free_size = footer-header-4;
    if (free_size % 2 == 1) {
        fprintf(stderr, "Block_Create:: Misalignment in size, should be two-byte aligned\n");
    }
    WRITE_W(block, header, free_size | 0x1);
    WRITE_W(block, footer, free_size | 0x1);

    return block;
}

int Is_Attr_Width_Fixed(enum Block_Attribute_t attr_type) {
    switch (attr_type) {
        case BLK_A_VARCHAR:
            return 0;
        default:
            return 1;
    }
}

struct Block_Schema Block_Get_Schema(void *block) {
    int table_schema = BLOCK_SCHEMA_TABLE(block);
    int attr_count = *(uint8_t*)ADDR(block, table_schema-4);
    void *schema_arena = malloc(attr_count*16);
    struct Block_Schema schema = {
        .arena = schema_arena,
        .attr_count = attr_count,
        .attr_types = schema_arena,
        .attr_names = (void*)((char*)schema_arena+attr_count*4),
        .var_sizes = (void*)((char*)schema_arena+attr_count*8),
    };

    int pos = table_schema;
    for (int i=0; i<attr_count; i++) {
        schema.attr_types[i] = *(uint8_t*)ADDR(block, pos);

        int var = 4*!Is_Attr_Width_Fixed(schema.attr_types[i]);
        if (var) schema.var_sizes[i] = *(uint32_t*)ADDR(block,pos+1);

        int len = *(uint8_t*)ADDR(block, pos+1+var);
        schema.attr_names[i] = (char*)ADDR(block, pos+2+var);
        pos += len+3+var;
    }

    return schema;
}


int Block_Insert_Entry(void *block, struct Block_Field *entry_values) {
    struct Block_Schema schema = Block_Get_Schema(block);
    int table_row = BLOCK_ROW_TABLE(block);
    
    int row_indx = *(uint32_t*)ADDR(block, table_row-4);

    // adjust last entry to make space for Row table
    uint32_t last_header = BLOCK_ROW_TABLE(block) + 4*TABLE_ROW_INDX(block);
    uint32_t free_size = ENTRY_SIZE(ADDR(block, last_header+4));

    WRITE_W(block, last_header+4,           free_size-4 | 0x1);
    WRITE_W(block, last_header+free_size+4, free_size-4 | 0x1);
    WRITE_W(block, BLOCK_ROW_TABLE(block)-4, row_indx+1);
    
    // compute entry size
    int entry_offs = 0;
    for (int i=0; i<schema.attr_count; i++) {
        enum Block_Attribute_t attr_type = schema.attr_types[i];
        int fixed = Is_Attr_Width_Fixed(attr_type);
        int field_size =  fixed ? Block_Attribute_t_Width[attr_type] : entry_values[i].attr_width;
        entry_offs += fixed ? field_size : field_size + 4;
    }

    int entry_size = entry_offs & 0x1 ? entry_offs+1 : entry_offs; // align to even bytes
    
    // find and allocate entry in block
    int free_pos = Find_Free_Entry(block, ENTRY_FIRST(block), entry_size);
    int entry_pos = Allocate_Entry(block, free_pos, entry_size);
    
    // Failed to allocate enough space, return failure.
    if (entry_pos == -1) {
        WRITE_W(block, last_header+4,           free_size | 0x1);
        WRITE_W(block, last_header+free_size+4, free_size | 0x1);
        WRITE_W(block, BLOCK_ROW_TABLE(block)-4, row_indx);
        return -1;
    }

    // write entry into Row table
    WRITE_W(block, last_header, entry_pos);

    // write values into entry
    entry_offs = entry_pos;
    for (int i=0; i<schema.attr_count; i++) {
        enum Block_Attribute_t attr_type = schema.attr_types[i];
        int fixed = Is_Attr_Width_Fixed(attr_type);
        int field_size =  fixed ? Block_Attribute_t_Width[attr_type] : entry_values[i].attr_width;
        
        if (!fixed) {
            WRITE_W(block, entry_offs, entry_values[i].attr_width);
            entry_offs += 4;
        }
        memcpy(ADDR(block, entry_offs), entry_values[i].value, field_size);
        entry_offs += field_size;
    }


    return row_indx;
}

/*
 * Searches from bottom of block upwards. This is very cache inefficient,
 * but basically required for a bottom-up block approach. Alternatively, would
 * create Row table of fixed size and start from top.
*/
static int Find_Free_Entry(void *block, uint32_t start, int entry_size) {
    uint32_t curr_pos  = start;
    uint32_t curr_size = ENTRY_SIZE(ADDR(block, curr_pos));

    while(curr_size < (uint32_t)entry_size+8 || !ENTRY_FREE(ADDR(block, curr_pos))) {
        curr_pos -= ENTRY_NEXT_SIZE(ADDR(block, curr_pos)) + 8;
        curr_size = ENTRY_SIZE(ADDR(block, curr_pos));
    }
    
    return curr_pos;
}


static int Allocate_Entry(void* block, uint32_t entry, uint32_t target_size) {
    uint32_t free_size = ENTRY_SIZE(ADDR(block, entry));
    uint32_t free_pos = entry;
    uint32_t entry_pos;
    uint32_t entry_size;
    if (free_size > target_size+8+SPLIT_MARGIN) {
        uint32_t split_size = free_size-target_size-8;
        entry_pos = free_pos + split_size + 8;
        entry_size = target_size;
        WRITE_W(block, free_pos-4,  split_size | 0x1);
        WRITE_W(block, entry_pos-8, split_size | 0x1);
    } else {
        uint32_t last_entry = BLOCK_ROW_TABLE(block) + 4*TABLE_ROW_INDX(block) + 4; 
        // Block is already full.
        if (free_pos == last_entry) {
            return -1;
        }
        entry_pos = free_pos;
        entry_size = free_size;
    }
    WRITE_W(block, 12, BLOCK_USED(block)+entry_size+8);
    WRITE_W(block, entry_pos-4,          entry_size & ~0x1);
    WRITE_W(block, entry_pos+entry_size, entry_size & ~0x1);
    return entry_pos;
}


static void Deallocate_Entry(void *block, uint32_t entry) {
    uint8_t *entry_ptr = ADDR(block, entry);
    uint32_t entry_size = ENTRY_SIZE(entry_ptr);
    WRITE_W(block, entry,            entry_size | 0x1);
    WRITE_W(block, entry+entry_size, entry_size | 0x1);

    // Recall: Next is up the block
    if (ENTRY_NEXT_FREE(entry_ptr)) {
        uint32_t free_size = ENTRY_NEXT_SIZE(entry_ptr);
        uint32_t free_pos = entry-free_size-8;
        uint32_t free_size_new = free_size+entry_size+8;
        WRITE_W(block, free_pos-4, free_size_new | 0x1);
        WRITE_W(block, free_pos+free_size_new, free_size_new | 0x1);
        entry = free_pos;
        entry_size = free_size_new;
        entry_ptr = ADDR(block, entry);
    }
    if (ENTRY_PREV_FREE(entry_ptr)) {
        uint32_t free_size = ENTRY_PREV_SIZE(entry_ptr);
        uint32_t free_size_new = free_size+entry_size+8;
        WRITE_W(block, entry-4, free_size_new | 0x1);
        WRITE_W(block, entry+free_size_new, free_size_new | 0x1);
        entry_size = free_size_new;
        entry_ptr = ADDR(block, entry);
   }
    return;
}


struct Block_Field* Block_Get(void *block, int row_index) {
    uint32_t entry_pos = TABLE_ROW_GET(block, row_index);
    struct Block_Schema schema = Block_Get_Schema(block);

    int arena_size = 0;
    int entry_offs = entry_pos;
    for (int i=0; i<schema.attr_count; i++) {
        enum Block_Attribute_t attr_type = schema.attr_types[i];
        int fixed = Is_Attr_Width_Fixed(attr_type);
        int field_size =  fixed ? Block_Attribute_t_Width[attr_type] : *(uint32_t*)ADDR(block, entry_offs);
        
        if (!fixed) {
            entry_offs += 4;
        }
        entry_offs += field_size;
        arena_size += field_size;
    }

    // Allocate space for return struct and the attribute values in one go
    struct Block_Field *entry = malloc(sizeof(struct Block_Field)*schema.attr_count+arena_size);
    void *arena = (uint8_t*)entry+sizeof(struct Block_Field)*schema.attr_count;

    entry_offs = entry_pos;
    for (int i=0; i<schema.attr_count; i++) {
        enum Block_Attribute_t attr_type = schema.attr_types[i];
        entry[i].attr_type = attr_type;
        int fixed = Is_Attr_Width_Fixed(attr_type);
        int field_size =  fixed ? Block_Attribute_t_Width[attr_type] : *(uint32_t*)ADDR(block, entry_offs);
        
        if (!fixed) {
            entry[i].attr_width = field_size;
            entry_offs += 4;
        }
        entry[i].value = arena;
        memcpy(arena, ADDR(block, entry_offs), field_size);
        arena      += field_size;
        entry_offs += field_size;
    }
    return entry;
}


void Block_Delete_Entry(void *block, int row_index) {
    uint32_t entry_pos = TABLE_ROW_GET(block, row_index);
    Deallocate_Entry(block, entry_pos);
}

// TODO
void Block_Remove(void *block) {
    free(block);
}
