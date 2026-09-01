#include "SlottedPage.h"
#include <stdint.h>
#include <string.h>

#define ALIGN_SIZE(val) ( val + (val & 0x1) )

#define ENTRY_H_FREE(entry_header) ( *(uint32_t*)entry_header &  0x1  ) 
#define ENTRY_H_SIZE(entry_header) ( *(uint32_t*)entry_header & ~0x1 ) 

#define ENTRY_H_TO_F(entry_h, size) (  (uint32_t*)((uint8_t*)entry_h + size + 4) )

static RID Row_Table_New_Entry(Slotted_Page *page, 
        uint32_t data_size);

static uint32_t *Entry_List_Find_Free_Entry(Slotted_Page *page, 
        uint32_t entry_size);

static uint32_t Row_Table_Entry_Size(Slotted_Page *page, uint32_t indx);

static uint32_t *Row_Table_Entry(Slotted_Page *page, uint32_t indx);

static void Row_Table_Set(Slotted_Page *page, uint32_t indx, 
        uint32_t value);

static bool Row_Table_Increment(Slotted_Page *page);

static uint32_t Row_Table_Length(Slotted_Page *page);

static uint32_t *Entry_List_First_Entry_Header(Slotted_Page *page);

static uint32_t *Entry_List_Last_Entry_Footer(Slotted_Page *page);

static void Entry_List_Free(Slotted_Page *page, uint32_t *entry);

static uint32_t *Entry_List_Coalesce_Left(uint32_t *entry);

static bool Entry_List_Coalesce_Right(uint32_t *entry);

static bool Entry_List_Shrink_First_Entry(Slotted_Page *page);

static uint32_t *Entry_List_Split(Slotted_Page *page, uint32_t *entry, 
        uint32_t data_size);

static void Entry_List_Insert_Data(uint32_t *entry, void *data, 
        uint32_t data_size);


RID Slotted_Page_Insert_Entry(Slotted_Page *page, void *data, 
        uint32_t data_size) {
    if (!Slotted_Page_Can_Fit_Insert(page, data_size)) {
        return (RID) {0};
    }

    RID rid = Row_Table_New_Entry(page, data_size);
    // If the entry didn't fit, defragment and try again
    // This might still fail
    if (rid.block_ID == 0) {
        Slotted_Page_Defragment(page);
        rid = Row_Table_New_Entry(page, data_size);
    }

    // Entry might not fit into page at all
    if (rid.block_ID != 0) {
        uint32_t *entry = Row_Table_Entry(page, rid.row_index);
        Entry_List_Insert_Data(entry, data, data_size);
    }

    return rid;
}


void Slotted_Page_Remove_Entry(Slotted_Page *page, uint32_t indx) {
    uint32_t *entry = Row_Table_Entry(page, indx);
    Row_Table_Set(page, indx, -1);
    Entry_List_Free(page, entry);
    return;
}


bool Slotted_Page_Can_Fit_Insert(Slotted_Page *page, uint32_t entry_size) {
    return page->header.free >= ALIGN_SIZE(entry_size) + 12;
}


bool Slotted_Page_Can_Fit_Update(Slotted_Page *page, uint32_t indx, 
        uint32_t entry_size) {
    return (ALIGN_SIZE(entry_size) <= Row_Table_Entry_Size(page, indx) 
            || Slotted_Page_Can_Fit_Insert(page, entry_size));
}



void Slotted_Page_Defragment(Slotted_Page *page) {
    fprintf(stderr, "Slotted_Page_Defragment:: Not yet implemented!\n");
}


static RID Row_Table_New_Entry(Slotted_Page *page, uint32_t data_size) {
    uint32_t *entry_free = Entry_List_Find_Free_Entry(page, data_size);
    uint32_t index      = Row_Table_Length(page);

    bool new_slot_success = Row_Table_Increment(page);
    if (entry_free == NULL || !new_slot_success) {
        return (RID) {0};
    }
    if (entry_free == Entry_List_First_Entry_Header(page)) {
        entry_free += 1;
    }
    
    uint32_t *entry       = Entry_List_Split(page, entry_free, data_size);
    uint32_t entry_offset = (uint8_t*)entry - (uint8_t*)page;

    Row_Table_Set(page, index, entry_offset);
    return (RID) {
        .block_ID = Block_Get_ID((Block*)page), 
        .row_index = index};
}


static bool Row_Table_Increment(Slotted_Page *page) {
    if (!Entry_List_Shrink_First_Entry(page)) {
        return false;
    }
    page->data[0]++;
    return true;
}


static uint32_t *Entry_List_Split(Slotted_Page *page, uint32_t *entry, uint32_t data_size) {
    uint32_t *entry_h_free = entry - 1;
    uint32_t entry_size    = ENTRY_H_SIZE(entry_h_free);
    uint32_t aligned_size  = ALIGN_SIZE(data_size);
    bool first_entry = entry_h_free == Entry_List_First_Entry_Header(page);

    // This case should not happen if the caller operates nominally.
    if ((first_entry && aligned_size + 8 > entry_size) || entry_size < aligned_size) {
        return NULL;
    }

    uint32_t used_bytes;
    uint32_t *entry_out;
    
    if (first_entry || aligned_size + 8 <= entry_size) {
        uint32_t new_free_size = entry_size - aligned_size - 8;
        uint32_t *entry_f_free = ENTRY_H_TO_F(entry_h_free, new_free_size);
        uint32_t *entry_h      = entry_f_free + 1;
        uint32_t *entry_f      = ENTRY_H_TO_F(entry_h, aligned_size);

        *entry_h_free = new_free_size | 1;
        *entry_f_free = new_free_size | 1;
        *entry_h      = aligned_size;
        *entry_f      = aligned_size;

        used_bytes = aligned_size + 8;
        entry_out  = entry_h + 1;
    }
    else {
        uint32_t *entry_f_free = ENTRY_H_TO_F(entry_h_free, entry_size);
        *entry_h_free = entry_size;
        *entry_f_free = entry_size;

        used_bytes = entry_size;
        entry_out  = entry_h_free + 1;
    }

    page->header.free -= used_bytes;
    return entry_out;
}


static void Entry_List_Insert_Data(uint32_t *entry, void *data, uint32_t data_size) {
    memcpy(entry, data, data_size);
}


static bool Entry_List_Shrink_First_Entry(Slotted_Page *page) {
    uint32_t *entry_h_first = Entry_List_First_Entry_Header(page);
    uint32_t size = ENTRY_H_SIZE(entry_h_first);

    if (size == 0) {
        return false;
    }

    uint32_t new_size = size - 4;
    uint32_t *entry_h = entry_h_first + 1;
    uint32_t *entry_f = ENTRY_H_TO_F(entry_h, size - 4);

    *entry_h = new_size | 1;
    *entry_f = new_size | 1;

    page->header.free -= 4;
    return true;
}


/*
 * Searches the entire list for the last free spot with enough space.
 * Perfect fits are preferred over too large spaces to avoid
 * fragmentation.
 * This is always an O(N) search and inefficient.
 * Alternatively, could search backwards, but that will be very cache
 * unfriendly.
*/ 
static uint32_t *Entry_List_Find_Free_Entry(Slotted_Page *page, 
        uint32_t entry_size) {
    entry_size = ALIGN_SIZE(entry_size);
    uint8_t *entry_h_curr = (uint8_t*)Entry_List_First_Entry_Header(page);
    uint8_t *entry_f_last = (uint8_t*)Entry_List_Last_Entry_Footer(page);


    uint8_t *entry_h_fits = NULL;
    bool perfect_fit = false;


    // First entry needs to be free. If first entry can't be split 
    // into requested size and a free entry, disregard. 
    // New entries will increase the size of the row table, thus 
    // another four bytes are required.
    uint32_t size_curr = ENTRY_H_SIZE(entry_h_curr);
    if (size_curr >= entry_size + 12) {
        entry_h_fits = entry_h_curr;
    }

    entry_h_curr += size_curr + 8;
    

    while (entry_h_curr < entry_f_last) {
        uint32_t size   = ENTRY_H_SIZE(entry_h_curr);
        bool entry_free = ENTRY_H_FREE(entry_h_curr); 

        if (entry_free && size == entry_size) {
            perfect_fit = true;
            entry_h_fits = entry_h_curr;
        } else if (entry_free && !perfect_fit && size > entry_size) {
            entry_h_fits = entry_h_curr;
        }

        entry_h_curr += size + 8;
    }

    if (entry_h_fits == NULL) {
        return 0;
    }
    return (uint32_t*)(entry_h_fits + 4);
}



static void Entry_List_Free(Slotted_Page *page, uint32_t *entry) {
    uint32_t *entry_header = entry - 1;
    uint32_t *new_entry    = entry;
    uint32_t entry_size    = ENTRY_H_SIZE(entry_header);
    uint32_t *entry_footer = ENTRY_H_TO_F(entry_header, entry_size);
    *entry_header |= 0x1;
    *entry_footer |= 0x1;
    page->header.free += entry_size;

    if (entry_header != Entry_List_First_Entry_Header(page)) {
        new_entry = Entry_List_Coalesce_Left(entry);
        page->header.free += new_entry != entry ? 8 : 0;
    }

    if (entry_footer != Entry_List_Last_Entry_Footer(page)) {
        bool coalesced = Entry_List_Coalesce_Right(new_entry);
        page->header.free += coalesced ? 8 : 0;
    }
    return;
}

static uint32_t *Entry_List_Coalesce_Left(uint32_t *entry) {
    uint32_t *entry_header = entry - 1;
    uint32_t entry_size    =  ENTRY_H_SIZE(entry_header);
    uint32_t *entry_footer = (uint32_t*) ((uint8_t*)entry + entry_size);

    uint32_t *prev_footer = entry - 2;
    uint32_t prev_size    = ENTRY_H_SIZE(prev_footer);
    uint32_t *prev_header = (uint32_t*) ((uint8_t*)prev_footer - prev_size - 4);
    bool prev_free        = ENTRY_H_FREE(prev_footer);

    if (prev_free) {
        uint32_t new_size = prev_size + entry_size + 8;
        *prev_header  = new_size | 0x1;
        *entry_footer = new_size | 0x1;
        return prev_header + 1;
    }
    return entry;
}

static bool Entry_List_Coalesce_Right(uint32_t *entry) {
    uint32_t *entry_header = entry - 1;
    uint32_t entry_size    =  ENTRY_H_SIZE(entry_header);

    uint32_t *next_header = (uint32_t*) ((uint8_t*)entry + entry_size + 4);
    uint32_t next_size    = ENTRY_H_SIZE(next_header);
    uint32_t *next_footer = (uint32_t*) ((uint8_t*)next_header + next_size + 4);
    bool next_free        = ENTRY_H_FREE(next_header);

    if (next_free) {
        uint32_t new_size = next_size + entry_size + 8;
        *entry_header = new_size | 0x1;
        *next_footer  = new_size | 0x1;
    }
    return next_free;
}


static uint32_t *Entry_List_First_Entry_Header(Slotted_Page *page) {
    uint32_t *table_start_ptr = ((uint32_t*)page->data);
    uint32_t table_size = *table_start_ptr;
    return table_start_ptr + table_size + 1;
}


static uint32_t *Entry_List_Last_Entry_Footer(Slotted_Page *page) {
    return (uint32_t*)((uint8_t*)page + page->header.block_header.block_size - 4);
}


static void Row_Table_Set(Slotted_Page *page, uint32_t indx, uint32_t value) {
    uint32_t *table_start_ptr = ((uint32_t*)page->data);
    uint32_t table_size = *table_start_ptr;
    if (indx >= table_size) {
        return;
    }
    *(table_start_ptr+1+indx) = value;
    return;
}


static uint32_t Row_Table_Entry_Size(Slotted_Page *page, uint32_t indx) {
    uint32_t *entry = Row_Table_Entry(page, indx);
    if (entry == NULL) {
        return 0;
    }
    uint32_t *entry_header = entry - 1;
    uint32_t size = ENTRY_H_SIZE(entry_header);
    return size;
}


static uint32_t *Row_Table_Entry(Slotted_Page *page, uint32_t indx) {
    uint32_t *table_start_ptr = page->data;
    uint32_t table_size = *table_start_ptr;
    if (indx >= table_size) {
        return NULL;
    }
    uint32_t offset = *(table_start_ptr + indx + 1);
    uint32_t *entry = (uint32_t*)((uint8_t*)page + offset);
    return entry;
}


static uint32_t Row_Table_Length(Slotted_Page *page) {
    return page->data[0];
}





static bool Consistency_Check_Mismatched(Slotted_Page *page, uint8_t *entry_h_curr, FILE *stream) {
        uint32_t size = *(uint32_t*)entry_h_curr & ~0x1;
        uint8_t *entry_f_curr = entry_h_curr+size+4;
        if (*(uint32_t*)entry_h_curr != *(uint32_t*)entry_f_curr) {
            fprintf(stream, 
                "Slotted Page Consistency:: Bad entry in block %u.\n\tHeader at %u: %u\n\tFooter at %u: %u\n", 
                page->header.block_header.block_ID, 
                (unsigned int)(entry_h_curr-(uint8_t*)page), 
                *(uint32_t*)entry_h_curr,
                (unsigned int)(entry_f_curr-(uint8_t*)page), 
                *(uint32_t*)entry_f_curr);
            return false;
        }
        return true;
}

static bool Consistency_Check_Last_Entry(Slotted_Page *page, uint8_t *entry_h_curr, FILE *stream) {
    uint8_t *entry_f_last = (uint8_t*)page 
        + page->header.block_header.block_size - 4;

    bool last_entry = entry_f_last == entry_h_curr - 4
        && entry_f_last == (uint8_t*)Entry_List_Last_Entry_Footer(page);

    if (!last_entry) {
        fprintf(stream, 
            "Slotted Page Consistency: Bad entry list in block %u.\n\tExpected last footer offset: %u\n\tActual last footer offset  : %u\n",
            page->header.block_header.block_ID, 
            (unsigned int)(entry_f_last-(uint8_t*)page), 
            (unsigned int)(entry_h_curr-4-(uint8_t*)page));
        return false;
    }
    return true;
}

static bool Consistency_Check_Free_Amount(Slotted_Page *page, uint32_t free, FILE *stream) {
    bool free_amount = free == page->header.free;
    if (!free_amount) {
        fprintf(stream, 
            "Slotted Page Consistency: Bad entry list in block %u.\n\tIndicated free amount: %u\n\tActual free amount   : %u\n",
            page->header.block_header.block_ID, 
            page->header.free, 
            free);
        return false;
    }
    return true;
}

static bool Consistency_Check_First_Entry(Slotted_Page *page, uint8_t *entry_h_first, FILE *stream) {
    uint8_t *first = (uint8_t*)Entry_List_First_Entry_Header(page);
    if (first != entry_h_first) {
        fprintf(stream, 
            "Slotted Page Consistency: Bad entry list in block %u.\n\tExpected first header offset: %u\n\tActual first header offset  : %u\n",
            page->header.block_header.block_ID, 
            (unsigned int)(first-(uint8_t*)page), 
            (unsigned int)(entry_h_first-(uint8_t*)page));
        return false;
        return false;
    }
    return true;
}

// This function does not abide by the seperation 
// of abstractions which most of the others try 
// to follow. It's ok though.
bool Slotted_Page_Consistency_Check(Slotted_Page *page, FILE *stream) {
    uint32_t row_size = page->data[0];
    uint8_t *entry_h_first = (uint8_t*)&(page->data[row_size+1]);
    uint8_t *entry_h_curr = entry_h_first;
    
    if (!Consistency_Check_First_Entry(page, entry_h_first, stream)) {
        return false;
    }

    uint32_t free = page->header.block_header.block_size 
        - sizeof(Slotted_Page_Header)
        - (row_size + 1) * 4;

    uint8_t *entry_f_last = (uint8_t*)page 
        + page->header.block_header.block_size;

    while (entry_h_curr < entry_f_last) {
        if (!Consistency_Check_Mismatched(page, entry_h_curr, stream)) {
            return false;
        }

        uint32_t size   = *(uint32_t*)entry_h_curr & ~0x1;
        bool entry_free = *(uint32_t*)entry_h_curr & 0x1; 
        free         -= entry_free ? 8 : size + 8;
        entry_h_curr += size + 8;
    }

    if (!Consistency_Check_Last_Entry(page, entry_h_curr, stream) 
            || !Consistency_Check_Free_Amount(page, free, stream)) {
        return false;
    }
    return true;
}
