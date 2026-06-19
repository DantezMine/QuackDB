#include "Block.h"
#include "BlockPrint.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define VANGO_TEST_ROOT
#include <vangotest/casserts.h>


// #define PRINT_ASSERTS

#ifdef PRINT_ASSERTS
#define PRINT(...) (printf(__VA_ARGS__))
#else
#define PRINT(...)
#endif

enum Block_Attribute_t attr_types_Grades[] = {BLK_A_INT, BLK_A_VARCHAR, BLK_A_INT, BLK_A_INT};
char *attr_names_Grades[] = {
    "PersNr",
    "Name",
    "LectureID",
    "Grade",
};
int var_sizes_Grades[] = {0, 20, 0, 0};

static struct Block_Schema Create_Schema_Grades() {

    return (struct Block_Schema){
        .attr_count = 4,
        .attr_types = attr_types_Grades,
        .attr_names = attr_names_Grades,
        .var_sizes = var_sizes_Grades,
    };
}

vango_test(Create_Block) {
    struct Block_Schema schema = Create_Schema_Grades();
    void *block = Block_Create(1024, 0.7f, 0.5f, schema);
    vg_assert(block != NULL);
    if (block != NULL) PRINT("Block creation successful.\n");
    Block_Print_Head(stdout, block, NULL);
    Block_Remove(block);
};

vango_test(Insert_Entry) {
    struct Block_Schema schema = Create_Schema_Grades();
    void *block = Block_Create(1024, 0.7f, 0.5f, schema);

    uint32_t persNr = 254950321;
    uint32_t lectureID = 201;
    uint32_t grade = 5;
    char *name = "Loeli";
    struct Block_Field entry[4] = {
        (struct Block_Field){.attr_type=BLK_A_INT, .value=&persNr},
        (struct Block_Field){.attr_type=BLK_A_VARCHAR, .value=name, .attr_width=strlen(name)+1},
        (struct Block_Field){.attr_type=BLK_A_INT, .value=&lectureID},
        (struct Block_Field){.attr_type=BLK_A_INT, .value=&grade},
    };

    Block_Insert_Entry(block, entry);
    Block_Insert_Entry(block, entry);
    Block_Insert_Entry(block, entry);
    Block_Insert_Entry(block, entry);
    Block_Insert_Entry(block, entry);

    int num_entries = 5;
    struct Block_Field **ret_entry = malloc(8*num_entries);
    for (int i=0; i<num_entries; i++) {
        ret_entry[i] = Block_Get(block, i);
    }

    for (int i=0; i<4; i++) {
        vg_assert_eq(ret_entry[0][i].attr_type,entry[i].attr_type);
    }
    vg_assert_eq(*(uint32_t*)ret_entry[0][0].value,persNr);
    vg_assert_eq(*(uint32_t*)ret_entry[0][2].value,lectureID);
    vg_assert_eq(*(uint32_t*)ret_entry[0][3].value,grade);
    vg_assert(strcmp(ret_entry[0][1].value, entry[1].value)==0);
    vg_assert_eq(ret_entry[0][1].attr_width,entry[1].attr_width);
    Block_Print_Rows(stdout, block, ret_entry, num_entries);

    for (int i=0; i<num_entries; i++) {
        free(ret_entry[i]);
    }
    free(schema.arena);
    free(ret_entry);
    Block_Remove(block);
}


vango_test(Delete_Entry) {
    struct Block_Schema schema = Create_Schema_Grades();
    void *block = Block_Create(1024, 0.7f, 0.5f, schema);

    uint32_t persNr = 254950321;
    uint32_t lectureID = 201;
    uint32_t grade = 5;
    char *name = "Loeli";
    struct Block_Field entry[4] = {
        (struct Block_Field){.attr_type=BLK_A_INT, .value=&persNr},
        (struct Block_Field){.attr_type=BLK_A_VARCHAR, .value=name, .attr_width=strlen(name)+1},
        (struct Block_Field){.attr_type=BLK_A_INT, .value=&lectureID},
        (struct Block_Field){.attr_type=BLK_A_INT, .value=&grade},
    };

    Block_Insert_Entry(block, entry);
    Block_Insert_Entry(block, entry);
    Block_Insert_Entry(block, entry);
    Block_Insert_Entry(block, entry);
    Block_Insert_Entry(block, entry);

    Block_Delete_Entry(block, 3);

    int num_entries = 4;
    struct Block_Field **ret_entry = malloc(8*num_entries);
    ret_entry[0] = Block_Get(block, 0);
    ret_entry[1] = Block_Get(block, 1);
    ret_entry[2] = Block_Get(block, 2);
    ret_entry[3] = Block_Get(block, 4);

    Block_Print_Rows(stdout, block, ret_entry, num_entries);

    for (int i=0; i<num_entries; i++) {
        free(ret_entry[i]);
    }
    free(schema.arena);
    free(ret_entry);
    Block_Remove(block);
}

vango_test_main(
        vango_test_reg(Create_Block);
        vango_test_reg(Insert_Entry);
        vango_test_reg(Delete_Entry);
)
