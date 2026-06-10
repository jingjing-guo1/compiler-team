/**
 * @file semantic.c
 * @brief Semantic analysis for the supported C subset.
 */

#include "common.h"

static void analyze_statement(ASTNode* node, SymbolTable* table,
    DataType func_ret_type, bool* has_return,
    int loop_depth);

void init_symbol_table(SymbolTable* table) {
    if (!table) return;
    table->head = NULL;
    table->current_scope = 0;
}

void enter_scope(SymbolTable* table) {
    if (table) table->current_scope++;
}

void exit_scope(SymbolTable* table) {
    if (!table) return;
    while (table->head && table->head->scope_level == table->current_scope) {
        Symbol* tmp = table->head;
        table->head = tmp->next;
        free(tmp);
    }
    if (table->current_scope > 0) table->current_scope--;
}

int insert_symbol(SymbolTable* table, const char* name, DataType type,
    SymbolKind kind, int line) {
    return insert_symbol_array(table, name, type, kind, -1, line);
}

int insert_symbol_array(SymbolTable* table, const char* name, DataType type,
    SymbolKind kind, int size, int line) {
    if (!table || !name) return 0;

    Symbol* p = table->head;
    while (p && p->scope_level == table->current_scope) {
        if (strcmp(p->name, name) == 0) {
            report_error(COMPILE_ERR_SEMANTIC,
                "Redefinition of '%s' at line %d", name, line);
            return 0;
        }
        p = p->next;
    }

    Symbol* sym = safe_malloc(sizeof(Symbol));
    strncpy(sym->name, name, MAX_NAME_LEN - 1);
    sym->name[MAX_NAME_LEN - 1] = '\0';
    sym->type = type;
    sym->kind = kind;
    sym->size = size;
    sym->is_const = false;
    sym->param_count = -1;
    sym->scope_level = table->current_scope;
    sym->line = line;
    sym->next = table->head;
    table->head = sym;
    return 1;
}

Symbol* lookup_symbol(SymbolTable* table, const char* name) {
    if (!table || !name) return NULL;
    for (Symbol* p = table->head; p; p = p->next) {
        if (strcmp(p->name, name) == 0) return p;
    }
    return NULL;
}

Symbol* lookup_symbol_current_scope(SymbolTable* table, const char* name) {
    if (!table || !name) return NULL;
    Symbol* p = table->head;
    while (p && p->scope_level == table->current_scope) {
        if (strcmp(p->name, name) == 0) return p;
        p = p->next;
    }
    return NULL;
}

void semantic_error(const char* message, int line) {
    report_error(COMPILE_ERR_SEMANTIC, "%s at line %d", message, line);
}

void print_symbol_table(SymbolTable* table) {
    printf("===== Symbol Table =====\n");
    if (!table) return;
    for (Symbol* p = table->head; p; p = p->next) {
        printf("%s (scope=%d, kind=%d, type=%d, size=%d, const=%d, params=%d, line=%d)\n",
            p->name, p->scope_level, p->kind, p->type, p->size,
            p->is_const, p->param_count, p->line);
    }
}

void free_symbol_table(SymbolTable* table) {
    if (!table) return;
    Symbol* p = table->head;
    while (p) {
        Symbol* next = p->next;
        free(p);
        p = next;
    }
    table->head = NULL;
    table->current_scope = 0;
}

static bool type_is_invalid(DataType type) {
    return type == TYPE_ERROR || type == TYPE_UNKNOWN;
}

static DataType expression_error(ASTNode* node) {
    if (node) node->data_type = TYPE_ERROR;
    return TYPE_ERROR;
}

static DataType analyze_lvalue(ASTNode* node, SymbolTable* table,
    bool for_assignment) {
    if (!node) return TYPE_ERROR;
    if (node->type != NODE_LVAL && node->type != NODE_ARRAY_ACCESS) {
        report_error(COMPILE_ERR_SEMANTIC,
            "Left side of assignment is not assignable at line %d",
            node->line_no);
        return expression_error(node);
    }

    Symbol* sym = lookup_symbol(table, node->attr.name);
    if (!sym) {
        report_error(COMPILE_ERR_SEMANTIC,
            "Undefined variable '%s' at line %d",
            node->attr.name ? node->attr.name : "?", node->line_no);
        return expression_error(node);
    }
    if (sym->kind == SYMBOL_FUNCTION) {
        report_error(COMPILE_ERR_SEMANTIC,
            "'%s' is a function, not a variable at line %d",
            sym->name, node->line_no);
        return expression_error(node);
    }
    if (for_assignment && sym->is_const) {
        report_error(COMPILE_ERR_SEMANTIC,
            "Cannot assign to constant '%s' at line %d",
            sym->name, node->line_no);
        return expression_error(node);
    }

    if (node->type == NODE_ARRAY_ACCESS) {
        DataType index_type = analyze_expression(node->child, table);
        if (sym->size < 0) {
            report_error(COMPILE_ERR_SEMANTIC,
                "'%s' is not an array at line %d",
                sym->name, node->line_no);
            return expression_error(node);
        }
        if (!type_is_invalid(index_type) && index_type != TYPE_INT) {
            report_error(COMPILE_ERR_SEMANTIC,
                "Array index for '%s' must be int at line %d",
                sym->name, node->line_no);
            return expression_error(node);
        }
        if (type_is_invalid(index_type)) return expression_error(node);
    }
    else if (sym->size >= 0) {
        report_error(COMPILE_ERR_SEMANTIC,
            "Array '%s' requires an index at line %d",
            sym->name, node->line_no);
        return expression_error(node);
    }

    node->data_type = sym->type;
    return sym->type;
}

static DataType analyze_assignment(ASTNode* node, SymbolTable* table) {
    ASTNode* left = node ? node->child : NULL;
    ASTNode* right = left ? left->sibling : NULL;
    if (!left || !right) {
        semantic_error("Incomplete assignment", node ? node->line_no : 0);
        return expression_error(node);
    }

    DataType left_type = analyze_lvalue(left, table, true);
    DataType right_type = analyze_expression(right, table);
    if (type_is_invalid(left_type) || type_is_invalid(right_type)) {
        return expression_error(node);
    }
    if (left_type != right_type) {
        report_error(COMPILE_ERR_SEMANTIC,
            "Assignment type mismatch at line %d", node->line_no);
        return expression_error(node);
    }
    node->data_type = left_type;
    return left_type;
}

DataType analyze_expression(ASTNode* node, SymbolTable* table) {
    if (!node) return TYPE_ERROR;

    switch (node->type) {
    case NODE_NUMBER:
        node->data_type = TYPE_INT;
        return TYPE_INT;

    case NODE_LVAL:
    case NODE_ARRAY_ACCESS:
        return analyze_lvalue(node, table, false);

    case NODE_ASSIGN_STMT:
        return analyze_assignment(node, table);

    case NODE_UNARY_EXPR: {
        DataType operand_type = analyze_expression(node->child, table);
        if (type_is_invalid(operand_type)) return expression_error(node);
        if (operand_type != TYPE_INT) {
            report_error(COMPILE_ERR_SEMANTIC,
                "Invalid operand for unary '%c' at line %d",
                node->attr.op, node->line_no);
            return expression_error(node);
        }
        node->data_type = TYPE_INT;
        return TYPE_INT;
    }

    case NODE_BINARY_EXPR: {
        ASTNode* left = node->child;
        ASTNode* right = left ? left->sibling : NULL;
        if (!left || !right) {
            semantic_error("Incomplete binary expression", node->line_no);
            return expression_error(node);
        }
        DataType left_type = analyze_expression(left, table);
        DataType right_type = analyze_expression(right, table);
        if (type_is_invalid(left_type) || type_is_invalid(right_type)) {
            return expression_error(node);
        }
        if (left_type != TYPE_INT || right_type != TYPE_INT) {
            report_error(COMPILE_ERR_SEMANTIC,
                "Operator '%c' requires int operands at line %d",
                node->attr.op, node->line_no);
            return expression_error(node);
        }
        node->data_type = TYPE_INT;
        return TYPE_INT;
    }

    case NODE_FUNC_CALL: {
        int arg_count = 0;
        for (ASTNode* arg = node->child; arg; arg = arg->sibling) {
            DataType arg_type = analyze_expression(arg, table);
            if (!type_is_invalid(arg_type) && arg_type != TYPE_INT) {
                report_error(COMPILE_ERR_SEMANTIC,
                    "Argument %d of function '%s' must have type int at line %d",
                    arg_count + 1,
                    node->attr.name ? node->attr.name : "?",
                    arg->line_no);
            }
            arg_count++;
        }

        Symbol* sym = lookup_symbol(table, node->attr.name);
        if (!sym) {
            report_error(COMPILE_ERR_SEMANTIC,
                "Undefined function '%s' at line %d",
                node->attr.name ? node->attr.name : "?",
                node->line_no);
            return expression_error(node);
        }
        if (sym->kind != SYMBOL_FUNCTION) {
            report_error(COMPILE_ERR_SEMANTIC,
                "'%s' is not a function at line %d",
                sym->name, node->line_no);
            return expression_error(node);
        }
        if (sym->param_count >= 0 && sym->param_count != arg_count) {
            report_error(COMPILE_ERR_SEMANTIC,
                "Function '%s' expects %d argument(s), got %d at line %d",
                sym->name, sym->param_count, arg_count, node->line_no);
        }
        node->data_type = sym->type;
        return sym->type;
    }

    default:
        report_error(COMPILE_ERR_SEMANTIC,
            "Invalid expression node at line %d", node->line_no);
        return expression_error(node);
    }
}

static bool insert_declaration(ASTNode* node, SymbolTable* table) {
    if (!node || !node->attr.name) return false;
    if (node->data_type == TYPE_VOID) {
        report_error(COMPILE_ERR_SEMANTIC,
            "Variable '%s' cannot have type void at line %d",
            node->attr.name, node->line_no);
    }
    if (node->type == NODE_ARRAY_DECL && node->size <= 0) {
        report_error(COMPILE_ERR_SEMANTIC,
            "Array '%s' must have positive size at line %d",
            node->attr.name, node->line_no);
    }

    int inserted;
    if (node->type == NODE_ARRAY_DECL) {
        inserted = insert_symbol_array(table, node->attr.name, node->data_type,
            SYMBOL_VARIABLE, node->size, node->line_no);
    }
    else {
        inserted = insert_symbol(table, node->attr.name, node->data_type,
            SYMBOL_VARIABLE, node->line_no);
    }
    if (inserted && node->type == NODE_CONST_DECL) {
        Symbol* sym = lookup_symbol_current_scope(table, node->attr.name);
        if (sym) sym->is_const = true;
    }
    return inserted != 0;
}

static void analyze_initializer(ASTNode* decl, SymbolTable* table) {
    if (!decl || !decl->child) return;
    ASTNode* def = decl->child;
    DataType init_type = analyze_expression(def->child, table);
    def->data_type = init_type;
    if (!type_is_invalid(init_type) && init_type != decl->data_type) {
        report_error(COMPILE_ERR_SEMANTIC,
            "Initializer type mismatch for '%s' at line %d",
            decl->attr.name ? decl->attr.name : "?", decl->line_no);
    }
}

static void analyze_block(ASTNode* block, SymbolTable* table,
    DataType func_ret_type, bool* has_return,
    int loop_depth, bool create_scope) {
    if (!block) return;
    if (create_scope) enter_scope(table);
    for (ASTNode* item = block->child; item; item = item->sibling) {
        analyze_statement(item, table, func_ret_type, has_return, loop_depth);
    }
    if (create_scope) exit_scope(table);
}

static void analyze_statement(ASTNode* node, SymbolTable* table,
    DataType func_ret_type, bool* has_return,
    int loop_depth) {
    if (!node) return;

    switch (node->type) {
    case NODE_BLOCK:
        analyze_block(node, table, func_ret_type, has_return,
            loop_depth, true);
        break;

    case NODE_VAR_DECL:
    case NODE_CONST_DECL:
    case NODE_ARRAY_DECL:
        insert_declaration(node, table);
        analyze_initializer(node, table);
        break;

    case NODE_ASSIGN_STMT:
        analyze_assignment(node, table);
        break;

    case NODE_EXPR_STMT:
        if (node->child) analyze_expression(node->child, table);
        break;

    case NODE_IF_STMT: {
        ASTNode* cond = node->child;
        ASTNode* then_stmt = cond ? cond->sibling : NULL;
        ASTNode* else_stmt = then_stmt ? then_stmt->sibling : NULL;
        DataType cond_type = analyze_expression(cond, table);
        if (!type_is_invalid(cond_type) && cond_type != TYPE_INT) {
            semantic_error("If condition must have type int", node->line_no);
        }
        analyze_statement(then_stmt, table, func_ret_type,
            has_return, loop_depth);
        analyze_statement(else_stmt, table, func_ret_type,
            has_return, loop_depth);
        break;
    }

    case NODE_WHILE_STMT: {
        ASTNode* cond = node->child;
        ASTNode* body = cond ? cond->sibling : NULL;
        DataType cond_type = analyze_expression(cond, table);
        if (!type_is_invalid(cond_type) && cond_type != TYPE_INT) {
            semantic_error("While condition must have type int", node->line_no);
        }
        analyze_statement(body, table, func_ret_type,
            has_return, loop_depth + 1);
        break;
    }

    case NODE_BREAK_STMT:
        if (loop_depth == 0) {
            semantic_error("Break statement is only valid inside a loop",
                node->line_no);
        }
        break;

    case NODE_CONTINUE_STMT:
        if (loop_depth == 0) {
            semantic_error("Continue statement is only valid inside a loop",
                node->line_no);
        }
        break;

    case NODE_RETURN_STMT: {
        ASTNode* ret_expr = node->child;
        if (func_ret_type == TYPE_VOID) {
            if (ret_expr) {
                analyze_expression(ret_expr, table);
                semantic_error("Void function cannot return a value",
                    node->line_no);
            }
        }
        else if (func_ret_type == TYPE_INT) {
            if (!ret_expr) {
                semantic_error("Non-void function must return a value",
                    node->line_no);
            }
            else {
                DataType ret_type = analyze_expression(ret_expr, table);
                if (!type_is_invalid(ret_type) && ret_type != func_ret_type) {
                    semantic_error("Return type mismatch", node->line_no);
                }
            }
        }
        else {
            if (ret_expr) analyze_expression(ret_expr, table);
            semantic_error("Return statement outside a valid function",
                node->line_no);
        }
        if (has_return) *has_return = true;
        break;
    }

    default:
        report_error(COMPILE_ERR_SEMANTIC,
            "Invalid statement node at line %d", node->line_no);
        break;
    }
}

static ASTNode* function_body(ASTNode* func) {
    ASTNode* node = func ? func->child : NULL;
    while (node && node->sibling) node = node->sibling;
    return node && node->type == NODE_BLOCK ? node : NULL;
}

static int function_param_count(ASTNode* func) {
    int count = 0;
    ASTNode* body = function_body(func);
    for (ASTNode* p = func ? func->child : NULL; p&& p != body; p = p->sibling) {
        if (p->type == NODE_FUNC_FPARAM) count++;
    }
    return count;
}

static void collect_top_level(ASTNode* root, SymbolTable* table) {
    for (ASTNode* node = root ? root->child : NULL; node; node = node->sibling) {
        if (node->type == NODE_FUNC_DEF) {
            if (insert_symbol(table, node->attr.name, node->data_type,
                SYMBOL_FUNCTION, node->line_no)) {
                Symbol* sym = lookup_symbol_current_scope(table, node->attr.name);
                if (sym) sym->param_count = function_param_count(node);
            }
        }
        else if (node->type == NODE_VAR_DECL ||
            node->type == NODE_CONST_DECL ||
            node->type == NODE_ARRAY_DECL) {
            insert_declaration(node, table);
        }
        else {
            report_error(COMPILE_ERR_SEMANTIC,
                "Invalid top-level node at line %d", node->line_no);
        }
    }
}

static void analyze_function(ASTNode* func, SymbolTable* table) {
    ASTNode* body = function_body(func);
    enter_scope(table);

    for (ASTNode* param = func ? func->child : NULL;
        param&& param != body; param = param->sibling) {
        if (param->type != NODE_FUNC_FPARAM) continue;
        if (param->data_type == TYPE_VOID) {
            report_error(COMPILE_ERR_SEMANTIC,
                "Parameter '%s' cannot have type void at line %d",
                param->attr.name ? param->attr.name : "?",
                param->line_no);
        }
        insert_symbol(table, param->attr.name, param->data_type,
            SYMBOL_PARAMETER, param->line_no);
    }

    bool has_return = false;
    if (!body) {
        report_error(COMPILE_ERR_SEMANTIC,
            "Function '%s' has no body at line %d",
            func && func->attr.name ? func->attr.name : "?",
            func ? func->line_no : 0);
    }
    else {
        /* Parameters and declarations in the outer function block share a scope. */
        analyze_block(body, table, func->data_type, &has_return, 0, false);
    }
    if (func && func->data_type != TYPE_VOID && !has_return) {
        report_error(COMPILE_ERR_SEMANTIC,
            "Non-void function '%s' is missing a return statement at line %d",
            func->attr.name ? func->attr.name : "?", func->line_no);
    }
    exit_scope(table);
}

int semantic_analyze(ASTNode* root, SymbolTable* table) {
    int before_errors = g_state.error_count;
    init_symbol_table(table);
    if (!root) return 1;

    collect_top_level(root, table);

    for (ASTNode* node = root->child; node; node = node->sibling) {
        if (node->type == NODE_FUNC_DEF) {
            analyze_function(node, table);
        }
        else if (node->type == NODE_VAR_DECL ||
            node->type == NODE_CONST_DECL ||
            node->type == NODE_ARRAY_DECL) {
            analyze_initializer(node, table);
        }
    }
    return g_state.error_count == before_errors;
}