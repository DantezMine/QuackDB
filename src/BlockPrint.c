#include "BlockPrint.h"
#include "Block.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INT_WIDTH(val) ( val > 9  ? val > 99 ? val > 999 ? 4 : 3 : 2 : 1 )

#define MAX_CELL_WIDTH 32

#define DOTDOTDOT(text) text[MAX_CELL_WIDTH-1] = '.'; text[MAX_CELL_WIDTH-2] = '.'; text[MAX_CELL_WIDTH-3] = '.';

static double _max(double a, double b);
static double _min(double a, double b);

static void Max_Column_Widths(int num_rows, int num_cols, struct Cell_Data *rows, struct Head_Cell_Data *head, uint32_t *col_widths);

static void Fields_To_Cells(int num_rows, int num_cols, struct Block_Field **rows, struct Cell_Data *cells, char *text_arena);

//  ============================================ 
// ||          ||             ||               ||
// ||  PersNr  ||  LectureID  ||  Name         ||
// ||  INT(4)  ||  INT(4)     ||  VARCHAR(20)  ||
// ||          ||             ||               ||
//  ============================================

struct Head_Cell_Data* Block_Print_Get_Header_Cells(struct Block_Schema *schema) {
    struct Head_Cell_Data *cells = malloc(schema->attr_count*sizeof(struct Head_Cell_Data));

    int width = 0;
    for (int i=0; i<schema->attr_count; i++) {
        uint8_t attr_type = schema->attr_types[i];

        int type_width   = Block_Attribute_t_Width[attr_type] == -1 ? schema->var_sizes[i] : Block_Attribute_t_Width[attr_type];
        int type_width_len = INT_WIDTH(type_width);

        cells[i] = (struct Head_Cell_Data){
            .attr_name = schema->attr_names[i],
            .attr_name_len = strlen(schema->attr_names[i]),
            .type_name = (char *)Block_Attribute_t_Name[attr_type],
            .type_name_len = strlen(Block_Attribute_t_Name[attr_type]), // +2 for parentheses
            .type_width = type_width,
            .type_width_len = type_width_len,
        };

        cells[i].width = (int)_min(_max(cells[i].attr_name_len, cells[i].type_name_len+type_width_len+2),MAX_CELL_WIDTH);
        width += cells[i].width;
        cells[i].width_from_start = width;
    }
    return cells;
}


void Block_Print_Head(FILE *file, Block *block, struct Head_Cell_Data *head) {
    struct Block_Schema schema = Block_Get_Schema(block);
    struct Head_Cell_Data *cells = head;
    if (head == NULL) {
        cells = Block_Print_Get_Header_Cells(&schema);
    }

    int width = cells[schema.attr_count-1].width_from_start + schema.attr_count*5 + 4;
    
    char *out = malloc(width*6+1);
    memset(out, ' ', width*6);
    memset(out+1        , '=', width-3);
    memset(out+5*width+1, '=', width-3);

    int pos = 1;
    for (int i=0; i<schema.attr_count; i++) {
        for (int j=1; j<5; j++) {
            out[j*width+pos  ] = '|';
        }

        char* name_pos = out+2*width+pos+3;
        char* type_pos = name_pos + width;
        char* type_end = type_pos+2+cells[i].type_name_len+cells[i].type_width_len;
        memcpy(name_pos, cells[i].attr_name, _min(cells[i].attr_name_len, MAX_CELL_WIDTH));
        memcpy(type_pos, cells[i].type_name, _min(cells[i].type_name_len, MAX_CELL_WIDTH));
        sprintf(type_pos+cells[i].type_name_len,"(%d)",cells[i].type_width);
        *type_end = ' ';
        if (cells[i].width > MAX_CELL_WIDTH) {
            if (cells[i].attr_name_len > MAX_CELL_WIDTH) {
                DOTDOTDOT(name_pos);
            } else {
                DOTDOTDOT(type_pos);
            }
        }

        pos += 5 + cells[i].width;
    }
    for (int j=2; j<6; j++) {
        out[(j-1)*width] = '|';
        out[j*width-3] = '|';
        out[j*width-2] = '|';
        out[j*width-1] = '\n';
    }
    out[  width-1] = '\n';
    out[6*width  ] = '\0';

    fprintf(file, "%s\n", out);

    free(schema.arena);
    free(out);
    if (head == NULL) {
        free(cells);
    }
}


void Block_Print_Rows(FILE *file, Block *block, struct Block_Field **rows, int num_rows) {    
    struct Block_Schema schema = Block_Get_Schema(block);
    struct Head_Cell_Data *head = Block_Print_Get_Header_Cells(&schema);

    int col_widths_size = schema.attr_count*4;
    int cells_size = num_rows*schema.attr_count*sizeof(struct Cell_Data);
    int text_arena_size = num_rows*schema.attr_count*(MAX_CELL_WIDTH+1);

    int arena_size = col_widths_size+cells_size+text_arena_size;
    char *arena = malloc(arena_size);
    int offs = 0;

    uint32_t *col_widths = (void*)(arena+offs);
    offs += col_widths_size;
    struct Cell_Data *cells = (void*)(arena+offs);
    offs += cells_size;
    char *text_arena = (void*)(arena+offs);
    offs += arena_size;
    Fields_To_Cells(num_rows, schema.attr_count, rows, cells, text_arena);

    Max_Column_Widths(num_rows, schema.attr_count, cells, head, col_widths);

    head[0].width = col_widths[0];
    head[0].width_from_start = head[0].width;
    for (int i=1; i<schema.attr_count; i++) {
        head[i].width = col_widths[i];
        head[i].width_from_start = head[i-1].width_from_start+head[i].width;
    }
    Block_Print_Head(file, block, head);

    int txt_width = 0;
    for (int i=0; i<schema.attr_count; i++) {
        txt_width += col_widths[i];
    }
    int width = txt_width + schema.attr_count*5 + 4;
    
    char *buf = malloc(width*4*num_rows+1);
    char *out = buf;

    for (int row=0; row<num_rows; row++) {
        memset(out, ' ', width*4);
        memset(out+3*width+1, '-', width-3);

        int pos = 1;
        for (int i=0; i<schema.attr_count; i++) {
            for (int j=0; j<3; j++) {
                out[j*width+pos  ] = '|';
            }

            char* name_pos = out+1*width+pos+3;
            int val_len = strlen(cells[i+row*schema.attr_count].text);
            memcpy(name_pos, cells[i+row*schema.attr_count].text, _min(val_len, MAX_CELL_WIDTH));
            if (val_len > MAX_CELL_WIDTH) {
                DOTDOTDOT(name_pos);
            }

            pos += 5 + col_widths[i];
        }
        for (int j=1; j<4; j++) {
            out[(j-1)*width] = '|';
            out[j*width-3] = '|';
            out[j*width-2] = '|';
            out[j*width-1] = '\n';
        }
        out[4*width-1] = '\n';
        out += 4*width;
    }
    buf[4*width*num_rows] = '\0';
    fprintf(file, "%s\n", buf);

    free(buf);
    free(head);
    free(arena);
    free(schema.arena);
}

static void Fields_To_Cells(int num_rows, int num_cols, struct Block_Field **rows, struct Cell_Data *cells, char *text_arena) {
    char *text = text_arena;
    for (int i=0; i<num_rows; i++) {
        for (int j=0; j<num_cols; j++) {
            int indx = i*num_cols+j;
            switch (rows[i][j].attr_type) {
                case BLK_A_INT:
                    sprintf(text, "%d", *(int32_t*)rows[i][j].value);
                    break;
                case BLK_A_VARCHAR:
                    strncpy(text, rows[i][j].value, MAX_CELL_WIDTH);
                    text[MAX_CELL_WIDTH] = '\0'; // strncpy doesn't implicitly append null-character if too long
                    break;
            }
            int width = strlen(text);
            if (width > MAX_CELL_WIDTH) {
                text[MAX_CELL_WIDTH-1] = '.';
                text[MAX_CELL_WIDTH-2] = '.';
                text[MAX_CELL_WIDTH-3] = '.';
            }
            cells[indx].text = text;
            cells[indx].width = width;
            text += width+1;
        }
    }
}


static void Max_Column_Widths(int num_rows, int num_cols, struct Cell_Data *cells,
                              struct Head_Cell_Data *head, uint32_t *col_widths) {
    for (int j=0; j<num_cols; j++) {
        col_widths[j] = _max(0, head[j].width);
    }
    for (int i=0; i<num_rows; i++) {
        for (int j=0; j<num_cols; j++) {
            col_widths[j] = _max(col_widths[j], cells[i*num_cols+j].width);
        }
    }
}


static double _max(double a, double b) {
    return a > b ? a : b;
}

static double _min(double a, double b) {
    return a < b ? a : b;
}
