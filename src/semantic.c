/**
 * @file semantic.c
 * @brief 语义分析（支持多错误：未定义变量、类型不匹配等）
 */

#include "common.h"

static Symbol *sym_table = NULL;

void init_symbol_table(void) { sym_table = NULL; }

void insert_symbol(const char *name, int type) {
    Symbol *s = (Symbol*)safe_malloc(sizeof(Symbol));
    s->name = strdup(name);
    s->type = type;
    s->next = sym_table;
    sym_table = s;
}

int lookup_symbol(const char *name) {
    Symbol *p = sym_table;
    while (p) {
        if (strcmp(p->name, name) == 0) return p->type;
        p = p->next;
    }
    return 0; // 未找到
}

void free_symbol_table(void) {
    Symbol *p = sym_table;
    while (p) {
        Symbol *next = p->next;
        free(p->name);
        free(p);
        p = next;
    }
    sym_table = NULL;
}

/** 分析表达式，返回其类型，同时检查未定义变量 */
static int analyze_expr(ASTNode *node) {
    if (!node) return 0;
    switch (node->type) {
        case NODE_CONST_EXPR:
            node->data_type = 1; // int
            return 1;
        case NODE_ID_EXPR:
            if (lookup_symbol(node->attr.id.name) == 0) {
                report_error(COMPILE_ERR_SEMANTIC, "Undefined variable '%s' at line %d",
                             node->attr.id.name, node->line_no);
                node->data_type = 1; // 假设为 int，继续
            } else {
                node->data_type = 1;
            }
            return 1;
        case NODE_OP_EXPR:
        {
            int left_type = analyze_expr(node->child[0]);
            int right_type = analyze_expr(node->child[1]);
            if (left_type != right_type) {
                report_error(COMPILE_ERR_SEMANTIC, "Type mismatch at line %d", node->line_no);
            }
            node->data_type = 1;
            return 1;
        }
        default:
            node->data_type = 0;
            return 0;
    }
}

/** 分析语句 */
static void analyze_stmt(ASTNode *node) {
    if (!node) return;
    switch (node->type) {
        case NODE_ASSIGN_STMT:
            if (node->child[1]) analyze_expr(node->child[1]);
            if (node->child[0] && node->child[0]->type == NODE_ID_EXPR) {
                if (lookup_symbol(node->child[0]->attr.id.name) == 0) {
                    // 赋值给未定义变量：视为声明（可选），这里报错但继续
                    report_error(COMPILE_ERR_SEMANTIC, "Variable '%s' not declared before assignment at line %d",
                                 node->child[0]->attr.id.name, node->line_no);
                }
            }
            break;
        case NODE_IF_STMT:
            if (node->child[0]) analyze_expr(node->child[0]);
            analyze_stmt(node->child[1]);
            analyze_stmt(node->child[2]);
            break;
        case NODE_REPEAT_STMT:
            analyze_stmt(node->child[0]);
            if (node->child[1]) analyze_expr(node->child[1]);
            break;
        case NODE_READ_STMT:
        case NODE_WRITE_STMT:
            if (node->child[0]) analyze_expr(node->child[0]);
            break;
        default:
            break;
    }
    analyze_stmt(node->sibling);
}

void semantic_analyze(ASTNode *root) {
    if (!root) return;
    analyze_stmt(root);
}