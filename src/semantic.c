/**
 * @file semantic.c
 * @brief 语义分析（C语言子集）
 */

#include "common.h"

void init_symbol_table(SymbolTable *table) {
    table->head = NULL;
    table->current_scope = 0;
}

void enter_scope(SymbolTable *table) {
    table->current_scope++;
}

void exit_scope(SymbolTable *table) {
    // 删除当前作用域的所有符号
    while (table->head && table->head->scope_level == table->current_scope) {
        Symbol *tmp = table->head;
        table->head = tmp->next;
        free(tmp);
    }
    table->current_scope--;
}

int insert_symbol(SymbolTable *table, const char *name, DataType type, SymbolKind kind, int line) {
    // 检查当前作用域是否重复
    Symbol *p = table->head;
    while (p && p->scope_level == table->current_scope) {
        if (strcmp(p->name, name) == 0) {
            report_error(COMPILE_ERR_SEMANTIC, "Redefinition of '%s' at line %d", name, line);
            return 0;
        }
        p = p->next;
    }
    Symbol *sym = safe_malloc(sizeof(Symbol));
    strncpy(sym->name, name, MAX_NAME_LEN - 1);
    sym->name[MAX_NAME_LEN - 1] = '\0';
    sym->type = type;
    sym->kind = kind;
    sym->scope_level = table->current_scope;
    sym->line = line;
    sym->next = table->head;
    table->head = sym;
    return 1;
}

Symbol *lookup_symbol(SymbolTable *table, const char *name) {
    Symbol *p = table->head;
    while (p) {
        if (strcmp(p->name, name) == 0) return p;
        p = p->next;
    }
    return NULL;
}

Symbol *lookup_symbol_current_scope(SymbolTable *table, const char *name) {
    Symbol *p = table->head;
    while (p && p->scope_level == table->current_scope) {
        if (strcmp(p->name, name) == 0) return p;
        p = p->next;
    }
    return NULL;
}

void semantic_error(const char *message, int line) {
    report_error(COMPILE_ERR_SEMANTIC, "%s at line %d", message, line);
}

void print_symbol_table(SymbolTable *table) {
    printf("===== Symbol Table =====\n");
    for (Symbol *p = table->head; p; p = p->next) {
        printf("%s (scope=%d, kind=%d, type=%d, line=%d)\n",
               p->name, p->scope_level, p->kind, p->type, p->line);
    }
}

void free_symbol_table(SymbolTable *table) {
    Symbol *p = table->head;
    while (p) {
        Symbol *next = p->next;
        free(p);
        p = next;
    }
    table->head = NULL;
}

// 表达式类型分析
DataType analyze_expression(ASTNode *node, SymbolTable *table) {
    if (!node) return TYPE_UNKNOWN;
    switch (node->type) {
        case NODE_NUMBER:
            node->data_type = TYPE_INT;
            return TYPE_INT;
        case NODE_LVAL: {
            Symbol *sym = lookup_symbol(table, node->attr.name);
            if (!sym) {
                semantic_error("Undefined variable", node->line_no);
                node->data_type = TYPE_UNKNOWN;
                return TYPE_UNKNOWN;
            }
            node->data_type = sym->type;
            return sym->type;
        }
        case NODE_UNARY_EXPR: {
            DataType child_type = analyze_expression(node->child, table);
            if (child_type != TYPE_INT && child_type != TYPE_UNKNOWN) {
                semantic_error("Invalid operand type for unary expression", node->line_no);
            }
            node->data_type = TYPE_INT;
            return TYPE_INT;
        }
        case NODE_BINARY_EXPR: {
            DataType left_type = analyze_expression(node->child, table);
            DataType right_type = analyze_expression(node->child ? node->child->sibling : NULL, table);
            if ((left_type != TYPE_INT && left_type != TYPE_UNKNOWN) ||
                (right_type != TYPE_INT && right_type != TYPE_UNKNOWN)) {
                semantic_error("Type mismatch in binary expression", node->line_no);
            }
            node->data_type = TYPE_INT;
            return TYPE_INT;
        }
        case NODE_FUNC_CALL: {
            Symbol *sym = lookup_symbol(table, node->attr.name);
            if (!sym || sym->kind != SYMBOL_FUNCTION) {
                semantic_error("Undefined function", node->line_no);
                node->data_type = TYPE_UNKNOWN;
                return TYPE_UNKNOWN;
            }
            // 检查参数（略）
            node->data_type = sym->type;
            return sym->type;
        }
        default:
            node->data_type = TYPE_UNKNOWN;
            return TYPE_UNKNOWN;
    }
}

// 语句分析
static void analyze_statement(ASTNode *node, SymbolTable *table, DataType func_ret_type, bool *has_return) {
    if (!node) return;
    switch (node->type) {
        case NODE_BLOCK:
            enter_scope(table);
            for (ASTNode *p = node->child; p; p = p->sibling)
                analyze_statement(p, table, func_ret_type, has_return);
            exit_scope(table);
            break;
        case NODE_VAR_DECL:
            // 插入符号
            insert_symbol(table, node->attr.name, node->data_type, SYMBOL_VARIABLE, node->line_no);
            if (node->child) {
                ASTNode *def = node->child;
                analyze_expression(def->child, table);
            }
            break;
        case NODE_ASSIGN_STMT: {
            ASTNode *lval = node->child;
            ASTNode *rval = lval ? lval->sibling : NULL;
            DataType ltype = analyze_expression(lval, table);
            DataType rtype = analyze_expression(rval, table);
            if (ltype != TYPE_INT || rtype != TYPE_INT)
                semantic_error("Assignment type mismatch", node->line_no);
            break;
        }
        case NODE_IF_STMT: {
            ASTNode *cond = node->child;
            ASTNode *then_stmt = cond ? cond->sibling : NULL;
            ASTNode *else_stmt = then_stmt ? then_stmt->sibling : NULL;
            DataType ctype = analyze_expression(cond, table);
            if (ctype != TYPE_INT) semantic_error("Condition must be integer", cond->line_no);
            analyze_statement(then_stmt, table, func_ret_type, has_return);
            analyze_statement(else_stmt, table, func_ret_type, has_return);
            break;
        }
        case NODE_WHILE_STMT: {
            ASTNode *cond = node->child;
            ASTNode *body = cond ? cond->sibling : NULL;
            analyze_expression(cond, table);
            analyze_statement(body, table, func_ret_type, has_return);
            break;
        }
        case NODE_RETURN_STMT: {
            ASTNode *ret_expr = node->child;
            if (func_ret_type == TYPE_VOID) {
                if (ret_expr) semantic_error("Void function cannot return a value", node->line_no);
            } else {
                if (!ret_expr) semantic_error("Non-void function must return a value", node->line_no);
                else {
                    DataType rtype = analyze_expression(ret_expr, table);
                    if (rtype != func_ret_type && rtype != TYPE_UNKNOWN)
                        semantic_error("Return type mismatch", node->line_no);
                }
            }
            *has_return = true;
            break;
        }
        case NODE_EXPR_STMT:
            if (node->child) analyze_expression(node->child, table);
            break;
        default:
            break;
    }
    // 处理兄弟节点
    analyze_statement(node->sibling, table, func_ret_type, has_return);
}

static void analyze_function(ASTNode *func, SymbolTable *table) {
    DataType ret_type = func->data_type;
    const char *func_name = func->attr.name;
    // 插入函数符号
    insert_symbol(table, func_name, ret_type, SYMBOL_FUNCTION, func->line_no);
    enter_scope(table);
    // 处理参数
    ASTNode *params = func->child;
    ASTNode *body = params;
    while (body && body->sibling) body = body->sibling;  // 最后一个sibling是函数体
    for (ASTNode *p = params; p && p != body; p = p->sibling) {
        if (p->type == NODE_FUNC_FPARAM) {
            insert_symbol(table, p->attr.name, p->data_type, SYMBOL_PARAMETER, p->line_no);
        }
    }
    bool has_return = (ret_type == TYPE_VOID);  // void函数允许无return
    analyze_statement(body, table, ret_type, &has_return);
    if (ret_type != TYPE_VOID && !has_return) {
        semantic_error("Non-void function missing return statement", func->line_no);
    }
    exit_scope(table);
}

int semantic_analyze(ASTNode *root, SymbolTable *table) {
    if (!root) return 1;
    init_symbol_table(table);
    // 预声明一些库函数？可选
    // 遍历所有顶层节点
    for (ASTNode *p = root->child; p; p = p->sibling) {
        if (p->type == NODE_FUNC_DEF) {
            analyze_function(p, table);
        } else {
            // 全局变量声明
            analyze_statement(p, table, TYPE_UNKNOWN, NULL);
        }
    }
    if (g_state.error_count > 0) return 0;
    return 1;
}