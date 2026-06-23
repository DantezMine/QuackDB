#ifndef H_MMANAGER
#define H_MMANAGER

#include "Block.h"
#include <stdint.h>

#define DIR_DEFAULT_LENGTH 16

#define SCAN_READ_ONLY 0x01
#define SCAN_MEMORY_FIRST 0x02

struct Block_Slot {
    Block *block;
    uint32_t block_id;
    uint8_t in_memory;
};

struct Directory {
    char *identifier;
    struct Block_Slot *insert_list;
    struct Block_Slot *update_list;
    uint32_t insert_next;
    uint32_t insert_size;
    uint32_t update_next;
    uint32_t update_size;
};

struct Buffer_Pool {
    struct Block * pool;
    void * evictor;
    uint32_t pool_size;
    uint64_t free_size;
};

struct Buffer_Pool_Header {
    uint32_t size;
    uint32_t count;
    struct Block_Slot *slot;
    Block block;
};

typedef struct {
    struct Buffer_Pool bpool;
    struct Directory *dirs;
    uint32_t dirs_next;
    uint32_t dirs_size;
    uint32_t id_count;
} MManager;

typedef struct {
    struct Block_Slot **sequence;
    uint32_t current;
    uint32_t num;
    uint8_t options;
} Scanner;

MManager * MManager_Create(uint64_t available_memory);

void MManager_New_Directory(MManager *manager, char *identifier, struct Block_Schema schema,
        uint32_t block_size, float PCTFREE, float PCTUSED);

void MManager_Free(MManager *manager);

/*
 * Insert a list of entries. Input is list of pointers to
 * entries. An entry is an array of Block_Fields.
*/
void MManager_Insert_Entries(MManager *manager, char *identifier, struct Block_Field **entries, int num_entries);

struct Directory * MManager_Get_Directory(MManager *manager, char *identifier);

Scanner MManager_Scanner(MManager *manager, struct Directory *dir, uint8_t options);

struct Block_Slot * MManager_Scanner_Next(MManager *manager, Scanner *scanner);

void MManager_Scanner_Free(Scanner *scanner);

struct Block_Slot * MManager_Scanner_New(MManager *manager, Scanner *scanner, struct Block_Schema* schema, struct Directory *dir);

#endif
