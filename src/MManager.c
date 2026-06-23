#include "MManager.h"
#include "Block.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BPOOL_HEADER(entry)    (struct Buffer_Pool_Header*)((uint32_t*)(entry)-4)

static void Move_To_Update_List(MManager *manager, struct Directory *dir, int slot_index);

static void New_Block(MManager *manager, struct Directory *dir, struct Block_Schema *schema);

static void Block_To_Buffer(MManager *manager, Block *block, struct Block_Slot *bslot);

static void Read_Block(MManager *manager, struct Block_Slot *bslot);

static void * BPool_Allocate(MManager *manager, uint32_t size);

static void BPool_Evict(MManager *manager);

MManager* MManager_Create(uint64_t available_memory) {
    MManager *manager = malloc(sizeof(MManager));
    manager->dirs = malloc(sizeof(struct Directory)*DIR_DEFAULT_LENGTH);
    manager->dirs_next = 0;
    manager->dirs_size = DIR_DEFAULT_LENGTH;
    manager->bpool.pool = malloc(available_memory);
    manager->bpool.pool_size = available_memory;
    manager->bpool.free_size = available_memory-8;
    manager->bpool.evictor = manager->bpool.pool;
    *(uint32_t*)manager->bpool.pool = available_memory-8 | 0x1;
    *(((uint32_t*)manager->bpool.pool)+1) = 0;
    return manager;
}

void MManager_New_Directory(MManager *manager, char *identifier, struct Block_Schema schema,
        uint32_t block_size, float PCTFREE, float PCTUSED) {
    block_size &= ~0x3; // force four-byte alignment
    if (manager->dirs_next == manager->dirs_size) {
        manager->dirs_size *= 2;
        manager->dirs = realloc(manager->dirs, manager->dirs_size);
    }
    int dir_indx = manager->dirs_next++;
    manager->dirs[dir_indx] = (struct Directory) {
        .insert_size = DIR_DEFAULT_LENGTH,
        .update_size = DIR_DEFAULT_LENGTH,
        .insert_next = 0,
        .update_next = 0,
    };
    struct Directory *dir = &manager->dirs[dir_indx];
    dir->insert_list = malloc(sizeof(struct Block_Slot)*DIR_DEFAULT_LENGTH);
    dir->update_list = malloc(sizeof(struct Block_Slot)*DIR_DEFAULT_LENGTH);
    dir->identifier = malloc(strlen(identifier));
    strcpy(dir->identifier, identifier);

    uint32_t block_id = manager->id_count++;
    Block *block_initial = BPool_Allocate(manager, block_size);
    Block_Create(block_initial, block_size, block_id, PCTFREE, PCTUSED, schema);
    struct Block_Slot bslot = (struct Block_Slot) {
        .block_id = block_id,
        .block = block_initial,
        .in_memory = 1,
    };
    dir->insert_list[dir->insert_next++] = bslot;
    Block_To_Buffer(manager, block_initial, &dir->insert_list[dir->insert_next-1]);
}

void MManager_Free(MManager *manager) {
    // Semi-Recursively frees all associated memory down to each block
    for (int i=0; i<(int)manager->dirs_next; i++) {
        for (int j=0; j<(int)manager->dirs[i].insert_next; j++) {
            free(manager->dirs[i].insert_list[j].block);
        }
        for (int j=0; j<(int)manager->dirs[i].update_next; j++) {
            free(manager->dirs[i].update_list[j].block);
        }
        free(manager->dirs[i].identifier);
        free(manager->dirs[i].insert_list);
        free(manager->dirs[i].update_list);
    }
    free(manager->dirs);
    free(manager->bpool.pool);
    free(manager);
}


struct Directory * MManager_Get_Directory(MManager *manager, char *identifier) {
    for (int i=0; i<(int)manager->dirs_next; i++) {
        if (strcmp(identifier, manager->dirs[i].identifier) == 0)
            return &manager->dirs[i];
    }
    return NULL;
}

void MManager_Insert_Entries(MManager *manager, char *identifier, struct Block_Field **entries, int num_entries) {
    struct Directory *dir = MManager_Get_Directory(manager, identifier);
    struct Block_Schema schema;
    if (dir->insert_next > 0) {
        schema = Block_Get_Schema(dir->insert_list[0].block);
    } else if (dir->update_next > 0) {
        schema = Block_Get_Schema(dir->update_list[0].block);
    } else {
        fprintf(stderr, "MManager_Insert_Entries:: Directory \"%s\" does not contain a schema\n",identifier);
    }

    Scanner scanner = MManager_Scanner(manager, dir, SCAN_MEMORY_FIRST);
    struct Block_Slot *slot = MManager_Scanner_Next(manager, &scanner);
    for (int i=0; i<num_entries; i++) {
        while (1) {
            if (slot == NULL) {
                slot = MManager_Scanner_New(manager, &scanner, &schema, dir);
            }
            if (!slot->in_memory) Read_Block(manager, slot);
            int rid = Block_Insert_Entry(slot->block, entries[i], &schema);
            if (rid >= 0) {
                uint8_t full = (slot->block->block_size-slot->block->used) < slot->block->block_size * (float)slot->block->PCTFREE;
                if (full) {
                    uint32_t slot_index = 0;
                    for (; slot_index<dir->insert_next; slot_index++) {
                        if (dir->insert_list[slot_index].block_id == slot->block_id) 
                            break;
                    }
                    Move_To_Update_List(manager, dir, slot_index);
                    slot = MManager_Scanner_Next(manager, &scanner);
                }
                break;
            }
            slot = MManager_Scanner_Next(manager, &scanner);
        }
    }
}


static void New_Block(MManager *manager, struct Directory *dir, struct Block_Schema *schema) {
    Block *reference_block;
    if (dir->insert_next > 0) {
        reference_block = dir->insert_list[0].block;
    } else if (dir->update_next > 0) {
        reference_block = dir->update_list[0].block;
    } else {
        fprintf(stderr, "MManager_Insert_Entries:: Directory \"%s\" does not contain a schema\n",dir->identifier);
        return;
    }
    Block *block = BPool_Allocate(manager, reference_block->block_size);
    Block_Create(block, reference_block->block_size, manager->id_count++, reference_block->PCTFREE, reference_block->PCTUSED, *schema);
    if (dir->insert_next == dir->insert_size) {
        dir->insert_size *= 2;
        dir->insert_list = realloc(dir->insert_list, dir->insert_size);
    }
    struct Block_Slot bslot = (struct Block_Slot) {
        .block = block,
        .block_id = block->block_id,
        .in_memory=1
    };
    dir->insert_list[dir->insert_next++] = bslot;
    Block_To_Buffer(manager, block, &dir->insert_list[dir->insert_next-1]);
    return;
}


static void Move_To_Update_List(MManager *manager, struct Directory *dir, int slot_index) {
    if (dir->update_next == dir->update_size) {
        dir->update_size *= 2;
        dir->update_list = realloc(dir->update_list, dir->update_size);
    }
    dir->update_list[dir->update_next] = dir->insert_list[slot_index];
    if ((uint32_t)slot_index < dir->insert_next-1) {
        // Copy the last slot of index list to fill the empty slot
        dir->insert_list[slot_index] = dir->insert_list[--dir->insert_next];
    }
}

// Simple implementation
Scanner MManager_Scanner(MManager *manager, struct Directory *dir, uint8_t options) {
    int num_blocks = dir->insert_next;
    num_blocks += options & SCAN_READ_ONLY ? dir->update_next : 0;
    Scanner scanner;
    scanner.options = options;
    scanner.current = 0;
    scanner.sequence = malloc(num_blocks * sizeof(struct Block_Slot));

    uint32_t indx = 0;
    if (options & SCAN_MEMORY_FIRST) { 
        for (uint32_t i=0; i<dir->update_next; i++) {
            if (dir->insert_list[i].in_memory)
                scanner.sequence[indx++] = &dir->update_list[i];
        }
        if (options & SCAN_READ_ONLY) {
            for (uint32_t i=0; i<dir->insert_next; i++) {
                if (dir->update_list[i].in_memory)
                    scanner.sequence[indx++] = &dir->insert_list[i];
            }
        }
        for (uint32_t i=0; i<dir->update_next; i++) {
            if (!dir->insert_list[i].in_memory)
                scanner.sequence[indx++] = &dir->update_list[i];
        }
        if (options & SCAN_READ_ONLY) {
            for (uint32_t i=0; i<dir->insert_next; i++) {
                if (!dir->update_list[i].in_memory)
                    scanner.sequence[indx++] = &dir->insert_list[i];
            }
        }
    } else {
        for (uint32_t i=0; i<dir->update_next; i++) {
            scanner.sequence[indx++] = &dir->update_list[i];
        }
        if (options & SCAN_READ_ONLY) {
            for (uint32_t i=0; i<dir->insert_next; i++) {
                scanner.sequence[indx++] = &dir->insert_list[i];
            }
        }
    }
    scanner.num = indx;
    return scanner;
}


/*
 * Block pointer already points into buffer and is used to
 * find the correct entry.
*/
static void Block_To_Buffer(MManager *manager, Block *block, struct Block_Slot *bslot) {
    struct Buffer_Pool_Header *header = BPOOL_HEADER(block);
    header->size = block->block_size & ~0x3; // ensure not dirty, not free
    header->slot = bslot;
    header->count = 1;
}


static void * BPool_Allocate(MManager *manager, uint32_t size) {
    uint8_t *evictor = manager->bpool.evictor;
    uint32_t curr_size = 0;
    uint8_t *curr_start = NULL;
    struct Buffer_Pool_Header *header = BPOOL_HEADER(evictor);

    // First pass: Find contiguous space
    while (curr_size < size) {
        if (!(header->size & 0x1) && header->count > 0) {
            curr_size = 0;
            curr_start = NULL;
            header->count -= 1;
        } else if (curr_start) {
            curr_size  = header->size & ~0x3;
            curr_start = evictor;
        } else {
            curr_size += header->size & ~0x3 + 16;
        }
        evictor += 16 + header->size & ~0x3;
        header = BPOOL_HEADER(evictor);
    }

    // Second pass: Evict all blocks in found space
    curr_size = 0;
    evictor = curr_start;
    while (curr_size < size) {
        if (!(header->size & 0x1)) BPool_Evict(manager);
        curr_size += header->size & ~0x3 + 16;
        evictor += 16 + header->size & ~0x3;
        header = BPOOL_HEADER(evictor);
    }

    // Not bothering to coalesce. The algorithm above combines free chunks anyway
    struct Buffer_Pool_Header *curr_header = BPOOL_HEADER(curr_start);
    struct Buffer_Pool_Header *free_header = BPOOL_HEADER(((uint8_t*)curr_start)+size);
    curr_header->size = size;
    free_header->size = curr_size-16-size | 0x1;
    
    return curr_start;
}

static void BPool_Evict(MManager *manager) {
    struct Buffer_Pool_Header *header = BPOOL_HEADER(manager->bpool.evictor);
    if (header->size & 0x2 && !(header->size & 0x1)) {
        Block_Write(&header->block);
        header->size |= 0x1;
    } else {
        header->size |= 0x1;
    }
    return;
}

void MManager_Scanner_Free(Scanner *scanner) {
    free(scanner);
    return;
}


struct Block_Slot * MManager_Scanner_Next(MManager *manager, Scanner *scanner) {
    if (scanner->current == scanner->num) {
        return NULL;
    }
    struct Block_Slot *bslot = scanner->sequence[scanner->current++];
    if (scanner->sequence[scanner->current]->in_memory) {
        return bslot;
    } else {
        Read_Block(manager, bslot);
        return bslot;
    }
}


struct Block_Slot * MManager_Scanner_New(MManager *manager, Scanner *scanner, struct Block_Schema* schema, struct Directory *dir) {
    New_Block(manager, dir, schema);
    struct Block_Slot *slot = &dir->insert_list[dir->insert_next-1];
    scanner->num++;
    scanner->sequence = realloc(scanner->sequence, scanner->num * sizeof(scanner->sequence[0]));
    scanner->sequence[scanner->current++] = slot;
    return slot;
}


static void Read_Block(MManager *manager, struct Block_Slot *bslot) {
    uint32_t block_size = Block_Read_Size(bslot->block_id);
    bslot->block = BPool_Allocate(manager, block_size);
    bslot->in_memory = 1;
    Block_Read(bslot->block, bslot->block_id, block_size);
    return;
}
