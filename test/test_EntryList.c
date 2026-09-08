#ifdef DO_TESTS

#include "SlottedPage.h"
#include "../src/SlottedPage.c" // allows to test static functions
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define VANGO_TEST_ROOT
#include <vangotest/casserts.h>

#define DEBUG_STREAM stderr

#define Page_Sample_0_Init \
    uint32_t list_size = 136;                  \
    uint8_t *list = calloc(list_size, 1);      \
    uint8_t *list_orig = calloc(list_size, 1); \
    float PCTFREE = 0.2f;                      \
    float PCTUSED = 0.5f;                      \
    *((uint32_t*)(list+0  )) = 128;            \
    *((uint32_t*)(list+4  )) = 21;             \
    *((uint32_t*)(list+8  )) = 0;              \
    *((uint32_t*)(list+12 )) = *&PCTFREE;      \
    *((uint32_t*)(list+16 )) = *&PCTUSED;      \
    *((uint32_t*)(list+20 )) = 16;             \
    *((uint32_t*)(list+24 )) = 3;              \
    *((uint32_t*)(list+28 )) = 68;             \
    *((uint32_t*)(list+32 )) = 80;             \
    *((uint32_t*)(list+36 )) = 108;            \
    *((uint32_t*)(list+40 )) = 17;             \
    *((uint32_t*)(list+60 )) = 17;             \
    *((uint32_t*)(list+64 )) = 14;             \
    *((uint32_t*)(list+82 )) = 14;             \
    *((uint32_t*)(list+86 )) = 20;             \
    *((uint32_t*)(list+110)) = 20;             \
    *((uint32_t*)(list+114)) = 6;              \
    *((uint32_t*)(list+124)) = 6;              \
    memcpy(list_orig, list, list_size);        \
    Slotted_Page *page = (Slotted_Page*)list;  \
    page->header.PCTFREE = *&PCTFREE;          \
    page->header.PCTUSED = *&PCTUSED;          \
    page->header.free    = 16;

#define Page_Sample_0_Reset \
    memcpy(list, list_orig, list_size);        \
    page->header.free    = 16;

#define Page_Sample_0_Free \
    free(list);       \
    free(list_orig);



vango_test(Slotted_Page_Consistency_Checking) {
    Page_Sample_0_Init 
    
    vg_assert(Slotted_Page_Consistency_Check(page, DEBUG_STREAM));

    // Mismatched header and footer
    *((uint32_t*)(list+60 )) = 19;
    vg_assert(!Slotted_Page_Consistency_Check(page, DEBUG_STREAM));
    *((uint32_t*)(list+60 )) = 17;

    // Mismatched header and footer
    *((uint32_t*)(list+40 )) = 19;
    vg_assert(!Slotted_Page_Consistency_Check(page, DEBUG_STREAM));
    *((uint32_t*)(list+40 )) = 17;

    // Last entry past size of block
    // List size large enough to avoid segfault
    *((uint32_t*)(list+114)) = 8;
    *((uint32_t*)(list+126)) = 8;
    vg_assert(!Slotted_Page_Consistency_Check(page, DEBUG_STREAM));
    *((uint32_t*)(list+114)) = 6;
    *((uint32_t*)(list+124)) = 6;

    // Wrong free amount in page
    *((uint32_t*)(list+20 )) = 18;
    vg_assert(!Slotted_Page_Consistency_Check(page, DEBUG_STREAM));
    *((uint32_t*)(list+60 )) = 16;

    Page_Sample_0_Free;
}

vango_test(Split_Entry_List) {
    Page_Sample_0_Init;

    vg_assert(Slotted_Page_Consistency_Check(page, DEBUG_STREAM));

    {
        uint32_t data_size = 4;
        uint32_t *entry_free = (uint32_t*) ((uint8_t *)page + 44);
        uint32_t *entry = Entry_List_Split(page, entry_free, data_size);
        vg_assert(Slotted_Page_Consistency_Check(page, DEBUG_STREAM));
        vg_assert_eq((uint32_t*) ((uint8_t *)page + 56), entry);
    }
    Page_Sample_0_Reset;
    {
        // Attempting to split first entry (size 16).
        // First entry must always be free, thus split required.
        // Split not possible, no space for additional header and footer.
        uint32_t data_size = 10;
        uint32_t *entry_free = (uint32_t*) ((uint8_t *)page + 44);
        uint32_t *entry = Entry_List_Split(page, entry_free, data_size);
        vg_assert_null(entry);
    }
    Page_Sample_0_Reset;
    {
        // Freed up entry at offset 90
        *((uint32_t*)(list+86 )) |= 1;
        *((uint32_t*)(list+110)) |= 1;
        page->header.free += 20;
        uint32_t data_size = 10;
        uint32_t *entry_free = (uint32_t*) ((uint8_t *)page + 90);
        uint32_t *entry = Entry_List_Split(page, entry_free, data_size);
        vg_assert(Slotted_Page_Consistency_Check(page, DEBUG_STREAM));
        vg_assert_eq((uint32_t*) ((uint8_t *)page + 100), entry);
    }

    Page_Sample_0_Free;
}


vango_test(Find_Free_Entry) {
    Page_Sample_0_Init;
    {
        *((uint32_t*)(list+86 )) |= 1;
        *((uint32_t*)(list+110)) |= 1;
        uint32_t data_size = 10;
        uint32_t *entry_free  = (uint32_t*) ((uint8_t *)page + 90);
        uint32_t *entry_found = (uint32_t*)Entry_List_Find_Free_Entry(page, data_size);
        vg_assert_eq(entry_free, entry_found);
    }
    Page_Sample_0_Reset;
    {
        *((uint32_t*)(list+86 )) |= 1;
        *((uint32_t*)(list+110)) |= 1;
        *((uint32_t*)(list+114)) |= 1;
        *((uint32_t*)(list+124)) |= 1;
        uint32_t data_size = 10;
        uint32_t *entry_free  = (uint32_t*) ((uint8_t *)page + 90);
        uint32_t *entry_found = (uint32_t*)Entry_List_Find_Free_Entry(page, data_size);
        vg_assert_eq(entry_free, entry_found);
    }
    Page_Sample_0_Reset;
    {
        // Not technically consistent, as two adjacent entries
        // are free, but that's ok for this test.
        *((uint32_t*)(list+86 )) |= 1;
        *((uint32_t*)(list+110)) |= 1;
        *((uint32_t*)(list+114)) |= 1;
        *((uint32_t*)(list+124)) |= 1;
        uint32_t data_size = 6;
        uint32_t *entry_free  = (uint32_t*) ((uint8_t *)page + 118);
        uint32_t *entry_found = (uint32_t*)Entry_List_Find_Free_Entry(page, data_size);
        vg_assert_eq(entry_free, entry_found);
    }
    Page_Sample_0_Reset;
    {
        *((uint32_t*)(list+64 )) |= 1;
        *((uint32_t*)(list+82 )) |= 1;
        *((uint32_t*)(list+86 )) |= 1;
        *((uint32_t*)(list+110)) |= 1;
        // Perfect fits are preferred over last fit
        uint32_t data_size = 14;
        uint32_t *entry_free  = (uint32_t*) ((uint8_t *)page + 68);
        uint32_t *entry_found = (uint32_t*)Entry_List_Find_Free_Entry(page, data_size);
        vg_assert_eq(entry_free, entry_found);
    }
    Page_Sample_0_Reset;
    {
        uint32_t data_size = 4;
        uint32_t *entry_free  = (uint32_t*) ((uint8_t *)page + 44);
        uint32_t *entry_found = (uint32_t*)Entry_List_Find_Free_Entry(page, data_size);
        vg_assert_eq(entry_free, entry_found);
    }
    Page_Sample_0_Reset;
    {
        uint32_t data_size = 16;
        uint32_t *entry_found = (uint32_t*)Entry_List_Find_Free_Entry(page, data_size);
        vg_assert_null(entry_found);
    }
    Page_Sample_0_Free;
}


vango_test(Free_Entry) {
    Page_Sample_0_Init;
    {
        uint32_t *entry  = (uint32_t*) ((uint8_t *)page + 68);
        Entry_List_Free(page, entry);
        vg_assert(Slotted_Page_Consistency_Check(page, DEBUG_STREAM));
        vg_assert_eq(*((uint32_t*)(list+40 )), 39);
        vg_assert_eq(*((uint32_t*)(list+82 )), 39);
    }
    Page_Sample_0_Reset;
    {
        *((uint32_t*)(list+86 )) |= 1;
        *((uint32_t*)(list+110)) |= 1;
        page->header.free += 20;
        uint32_t *entry  = (uint32_t*) ((uint8_t *)page + 68);
        Entry_List_Free(page, entry);
        vg_assert(Slotted_Page_Consistency_Check(page, DEBUG_STREAM));
        vg_assert_eq(*((uint32_t*)(list+40 )), 67);
        vg_assert_eq(*((uint32_t*)(list+110)), 67);
    }
    Page_Sample_0_Reset;
    {
        *((uint32_t*)(list+86 )) |= 1;
        *((uint32_t*)(list+110)) |= 1;
        page->header.free += 20;
        uint32_t *entry  = (uint32_t*) ((uint8_t *)page + 118);
        Entry_List_Free(page, entry);
        vg_assert(Slotted_Page_Consistency_Check(page, DEBUG_STREAM));
        vg_assert_eq(*((uint32_t*)(list+86 )), 35);
        vg_assert_eq(*((uint32_t*)(list+124)), 35);
    }
    Page_Sample_0_Reset;
    {
        uint32_t *entry  = (uint32_t*) ((uint8_t *)page + 118);
        Entry_List_Free(page, entry);
        vg_assert(Slotted_Page_Consistency_Check(page, DEBUG_STREAM));
        vg_assert_eq(*((uint32_t*)(list+114)), 7);
        vg_assert_eq(*((uint32_t*)(list+124)), 7);
    }
    Page_Sample_0_Free;
}

vango_test(Entry_List_No_Coalescing) {
    uint32_t list_size = 128;
    uint8_t *list = malloc(list_size);
    
    *((uint32_t*)(list+0  )) = 50 | 0; // header 1
    *((uint32_t*)(list+54 )) = 50 | 0; // footer 1
    *((uint32_t*)(list+58 )) = 16 | 0; // header 2
    *((uint32_t*)(list+78 )) = 16 | 0; // footer 2
    *((uint32_t*)(list+82 )) = 22 | 1; // header 3
    *((uint32_t*)(list+108)) = 22 | 1; // footer 3
    *((uint32_t*)(list+112)) =  8 | 0; // header 4
    *((uint32_t*)(list+124)) =  8 | 0; // footer 4
    
    uint8_t *list_copy = memcpy(malloc(list_size), list, list_size); 

    uint8_t *list_left = memcpy(malloc(list_size), list, list_size); 

    uint8_t *list_right = memcpy(malloc(list_size), list, list_size); 
    
    Entry_List_Coalesce_Left((uint32_t*)(list_copy+86));
    vg_assert_eq(memcmp(list_left, list_copy, list_size), 0);

    Entry_List_Coalesce_Right((uint32_t*)(list_copy+86));
    vg_assert_eq(memcmp(list_right, list_copy, list_size), 0);

    free(list);
    free(list_copy);
    free(list_left);
    free(list_right);
}


vango_test(Entry_List_Coalescing_Left) {
    uint32_t list_size = 128;
    uint8_t *list = malloc(list_size);
    
    *((uint32_t*)(list+0  )) = 50 | 0; // header 1
    *((uint32_t*)(list+54 )) = 50 | 0; // footer 1
    *((uint32_t*)(list+58 )) = 16 | 1; // header 2
    *((uint32_t*)(list+78 )) = 16 | 1; // footer 2
    *((uint32_t*)(list+82 )) = 22 | 1; // header 3
    *((uint32_t*)(list+108)) = 22 | 1; // footer 3
    *((uint32_t*)(list+112)) =  8 | 0; // header 4
    *((uint32_t*)(list+124)) =  8 | 0; // footer 4
    
    uint8_t *list_copy = memcpy(malloc(list_size), list, list_size); 

    uint8_t *list_left = memcpy(malloc(list_size), list, list_size); 
    *((uint32_t*)(list_left+58 )) = 46 | 1; // header 2
    *((uint32_t*)(list_left+108)) = 46 | 1; // footer 3

    uint8_t *list_right = memcpy(malloc(list_size), list_left, list_size); 
    
    Entry_List_Coalesce_Left((uint32_t*)(list_copy+86));
    vg_assert_eq(memcmp(list_left, list_copy, list_size), 0);

    Entry_List_Coalesce_Right((uint32_t*)(list_copy+86));
    vg_assert_eq(memcmp(list_right, list_copy, list_size), 0);

    free(list);
    free(list_copy);
    free(list_left);
    free(list_right);
}

vango_test(Entry_List_Coalescing_Right) {
    uint32_t list_size = 128;
    uint8_t *list = malloc(list_size);
    
    *((uint32_t*)(list+0  )) = 50 | 0; // header 1
    *((uint32_t*)(list+54 )) = 50 | 0; // footer 1
    *((uint32_t*)(list+58 )) = 16 | 0; // header 2
    *((uint32_t*)(list+78 )) = 16 | 0; // footer 2
    *((uint32_t*)(list+82 )) = 22 | 1; // header 3
    *((uint32_t*)(list+108)) = 22 | 1; // footer 3
    *((uint32_t*)(list+112)) =  8 | 1; // header 4
    *((uint32_t*)(list+124)) =  8 | 1; // footer 4
    
    uint8_t *list_copy = memcpy(malloc(list_size), list, list_size); 

    uint8_t *list_left = memcpy(malloc(list_size), list, list_size); 

    uint8_t *list_right = memcpy(malloc(list_size), list_left, list_size); 
    *((uint32_t*)(list_right+82 )) = 38 | 1; // header 3
    *((uint32_t*)(list_right+124)) = 38 | 1; // footer 4
    
    Entry_List_Coalesce_Left((uint32_t*)(list_copy+86));
    vg_assert_eq(memcmp(list_left, list_copy, list_size), 0);

    Entry_List_Coalesce_Right((uint32_t*)(list_copy+86));
    vg_assert_eq(memcmp(list_right, list_copy, list_size), 0);

    free(list);
    free(list_copy);
    free(list_left);
    free(list_right);
}



vango_test(Entry_List_Coalescing_Both) {
    uint32_t list_size = 128;
    uint8_t *list = malloc(list_size);
    
    *((uint32_t*)(list+0  )) = 50 | 0; // header 1
    *((uint32_t*)(list+54 )) = 50 | 0; // footer 1
    *((uint32_t*)(list+58 )) = 16 | 1; // header 2
    *((uint32_t*)(list+78 )) = 16 | 1; // footer 2
    *((uint32_t*)(list+82 )) = 30 | 1; // header 3
    *((uint32_t*)(list+116)) = 30 | 1; // footer 3
    *((uint32_t*)(list+120)) =  0 | 1; // header 4
    *((uint32_t*)(list+124)) =  0 | 1; // footer 4
    
    uint8_t *list_copy = memcpy(malloc(list_size), list, list_size); 

    uint8_t *list_left = memcpy(malloc(list_size), list, list_size); 
    *((uint32_t*)(list_left+58 )) = 54 | 1; // header 2
    *((uint32_t*)(list_left+116)) = 54 | 1; // footer 3

    uint8_t *list_right = memcpy(malloc(list_size), list_left, list_size); 
    *((uint32_t*)(list_right+58 )) = 62 | 1; // header 2
    *((uint32_t*)(list_right+124)) = 62 | 1; // footer 4
    
    uint32_t *new_entry = Entry_List_Coalesce_Left((uint32_t*)(list_copy+86));
    vg_assert_eq(memcmp(list_left, list_copy, list_size), 0);

    Entry_List_Coalesce_Right(new_entry);
    vg_assert_eq(memcmp(list_right, list_copy, list_size), 0);

    free(list);
    free(list_copy);
    free(list_left);
    free(list_right);
}



vango_test_main (
    vango_test_reg(Entry_List_No_Coalescing);
    vango_test_reg(Entry_List_Coalescing_Left);
    vango_test_reg(Entry_List_Coalescing_Right);
    vango_test_reg(Entry_List_Coalescing_Both);
    vango_test_reg(Slotted_Page_Consistency_Checking);
    vango_test_reg(Split_Entry_List);
    vango_test_reg(Find_Free_Entry);
    vango_test_reg(Free_Entry);
)
#endif
