/**
 * @file utils.c
 * @brief 辅助函数：内存释放、调试输出（适配C语言子集）
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
    free_ast(node->child);
    free_ast(node->sibling);
    if ((node->type == NODE_LVAL || node->type == NODE_FUNC_CALL ||
         node->type == NODE_VAR_DECL || node->type == NODE_VAR_DEF ||
         node->type == NODE_CONST_DECL || node->type == NODE_CONST_DEF ||
         node->type == NODE_ARRAY_DECL || node->type == NODE_ARRAY_ACCESS ||
         node->type == NODE_FUNC_DEF || node->type == NODE_FUNC_FPARAM) &&
        node->attr.name) {
        free(node->attr.name);
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

static const char *node_type_str(NodeType type) {
    switch (type) {
        case NODE_PROG: return "Prog";
        case NODE_COMP_UNIT: return "CompUnit";
        case NODE_VAR_DECL: return "VarDecl";
        case NODE_CONST_DECL: return "ConstDecl";
        case NODE_VAR_DEF: return "VarDef";
        case NODE_CONST_DEF: return "ConstDef";
        case NODE_FUNC_DEF: return "FuncDef";
        case NODE_FUNC_FPARAM: return "FuncFParam";
        case NODE_BLOCK: return "Block";
        case NODE_ASSIGN_STMT: return "AssignStmt";
        case NODE_IF_STMT: return "IfStmt";
        case NODE_WHILE_STMT: return "WhileStmt";
        case NODE_BREAK_STMT: return "BreakStmt";
        case NODE_CONTINUE_STMT: return "ContinueStmt";
        case NODE_RETURN_STMT: return "ReturnStmt";
        case NODE_EXPR_STMT: return "ExprStmt";
        case NODE_ARRAY_DECL: return "ArrayDecl";
        case NODE_ARRAY_ACCESS: return "ArrayAccess";
        case NODE_BINARY_EXPR: return "BinaryExpr";
        case NODE_UNARY_EXPR: return "UnaryExpr";
        case NODE_FUNC_CALL: return "FuncCall";
        case NODE_LVAL: return "LVal";
        case NODE_NUMBER: return "Number";
        default: return "Unknown";
    }
}

void dump_ast(ASTNode *node, int indent) {
    if (!node) return;
    for (int i = 0; i < indent; i++) printf("  ");
    printf("%s", node_type_str(node->type));
    if (node->type == NODE_NUMBER) printf("(%d)", node->attr.int_val);
    else if (node->type == NODE_LVAL || node->type == NODE_FUNC_CALL ||
             node->type == NODE_VAR_DECL || node->type == NODE_VAR_DEF ||
             node->type == NODE_CONST_DECL || node->type == NODE_CONST_DEF ||
             node->type == NODE_ARRAY_DECL || node->type == NODE_ARRAY_ACCESS ||
             node->type == NODE_FUNC_DEF || node->type == NODE_FUNC_FPARAM)
        printf("(%s)", node->attr.name ? node->attr.name : "?");
    else if (node->type == NODE_BINARY_EXPR || node->type == NODE_UNARY_EXPR)
        printf("('%c')", node->attr.op);
    if (node->type == NODE_ARRAY_DECL) printf(" [size: %d]", node->size);
    printf("\n");
    dump_ast(node->child, indent + 1);
    dump_ast(node->sibling, indent);
}

void dump_ir(IRCode *ir) {
    printf("====== IR CODE ======\n");
    const char *opname[] = {
        "ADD", "SUB", "MUL", "DIV", "MOD",
        "ASSIGN", "LT", "LE", "GT", "GE", "EQ", "NE",
        "AND", "OR", "NOT", "LABEL", "GOTO", "IF_FALSE",
        "RETURN", "CALL", "FUNC_ENTER", "FUNC_EXIT"
    };
    for (int i = 0; i < ir->count; i++) {
        IRQuad *q = &ir->quads[i];
        printf("%4d: %-12s", i, opname[q->op]);
        if (q->arg1) printf(" %s", q->arg1);
        if (q->arg2) printf(", %s", q->arg2);
        if (q->result) printf(" -> %s", q->result);
        printf("\n");
    }
}