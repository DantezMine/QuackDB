#ifndef H_SLOTTEDPAGE
#define H_SLOTTEDPAGE

#include "Block.h"
#include <stdint.h>

typedef struct {
    Block_Header block_header;
    float PCTFREE;
    float PCTUSED;
    uint32_t free;

} Slotted_Page_Header;

typedef struct {
    Slotted_Page_Header header;
    uint32_t data[];
} Slotted_Page;

typedef union {
    struct {
        uint32_t block_ID;
        uint32_t row_index;
    };
    uint64_t rid;
} RID;

typedef struct {
    union {
        struct {
            const void *entry;
            const uint32_t size;
        } Entry;
        RID reference;
    };
    bool indirect;
} Page_Entry;


void Slotted_Page_Initialize(Slotted_Page *page, uint32_t size, uint32_t block_id, float PCTFREE, float PCTUSED);

/*
 * Attempts to insert an entry into the page. It
 * is up to the caller to ensure that the page
 * can fit the entry. In case the entry cannot 
 * be inserted, an index of 0 is returned.
*/
RID Slotted_Page_Insert_Entry(Slotted_Page *page, const void *entry, 
        uint32_t entry_size);

Page_Entry Slotted_Page_Get_Entry(Slotted_Page *page, RID rid);

/*
 * Attempts to update an entry in the page. In 
 * case the page cannot fit the entry, false is 
 * returned. The caller should then attempt to 
 * update and provide a page which can accept 
 * the entry using the corresponding method.
*/
bool Slotted_Page_Update_Entry(Slotted_Page *page, uint32_t indx, 
        const void *data, uint32_t data_size);

bool Slotted_Page_Update_Entry_Next(Slotted_Page *page, uint32_t indx, 
        const void *data, uint32_t data_size, Slotted_Page *page_next);

void Slotted_Page_Remove_Entry(Slotted_Page *page, uint32_t indx);

/*
 * This function does not guarantee that an insert can 
 * fit and only checks if the total free space is 
 * sufficient. Depending on fragmentation and worst-case 
 * defragmentation scenarios, it may still not fit.
*/
bool Slotted_Page_Can_Fit_Insert(Slotted_Page *page, uint32_t entry_size);

/*
 * This function takes into account
 * whether the update can fit in the original 
 * space or if the page can fit another insert.
*/
bool Slotted_Page_Can_Fit_Update(Slotted_Page *page, uint32_t indx, 
        uint32_t entry_size);

/*
 * Given the slotted page to defrag and a block of 
 * the same size, the entries will be copied into 
 * the new block in a packed order. The RIDs are 
 * preserved. All header data is preserved.
*/ 
void Slotted_Page_Defragment(Slotted_Page *page);

bool Slotted_Page_Consistency_Check(Slotted_Page *page, FILE *stream);

#endif
