#include "Block.h"
#define DO_TESTS
#ifdef DO_TESTS

#include "SlottedPage.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define VANGO_TEST_ROOT
#include <vangotest/casserts.h>

#define DEBUG_STREAM stderr

#define Page_Sample_0_Init                     \
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

#define Page_Sample_0_Reset                    \
    memcpy(list, list_orig, list_size);        \
    page->header.free    = 16;

#define Page_Sample_0_Free \
    free(list);            \
    free(list_orig);

vango_test(Initialization) {
    Slotted_Page *page = calloc(128, 1);
    Slotted_Page_Initialize(page, 128, 19, 0.2f, 0.5f);
    vg_assert(Slotted_Page_Consistency_Check(page, DEBUG_STREAM));
}

vango_test(Insert_Entry) {
    Page_Sample_0_Init;
    {
        *((uint32_t*)(list+32 )) = -1;
        *((uint32_t*)(list+86 )) |= 1;
        *((uint32_t*)(list+110)) |= 1;
        page->header.free += 20;
        const uint32_t data_size = 10;
        const uint8_t data[10] = {0};
        RID rid = Slotted_Page_Insert_Entry(page, data, data_size);
        Page_Entry entry = Slotted_Page_Get_Entry(page, rid);
        vg_assert(Slotted_Page_Consistency_Check(page, DEBUG_STREAM));
        vg_assert_eq(rid.block_ID, 21);
        vg_assert_eq(rid.row_index, 3);
        vg_assert_eq((uint8_t*)entry.Entry.entry, list+100);
        vg_assert_eq(data_size, entry.Entry.size);
    }
    Page_Sample_0_Reset;
    {
        const uint32_t data_size = 4;
        const uint8_t data[4] = {0};
        RID rid = Slotted_Page_Insert_Entry(page, data, data_size);
        Page_Entry entry = Slotted_Page_Get_Entry(page, rid);
        vg_assert(Slotted_Page_Consistency_Check(page, DEBUG_STREAM));
        vg_assert_eq(rid.block_ID, 21);
        vg_assert_eq(rid.row_index, 3);
        vg_assert_eq((uint8_t*)entry.Entry.entry, list+56);
        vg_assert_eq(data_size, entry.Entry.size);
    }
    Page_Sample_0_Reset;
    {
        uint32_t data_size = 8;
        const uint8_t data[8] = {0};
        RID rid = Slotted_Page_Insert_Entry(page, data, data_size);
        vg_assert_null(rid.rid);
        vg_assert(Slotted_Page_Consistency_Check(page, DEBUG_STREAM));
    }
    Page_Sample_0_Reset;
    {
        Slotted_Page *page = calloc(128, 1);
        Slotted_Page_Initialize(page, 128, 19, 0.2f, 0.5f);
        const uint32_t data_size = 11;
        const uint8_t data[12] = "Hello World";
        RID rid = Slotted_Page_Insert_Entry(page, data, data_size);
        Page_Entry entry = Slotted_Page_Get_Entry(page, rid);
        vg_assert(Slotted_Page_Consistency_Check(page, DEBUG_STREAM));
        vg_assert_eq((uint8_t*)entry.Entry.entry, (uint8_t*)page+112);
        vg_assert(memcmp(data, entry.Entry.entry, data_size) == 0);
        FILE *fd = Block_Get_File(Block_Get_ID((Block*)page), "null", "wb");
        Block_Write_To_File((Block*)page, fd);
        Block_Close_File(fd);
        free(page);

    }
    Page_Sample_0_Reset;
    {
        Slotted_Page *page = calloc(512, 1);
        Slotted_Page_Initialize(page, 512, 20, 0.2f, 0.5f);
        const uint32_t data_size = 11;
        const uint8_t data[12] = "Hello World";
        RID rows[20] = {0};
        for (int i=0; i<20; i++) {
            if (Slotted_Page_Can_Fit_Insert(page, data_size)) {
                RID rid = Slotted_Page_Insert_Entry(page, data, data_size);
                if (rid.rid == 0) break;
                rows[i] = rid;
            } else {
                break;
            }
        }
        Page_Entry entry = Slotted_Page_Get_Entry(page, rows[2]);
        vg_assert(Slotted_Page_Consistency_Check(page, DEBUG_STREAM));
        vg_assert(memcmp(data, entry.Entry.entry, data_size) == 0);
        FILE *fd = Block_Get_File(Block_Get_ID((Block*)page), "null", "wb");
        Block_Write_To_File((Block*)page, fd);
        Block_Close_File(fd);
        free(page);
    }
    Page_Sample_0_Free;
}


vango_test(Update_Entry) {
    {
        Slotted_Page *page = calloc(128, 1);
        Slotted_Page_Initialize(page, 128, 24, 0.2f, 0.5f);

        const uint32_t data_size = 11;
        const uint8_t data[12] = "Hello World";
        RID rows[10] = {0};
        for (int i=0; i<2; i++) {
            rows[i] = Slotted_Page_Insert_Entry(page, data, data_size);
        }

        const uint8_t data_new[14] = "Bye Bye World";
        Slotted_Page_Update_Entry(page, rows[1].row_index, data_new, 13);
        Page_Entry entry = Slotted_Page_Get_Entry(page, rows[1]);

        vg_assert(Slotted_Page_Consistency_Check(page, DEBUG_STREAM));
        vg_assert(memcmp(data_new, entry.Entry.entry, 13) == 0);

        FILE *fd = Block_Get_File(Block_Get_ID((Block*)page), "null", "wb");
        Block_Write_To_File((Block*)page, fd);
        Block_Close_File(fd);
        free(page);
    }
    {
        Slotted_Page *page = calloc(128, 1);
        Slotted_Page_Initialize(page, 128, 25, 0.2f, 0.5f);

        const uint32_t data_size = 11;
        const uint8_t data[12] = "Hello World";
        RID rows[10] = {0};
        for (int i=0; i<2; i++) {
            rows[i] = Slotted_Page_Insert_Entry(page, data, data_size);
        }

        const uint8_t data_new[14] = "Bye Bye World";
        Slotted_Page_Update_Entry(page, rows[0].row_index, data_new, 13);
        Page_Entry entry = Slotted_Page_Get_Entry(page, rows[0]);

        vg_assert(Slotted_Page_Consistency_Check(page, DEBUG_STREAM));
        vg_assert(memcmp(data_new, entry.Entry.entry, 13) == 0);

        FILE *fd = Block_Get_File(Block_Get_ID((Block*)page), "null", "wb");
        Block_Write_To_File((Block*)page, fd);
        Block_Close_File(fd);
        free(page);
    }
    {
        Slotted_Page *page = calloc(128, 1);
        Slotted_Page_Initialize(page, 128, 24, 0.2f, 0.5f);

        const uint32_t data_size = 11;
        const uint8_t data[12] = "Hello World";
        RID rows[10] = {0};
        for (int i=0; i<3; i++) {
            rows[i] = Slotted_Page_Insert_Entry(page, data, data_size);
        }

        const uint8_t data_new[14] = "Bye Bye World";
        bool success = Slotted_Page_Update_Entry(page, rows[1].row_index, data_new, 13);
        vg_assert(!success);
        
        Slotted_Page *page_other = calloc(128, 1);
        Slotted_Page_Initialize(page_other, 128, 26, 0.2f, 0.5f);

        success = Slotted_Page_Update_Entry_Next(page, rows[1].row_index, data_new, 13, page_other);
        vg_assert(success);
        Page_Entry entry = Slotted_Page_Get_Entry(page, rows[1]);
        vg_assert(entry.indirect);
        RID rid = (RID){.row_index = 0, .block_ID = 26};
        vg_assert_eq(entry.reference.rid, rid.rid);
        
        free(page);
        free(page_other);
    }
}

vango_test_main (
    vango_test_reg(Initialization);
    vango_test_reg(Insert_Entry);
    vango_test_reg(Update_Entry);
) 
#endif
