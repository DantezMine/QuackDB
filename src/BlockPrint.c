#include "BlockPrint.h"
#include "Block.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INT_WIDTH(val) ( val > 9  ? val > 99 ? val > 999 ? 4 : 3 : 2 : 1 )

//   ===========================
//  ||          ||             ||
//  ||  PersNr  ||  LectureID  ||
//  ||  INT(4)  ||  INT(4)     ||
//  ||          ||             ||
//   ===========================
//
//  ============================================ 
// ||          ||             ||               ||
// ||  PersNr  ||  LectureID  ||  Name         ||
// ||  INT(4)  ||  INT(4)     ||  VARCHAR(20)  ||
// ||          ||             ||               ||
//  ============================================

struct Cell_Data* Block_Print_Get_Header_Cells(struct Block_Schema *schema) {
    struct Cell_Data *cells = malloc(schema->attr_count*sizeof(struct Cell_Data));

    int width = 0;
    for (int i=0; i<schema->attr_count; i++) {
        uint8_t attr_type = schema->attr_types[i];

        int type_width   = Block_Attribute_t_Width[attr_type] == -1 ? schema->var_sizes[i] : Block_Attribute_t_Width[attr_type];
        int type_width_len = INT_WIDTH(type_width);

        cells[i] = (struct Cell_Data){
            .attr_name = schema->attr_names[i],
            .attr_name_len = strlen(schema->attr_names[i]),
            .type_name = (char *)Block_Attribute_t_Name[attr_type],
            .type_name_len = strlen(Block_Attribute_t_Name[attr_type]), // +2 for parentheses
            .type_width = type_width,
            .type_width_len = type_width_len,
        };

        cells[i].width = (int)fmax(cells[i].attr_name_len, cells[i].type_name_len+type_width_len+2);
        width += cells[i].width;
        cells[i].width_from_start = width;
    }
    return cells;
}


void Block_Print_Head(FILE *file, void *block) {
    struct Block_Schema schema = Block_Get_Schema(block);

    struct Cell_Data *cells = Block_Print_Get_Header_Cells(&schema);

    int width = cells[schema.attr_count-1].width_from_start + schema.attr_count*6 + 3;
    
    char *out = malloc(width*6+1);
    memset(out, ' ', width*6+6);
    memset(out+1        , '=', width-3);
    memset(out+5*width+1, '=', width-3);

    int pos = 0;
    for (int i=0; i<schema.attr_count; i++) {
        for (int j=1; j<5; j++) {
            out[j*width+pos  ] = '|';
            out[j*width+pos+1] = '|';
        }

        char* name_pos = out+2*width+pos+4;
        char* type_pos = name_pos + width;
        char* type_end = type_pos+2+cells[i].type_name_len+cells[i].type_width_len;
        memcpy(name_pos, cells[i].attr_name, cells[i].attr_name_len);
        memcpy(type_pos, cells[i].type_name, cells[i].type_name_len);
        sprintf(type_pos+cells[i].type_name_len,"(%d)",cells[i].type_width);
        *type_end = ' ';

        int used_width = fmax((int)cells[i].type_name_len+2+cells[i].type_width_len, cells[i].attr_name_len);
        pos += 6 + used_width;
    }
    for (int j=2; j<6; j++) {
        out[j*width-3] = '|';
        out[j*width-2] = '|';
        out[j*width-1] = '\n';
    }
    out[  width-1] = '\n';
    out[6*width  ] = '\0';

    fprintf(file, "%s\n",out);

    free(schema.attr_names);
    free(schema.attr_types);
    free(schema.var_sizes);
    free(out);
    free(cells);
}
