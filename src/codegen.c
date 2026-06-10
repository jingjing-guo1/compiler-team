/**
 * @file codegen.c
 * @brief 目标代码生成（x86-64, AT&T语法）
 */

#include "common.h"
#include <ctype.h>

static FILE *out;

static void emit(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    fprintf(out, "\n");
    va_end(args);
}

static void emit_mov_imm(const char *reg, int imm) {
    emit("    movl $%d, %%%s", imm, reg);
}

void gen_assembly(IRCode *ir, const char *output_filename) {
    out = fopen(output_filename, "w");
    if (!out) {
        report_error(COMPILE_ERR_CODEGEN, "Cannot create output file %s", output_filename);
        return;
    }

    emit(".section .text");
    emit(".global main");
    emit("main:");
    emit("    pushq %%rbp");
    emit("    movq %%rsp, %%rbp");
    // 分配栈空间简单固定
    emit("    subq $256, %%rsp");

    // 遍历IR，生成汇编（简化实现，只支持部分指令）
    for (int i = 0; i < ir->count; i++) {
        IRQuad *q = &ir->quads[i];
        switch (q->op) {
            case IR_ASSIGN:
                if (q->arg1 && isdigit(q->arg1[0])) {
                    // 立即数赋值
                    int val = atoi(q->arg1);
                    emit_mov_imm("eax", val);
                } else if (q->arg1 && strncmp(q->arg1, "t", 1) == 0) {
                    emit("    movl %s(%%rbp), %%eax", q->arg1);
                } else if (q->arg1 && strchr(q->arg1, '_') == NULL) {
                    // 变量
                    emit("    movl %s(%%rbp), %%eax", q->arg1);
                } else {
                    emit("    movl $0, %%eax");
                }
                if (q->result && strncmp(q->result, "t", 1) == 0) {
                    emit("    movl %%eax, %s(%%rbp)", q->result);
                } else if (q->result) {
                    emit("    movl %%eax, %s(%%rbp)", q->result);
                }
                break;
            case IR_ADD:
                emit("    movl %s(%%rbp), %%eax", q->arg1);
                emit("    addl %s(%%rbp), %%eax", q->arg2);
                emit("    movl %%eax, %s(%%rbp)", q->result);
                break;
            case IR_SUB:
                emit("    movl %s(%%rbp), %%eax", q->arg1);
                emit("    subl %s(%%rbp), %%eax", q->arg2);
                emit("    movl %%eax, %s(%%rbp)", q->result);
                break;
            case IR_MUL:
                emit("    movl %s(%%rbp), %%eax", q->arg1);
                emit("    imull %s(%%rbp), %%eax", q->arg2);
                emit("    movl %%eax, %s(%%rbp)", q->result);
                break;
            case IR_DIV:
                emit("    movl %s(%%rbp), %%eax", q->arg1);
                emit("    cltd");
                emit("    idivl %s(%%rbp)", q->arg2);
                emit("    movl %%eax, %s(%%rbp)", q->result);
                break;
            case IR_LABEL:
                fprintf(out, "%s:\n", q->arg1);
                break;
            case IR_GOTO:
                emit("    jmp %s", q->result);
                break;
            case IR_IF_FALSE_GOTO:
                emit("    cmpl $0, %s(%%rbp)", q->arg1);
                emit("    je %s", q->result);
                break;
            case IR_RETURN:
                if (q->arg1) emit("    movl %s(%%rbp), %%eax", q->arg1);
                emit("    leave");
                emit("    ret");
                break;
            case IR_FUNC_ENTER:
                emit(".globl %s", q->arg1);
                emit("%s:", q->arg1);
                emit("    pushq %%rbp");
                emit("    movq %%rsp, %%rbp");
                emit("    subq $256, %%rsp");
                break;
            case IR_FUNC_EXIT:
                emit("    leave");
                emit("    ret");
                break;
            default:
                emit("    # unimplemented IR op %d", q->op);
                break;
        }
    }
    fclose(out);
}
