/**
 * @file irgen.c
 * @brief 中间代码生成（桩，成员5实现）
 */

#include "common.h"

static int temp_counter = 0;

char *new_temp(void) {
    char *buf = (char*)safe_malloc(10);
    sprintf(buf, "t%d", temp_counter++);
    return buf;
}

IRCode *gen_ir(ASTNode *root) {
    (void)root;
    if (g_state.verbose) printf("[IRGEN] Stub: returning empty IR\n");
    IRCode *ir = (IRCode*)safe_malloc(sizeof(IRCode));
    ir->quads = NULL;
    ir->count = 0;
    ir->capacity = 0;
    return ir;
}