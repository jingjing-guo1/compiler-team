/**
 * @file codegen.c
 * @brief 目标代码生成（x86-64, AT&T语法）- 支持数组与函数参数传递
 * @author 成员6
 */

#include "common.h"
#include <ctype.h>
#include <string.h>

 /* ---------- 符号映射（变量名 → 栈偏移与类型） ---------- */
typedef struct VarInfo {
    char* name;
    int offset;          /* 基址偏移（负值） */
    int array_size;      /* 数组长度，-1 表示非数组 */
    struct VarInfo* next;
} VarInfo;

static VarInfo* var_table = NULL;
static int next_offset = -4;   /* 下一个空闲偏移（向下增长） */

/* ---------- 参数传递临时存储 ---------- */
static char* param_values[6];   /* 最多支持6个参数，存储其表达式（变量名或立即数字符串） */
static int param_count = 0;

/* 查找或分配变量信息 */
static VarInfo* get_var_info(const char* name) {
    if (!name) return NULL;
    for (VarInfo* p = var_table; p; p = p->next) {
        if (strcmp(p->name, name) == 0)
            return p;
    }
    return NULL;
}

/* 为普通变量分配空间（4字节） */
static int alloc_var(const char* name) {
    VarInfo* info = get_var_info(name);
    if (info) return info->offset;
    info = safe_malloc(sizeof(VarInfo));
    info->name = strdup(name);
    info->offset = next_offset;
    info->array_size = -1;
    info->next = var_table;
    var_table = info;
    next_offset -= 4;
    return info->offset;
}

/* 为数组分配连续空间（size * 4 字节） */
static int alloc_array(const char* name, int size) {
    VarInfo* info = get_var_info(name);
    if (info) return info->offset;
    int bytes = size * 4;
    next_offset -= bytes;
    info = safe_malloc(sizeof(VarInfo));
    info->name = strdup(name);
    info->offset = next_offset;
    info->array_size = size;
    info->next = var_table;
    var_table = info;
    return info->offset;
}
static int is_immediate(const char* s)
{
    if (!s) return 0;

    if (*s == '-')
        s++;

    return isdigit(*s);
}

/* 获取变量的栈偏移（自动分配） */
static int get_offset(const char* name) {
    VarInfo* info = get_var_info(name);
    if (info) return info->offset;
    return alloc_var(name);
}

/* 解析数组访问字符串 "arr[index]" */
static int parse_array_access(const char* str, char* arr_name, char* idx_name) {
    const char* lbracket = strchr(str, '[');
    if (!lbracket) return 0;
    size_t name_len = lbracket - str;
    if (name_len >= 64) name_len = 63;
    strncpy(arr_name, str, name_len);
    arr_name[name_len] = '\0';
    const char* rbracket = strchr(lbracket, ']');
    if (!rbracket) return 0;
    size_t idx_len = rbracket - lbracket - 1;
    if (idx_len >= 64) idx_len = 63;
    strncpy(idx_name, lbracket + 1, idx_len);
    idx_name[idx_len] = '\0';
    return 1;
}

/* ---------- 汇编输出 ---------- */
static FILE* out;

static void emit(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    fprintf(out, "\n");
    va_end(args);
}

/* 生成数组元素的加载或存储 */
static void handle_array_access(const char* access_str, int is_store) {
    char arr_name[64], idx_name[64];
    if (!parse_array_access(access_str, arr_name, idx_name)) {
        emit("    # error: invalid array access %s", access_str);
        return;
    }
    VarInfo* arr_info = get_var_info(arr_name);
    if (!arr_info || arr_info->array_size <= 0) {
        int off = get_offset(arr_name);
        if (!is_store)
            emit("    movl %d(%%rbp), %%eax", off);
        else
            emit("    movl %%eax, %d(%%rbp)", off);
        return;
    }
    int base_off = arr_info->offset;
    if (isdigit(idx_name[0]) || (idx_name[0] == '-' && isdigit(idx_name[1])))
        emit("    movl $%s, %%ecx", idx_name);
    else {
        int idx_off = get_offset(idx_name);
        emit("    movl %d(%%rbp), %%ecx", idx_off);
    }
    emit("    shlq $2, %%rcx");
    if (is_store)
        emit("    movl %%eax, %d(%%rbp,%%rcx)", base_off);
    else
        emit("    movl %d(%%rbp,%%rcx), %%eax", base_off);
}

/* 生成普通赋值 */
static void gen_assign(const char* src, const char* dst) {
    if (!dst) return;
    int dst_off = get_offset(dst);

    // 立即数：直接 mov 到栈，不经过 eax
    if (src && (isdigit(src[0]) || (src[0] == '-' && isdigit(src[1])))) {
        emit("    movl $%s, %d(%%rbp)", src, dst_off);
    }
    // 普通变量：栈 -> eax -> 栈（不可省略，保留必要拷贝）
    else if (src) {
        int src_off = get_offset(src);
        emit("    movl %d(%%rbp), %%eax", src_off);
        emit("    movl %%eax, %d(%%rbp)", dst_off);
    }
    // src 为空，赋值 0
    else {
        emit("    movl $0, %d(%%rbp)", dst_off);
    }
}

/* 生成二元运算 */
static void gen_binary_op(IROp op, const char* arg1, const char* arg2, const char* result) {
    if (isdigit(arg1[0]) || (arg1[0] == '-' && isdigit(arg1[1])))
        emit("    movl $%s, %%eax", arg1);
    else {
        int off1 = get_offset(arg1);
        emit("    movl %d(%%rbp), %%eax", off1);
    }
    if (isdigit(arg2[0]) || (arg2[0] == '-' && isdigit(arg2[1])))
        emit("    movl $%s, %%ecx", arg2);
    else {
        int off2 = get_offset(arg2);
        emit("    movl %d(%%rbp), %%ecx", off2);
    }

    switch (op) {
    case IR_ADD: emit("    addl %%ecx, %%eax"); break;
    case IR_SUB: emit("    subl %%ecx, %%eax"); break;
    case IR_MUL: emit("    imull %%ecx, %%eax"); break;
    case IR_DIV: emit("    cltd"); emit("    idivl %%ecx"); break;
    case IR_MOD: emit("    cltd"); emit("    idivl %%ecx"); emit("    movl %%edx, %%eax"); break;
    case IR_LT:  emit("    cmpl %%ecx, %%eax"); emit("    setl %%al"); emit("    movzbl %%al, %%eax"); break;
    case IR_LE:  emit("    cmpl %%ecx, %%eax"); emit("    setle %%al"); emit("    movzbl %%al, %%eax"); break;
    case IR_GT:  emit("    cmpl %%ecx, %%eax"); emit("    setg %%al"); emit("    movzbl %%al, %%eax"); break;
    case IR_GE:  emit("    cmpl %%ecx, %%eax"); emit("    setge %%al"); emit("    movzbl %%al, %%eax"); break;
    case IR_EQ:  emit("    cmpl %%ecx, %%eax"); emit("    sete %%al"); emit("    movzbl %%al, %%eax"); break;
    case IR_NE:  emit("    cmpl %%ecx, %%eax"); emit("    setne %%al"); emit("    movzbl %%al, %%eax"); break;
    case IR_AND: {
        static int and_label = 0;
        int lbl = and_label++;
        emit("    cmpl $0, %%eax");
        emit("    je  .L_and_false_%d", lbl);
        emit("    cmpl $0, %%ecx");
        emit("    je  .L_and_false_%d", lbl);
        emit("    movl $1, %%eax");
        emit("    jmp .L_and_end_%d", lbl);
        emit(".L_and_false_%d:", lbl);
        emit("    movl $0, %%eax");
        emit(".L_and_end_%d:", lbl);
        break;
    }
    case IR_OR: {
        static int or_label = 0;
        int lbl = or_label++;
        emit("    cmpl $0, %%eax");
        emit("    jne .L_or_true_%d", lbl);
        emit("    cmpl $0, %%ecx");
        emit("    jne .L_or_true_%d", lbl);
        emit("    movl $0, %%eax");
        emit("    jmp .L_or_end_%d", lbl);
        emit(".L_or_true_%d:", lbl);
        emit("    movl $1, %%eax");
        emit(".L_or_end_%d:", lbl);
        break;
    }
    default:
        emit("    # unimplemented binary op %d", op);
        break;
    }

    if (result) {
        int off_res = get_offset(result);
        emit("    movl %%eax, %d(%%rbp)", off_res);
    }
}

static void gen_unary_not(const char* arg, const char* result) {
    if (isdigit(arg[0]) || (arg[0] == '-' && isdigit(arg[1])))
        emit("    movl $%s, %%eax", arg);
    else {
        int off = get_offset(arg);
        emit("    movl %d(%%rbp), %%eax", off);
    }
    emit("    cmpl $0, %%eax");
    emit("    sete %%al");
    emit("    movzbl %%al, %%eax");
    if (result) {
        int off_res = get_offset(result);
        emit("    movl %%eax, %d(%%rbp)", off_res);
    }
}

/* ---------- 主入口 ---------- */
void gen_assembly(IRCode* ir, const char* output_filename) {
    out = fopen(output_filename, "w");
    if (!out) {
        report_error(COMPILE_ERR_CODEGEN, "Cannot create output file %s", output_filename);
        return;
    }

    /* 第一遍扫描：收集所有变量、数组、参数，分配内存 */
    for (int i = 0; i < ir->count; i++) {
        IRQuad* q = &ir->quads[i];
        /* 处理 arg1 */
        if (q->arg1) {
            if (strchr(q->arg1, '[')) {
                char arr[64], idx[64];
                if (parse_array_access(q->arg1, arr, idx)) {
                    VarInfo* info = get_var_info(arr);
                    if (!info) alloc_array(arr, 64);
                    if (!isdigit(idx[0]) && idx[0] != '-')
                        get_offset(idx);
                }
                else {
                    if (!is_immediate(q->arg1))
                        get_offset(q->arg1);
                }
            }
            else {
                if (!is_immediate(q->arg1))
                    get_offset(q->arg1);
            }
        }
        /* 处理 arg2 */
        if (q->arg2) {
            if (strchr(q->arg2, '[')) {
                char arr[64], idx[64];
                if (parse_array_access(q->arg2, arr, idx)) {
                    VarInfo* info = get_var_info(arr);
                    if (!info) alloc_array(arr, 64);
                    if (!isdigit(idx[0]) && idx[0] != '-')
                        get_offset(idx);
                }
                else {
                    if (!is_immediate(q->arg2))
                        get_offset(q->arg2);
                }
            }
            else {
                if (!is_immediate(q->arg2))
                    get_offset(q->arg2);
            }
        }
        /* 处理 result */
        if (q->result) {
            if (strchr(q->result, '[')) {
                char arr[64], idx[64];
                if (parse_array_access(q->result, arr, idx)) {
                    VarInfo* info = get_var_info(arr);
                    if (!info) alloc_array(arr, 64);
                    if (!isdigit(idx[0]) && idx[0] != '-')
                        get_offset(idx);
                }
                else {
                    if (!is_immediate(q->result))
                        get_offset(q->result);
                }
            }
            else {
                if (!is_immediate(q->result))
                    get_offset(q->result);
            }
        }
        /* 特别处理 IR_PARAM_LOAD 中的 result（形参变量） */
        if (q->op == IR_PARAM_LOAD && q->result) {
            get_offset(q->result);
        }
    }

    emit(".section .text");
    emit(".global main");
    
    bool has_return = false;    // 记录是否生成过 return
    /* 第二遍：生成指令 */
    for (int i = 0; i < ir->count; i++) {
        IRQuad* q = &ir->quads[i];
        switch (q->op) {
        case IR_ASSIGN:
            if (q->arg1 && strchr(q->arg1, '[')) {
                handle_array_access(q->arg1, 0);
                if (q->result) {
                    int off = get_offset(q->result);
                    emit("    movl %%eax, %d(%%rbp)", off);
                }
            }
            else if (q->result && strchr(q->result, '[')) {
                if (q->arg1 && (isdigit(q->arg1[0]) || (q->arg1[0] == '-' && isdigit(q->arg1[1]))))
                    emit("    movl $%s, %%eax", q->arg1);
                else if (q->arg1) {
                    int off = get_offset(q->arg1);
                    emit("    movl %d(%%rbp), %%eax", off);
                }
                else
                    emit("    movl $0, %%eax");
                handle_array_access(q->result, 1);
            }
            else {
                gen_assign(q->arg1, q->result);
            }
            break;

        case IR_ADD: case IR_SUB: case IR_MUL: case IR_DIV: case IR_MOD:
        case IR_LT:  case IR_LE:  case IR_GT:  case IR_GE:  case IR_EQ: case IR_NE:
        case IR_AND: case IR_OR:
            gen_binary_op(q->op, q->arg1, q->arg2, q->result);
            break;

        case IR_NOT:
            gen_unary_not(q->arg1, q->result);
            break;

        case IR_LABEL:
            fprintf(out, "%s:\n", q->arg1);
            break;

        case IR_GOTO:
            emit("    jmp %s", q->result);
            break;

        case IR_IF_FALSE_GOTO: {
            // 步骤1：处理条件值（支持立即数、数组访问、普通变量）
            const char* cond = q->arg1;
            if (isdigit(cond[0]) || (cond[0] == '-' && isdigit(cond[1]))) {
                // 条件是立即数：直接加载到eax
                emit("    movl $%s, %%eax", cond);
            }
            else if (strchr(cond, '[')) {
                // 条件是数组访问：加载数组元素到eax
                handle_array_access(cond, 0);
            }
            else {
                // 条件是普通变量：从栈加载到eax
                int off_cond = get_offset(cond);
                emit("    movl %d(%%rbp), %%eax", off_cond);
            }
            // 步骤2：判断eax是否为0，为0则跳转到目标标签
            emit("    cmpl $0, %%eax");
            emit("    je %s", q->result);
            break;
        }

        case IR_PARAM:
            /* 收集参数值，最多6个 */
            if (param_count < 6) {
                param_values[param_count++] = strdup(q->arg1);
            }
            else {
                emit("    # warning: too many arguments, only first 6 used");
            }
            break;

        case IR_CALL: {
            /* 将参数按顺序放入寄存器 rdi, rsi, rdx, rcx, r8, r9 */
            const char* regs[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };
            for (int j = 0; j < param_count && j < 6; j++) {
                char* val = param_values[j];
                if (isdigit(val[0]) || (val[0] == '-' && isdigit(val[1]))) {
                    emit("    movq $%s, %%%s", val, regs[j]);
                }
                else {
                    int off = get_offset(val);
                    emit("    movl %d(%%rbp), %%eax", off);
                    emit("    movq %%rax, %%%s", regs[j]);
                }
                free(val);
                param_values[j] = NULL;
            }
            param_count = 0;
            emit("    call %s", q->arg1);
            if (q->result) {
                int off = get_offset(q->result);
                emit("    movl %%eax, %d(%%rbp)", off);
            }
            break;
        }

        case IR_PARAM_LOAD: {
            /* 从寄存器加载参数值到临时变量 */
            /* arg1 是寄存器索引字符串 "0"~"5"，result 是目标变量名 */
            int reg_idx = atoi(q->arg1);
            if (reg_idx < 0 || reg_idx > 5) {
                emit("    # error: invalid param load reg index %s", q->arg1);
                break;
            }
            const char* regs[] = { "edi", "esi", "edx", "ecx", "r8d", "r9d" };
            int off = get_offset(q->result);
            emit("    movl %%%s, %%eax", regs[reg_idx]);  /* 低32位 */
            emit("    movl %%eax, %d(%%rbp)", off);
            break;
        }

        case IR_RETURN:
            if (q->arg1) {
                if (isdigit(q->arg1[0]) || (q->arg1[0] == '-' && isdigit(q->arg1[1])))
                    emit("    movl $%s, %%eax", q->arg1);
                else {
                    int off = get_offset(q->arg1);
                    emit("    movl %d(%%rbp), %%eax", off);
                }
            }
            emit("    leave");
            emit("    ret");
            has_return = true;
            break;

        case IR_FUNC_ENTER:
            
            emit("%s:", q->arg1);   // 去掉重复的 .globl
            emit("    pushq %%rbp");
            emit("    movq %%rsp, %%rbp");
            {
                int stack_size = -next_offset;
                stack_size = (stack_size + 15) & ~15; // 仅保证16字节对齐
                if (stack_size < 0) stack_size = 0;    // 无局部变量则不分配栈
                if (stack_size > 0)
                    emit("    subq $%d, %%rsp", stack_size);
            }
            
            break;

        case IR_FUNC_EXIT:
            if (!has_return) {
                emit("    leave");
                emit("    ret");
            }
            break;

        default:
            emit("    # unsupported IR op %d", q->op);
            break;
        }
    }

    fclose(out);

    /* 释放变量表 */
    VarInfo* p = var_table;
    while (p) {
        VarInfo* next = p->next;
        free(p->name);
        free(p);
        p = next;
    }
    var_table = NULL;
    next_offset = -4;
}