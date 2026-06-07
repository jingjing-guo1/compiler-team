/**
 * @file codegen.c
 * @brief 目标代码生成（桩，成员6实现）
 */

#include "common.h"

void gen_assembly(IRCode *ir, const char *output_filename) {
    (void)ir;
    FILE *out = fopen(output_filename, "w");
    if (!out) {
        report_error(COMPILE_ERR_CODEGEN, "Cannot create output file %s", output_filename);
        return;
    }
    fprintf(out, "; TINY compiler stub output\n");
    fprintf(out, "section .text\n");
    fprintf(out, "global _start\n");
    fprintf(out, "_start:\n");
    fprintf(out, "    mov eax, 60\n");
    fprintf(out, "    xor edi, edi\n");
    fprintf(out, "    syscall\n");
    fclose(out);
}