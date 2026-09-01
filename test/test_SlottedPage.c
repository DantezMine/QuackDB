/* #include "SlottedPage.h"
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

vango_test_main (
) */
