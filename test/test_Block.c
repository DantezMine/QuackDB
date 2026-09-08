#ifdef DO_TESTS

#include "Block.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VANGO_TEST_ROOT
#include <vangotest/casserts.h>

vango_test(Block_Create_and_Get) {
    uint32_t size = 1024;
    uint32_t id = 23;
    Block *block = malloc(size);
    Block_Initialize(block, size, id);
    vg_assert_eq(Block_Get_Size(block), size);
    vg_assert_eq(Block_Get_ID(block), id);
    vg_assert_eq(Block_Is_Dirty(block), false);
}

vango_test(Block_Write_and_Read) {
    uint32_t size = 1024;
    uint32_t id = 23;
    char *directory = "null";
    char *test_string = "Lorem Ipsum this DB";
    {
        Block *block = malloc(size);
        Block_Initialize(block, size, id);
        char *data = Block_Get_Data(block);
        strcpy(data+512, test_string);
        FILE *f_block = Block_Get_File(Block_Get_ID(block), directory ,"wb");
        vg_assert_non_null(f_block);
        vg_assert(Block_Write_To_File(block, f_block));
        fclose(f_block);
    }
    {
        FILE *f_other = Block_Get_File(id, directory ,"rb");
        vg_assert_non_null(f_other);
        uint32_t block_size = Block_Get_Size_From_File(f_other);
        vg_assert_eq(block_size, size);
        Block *block_other = malloc(block_size);
        Block_Read_From_File(block_other, f_other);
        char *data = Block_Get_Data(block_other);

        vg_assert_eq(Block_Get_Size(block_other), size);
        vg_assert_eq(Block_Get_ID(block_other), id);
        vg_assert_eq(Block_Is_Dirty(block_other), false);
        vg_assert_eq(strcmp(data+512, test_string), 0);
        fclose(f_other);
    }
}

vango_test_main(
    vango_test_reg(Block_Create_and_Get);
    vango_test_reg(Block_Write_and_Read);
)
#endif
