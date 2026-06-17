#include "Block.h"
#include "BlockPrint.h"

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
};

vango_test(Insert_Entry) {
    struct Block_Schema schema = Create_Schema_Grades();
    void *block = Block_Create(1024, 0.7f, 0.5f, schema);

    int persNr = 254950321;
    int lectureID = 201;
    int Grade = 5;
    char *name = "Lööli";
    struct Block_Field entry[4] = {
        (struct Block_Field){.attr_type=BLK_A_INT, .value=&persNr},
        (struct Block_Field){.attr_type=BLK_A_VARCHAR, .value=name, .attr_width=strlen(name)+1},
        (struct Block_Field){.attr_type=BLK_A_INT, .value=&lectureID},
        (struct Block_Field){.attr_type=BLK_A_INT, .value=&Grade},
    };
    int id = Block_Insert(block, entry);
    PRINT("Entry id: %d\n", id);
}

vango_test_main(
        vango_test_reg(Create_Block);
        vango_test_reg(Insert_Entry);
)
