/**
 * @file utils.c
 * @brief 辅助函数：内存释放、调试输出
 */

#include "common.h"

void free_token_list(TokenList *list) {
    if (!list) return;
    for (int i = 0; i < list->count; i++) {
        free(list->tokens[i].value);
    }
    free(list->tokens);
    free(list);
}

void free_ast(ASTNode *node) {
    if (!node) return;
    for (int i = 0; i < 3; i++) free_ast(node->child[i]);
    free_ast(node->sibling);
    if (node->type == NODE_ID_EXPR && node->attr.id.name) {
        free(node->attr.id.name);
    }
    free(node);
}

void free_ir(IRCode *ir) {
    if (!ir) return;
    for (int i = 0; i < ir->count; i++) {
        free(ir->quads[i].arg1);
        free(ir->quads[i].arg2);
        free(ir->quads[i].result);
    }
    free(ir->quads);
    free(ir);
}

void dump_tokens(TokenList *list) {
    printf("====== TOKENS ======\n");
    for (int i = 0; i < list->count; i++) {
        Token *t = &list->tokens[i];
        printf("%4d: %2d", i, t->type);
        if (t->value) printf(" \"%s\"", t->value);
        if (t->line) printf(" (line %d)", t->line);
        printf("\n");
    }
}

void dump_ast(ASTNode *node, int indent) {
    if (!node) return;
    for (int i = 0; i < indent; i++) printf("  ");
    const char *typestr = "Unknown";
    switch (node->type) {
        case NODE_PROGRAM:      typestr = "Program"; break;
        case NODE_IF_STMT:      typestr = "IfStmt"; break;
        case NODE_REPEAT_STMT:  typestr = "RepeatStmt"; break;
        case NODE_ASSIGN_STMT:  typestr = "AssignStmt"; break;
        case NODE_READ_STMT:    typestr = "ReadStmt"; break;
        case NODE_WRITE_STMT:   typestr = "WriteStmt"; break;
        case NODE_OP_EXPR:      typestr = "OpExpr"; break;
        case NODE_CONST_EXPR:   typestr = "ConstExpr"; break;
        case NODE_ID_EXPR:      typestr = "IdExpr"; break;
        case NODE_ERROR:        typestr = "ErrorNode"; break;
    }
    printf("%s", typestr);
    if (node->type == NODE_OP_EXPR) printf("('%c')", node->attr.op);
    if (node->type == NODE_CONST_EXPR) printf("(%d)", node->attr.int_val);
    if (node->type == NODE_ID_EXPR) printf("(%s)", node->attr.id.name);
    printf("\n");
    for (int i = 0; i < 3; i++) dump_ast(node->child[i], indent + 1);
    dump_ast(node->sibling, indent);
}

void dump_ir(IRCode *ir) {
    printf("====== IR CODE ======\n");
    const char *opname[] = {
        "ADD", "SUB", "MUL", "DIV",
        "ASSIGN", "LABEL", "GOTO", "IF_FALSE",
        "EQ", "LT", "READ", "WRITE"
    };
    for (int i = 0; i < ir->count; i++) {
        IRQuad *q = &ir->quads[i];
        printf("%4d: %-8s", i, opname[q->op]);
        if (q->arg1) printf(" %s", q->arg1);
        if (q->arg2) printf(", %s", q->arg2);
        if (q->result) printf(" -> %s", q->result);
        printf("\n");
    }
}