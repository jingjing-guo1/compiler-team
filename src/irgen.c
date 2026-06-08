/**
 * @file irgen.c
 * @brief 中间代码生成（C语言子集）
 */

#include "common.h"

static IRCode *ir;
static int temp_counter = 0;
static int label_counter = 0;

char *new_temp(void) {
    char *buf = safe_malloc(16);
    sprintf(buf, "t%d", temp_counter++);
    return buf;
}

static char *new_label(void) {
    char *buf = safe_malloc(16);
    sprintf(buf, "L%d", label_counter++);
    return buf;
}

static void add_quad(IROp op, const char *arg1, const char *arg2, const char *result) {
    if (ir->count >= ir->capacity) {
        ir->capacity = ir->capacity == 0 ? 64 : ir->capacity * 2;
        ir->quads = safe_realloc(ir->quads, ir->capacity * sizeof(IRQuad));
    }
    IRQuad *q = &ir->quads[ir->count++];
    q->op = op;
    q->arg1 = arg1 ? strdup(arg1) : NULL;
    q->arg2 = arg2 ? strdup(arg2) : NULL;
    q->result = result ? strdup(result) : NULL;
}

static char *gen_expr(ASTNode *node, SymbolTable *table);
static void gen_stmt(ASTNode *node, SymbolTable *table, char *break_label, char *continue_label);
static char *gen_condition(ASTNode *node, SymbolTable *table, char *true_label, char *false_label);

static char *gen_expr(ASTNode *node, SymbolTable *table) {
    if (!node) return NULL;
    switch (node->type) {
        case NODE_NUMBER: {
            char *tmp = new_temp();
            add_quad(IR_ASSIGN, NULL, NULL, tmp);
            // 直接生成立即数，我们稍后需要特殊处理。简化：生成一条"CONST"伪指令，codegen处理。
            // 这里用 IR_ASSIGN 带立即数比较麻烦，改为生成一条特殊的IR，或者用arg1存数值。
            // 简便方法：arg1存放数字字符串
            char num[32];
            sprintf(num, "%d", node->attr.int_val);
            add_quad(IR_ASSIGN, num, NULL, tmp);
            return tmp;
        }
        case NODE_LVAL: {
            char *tmp = new_temp();
            add_quad(IR_ASSIGN, node->attr.name, NULL, tmp);
            return tmp;
        }
        case NODE_UNARY_EXPR: {
            char *operand = gen_expr(node->child, table);
            char *tmp = new_temp();
            IROp op = (node->attr.op == '-') ? IR_SUB : IR_NOT;  // 一元负用0 - operand实现
            if (op == IR_SUB) {
                add_quad(IR_ASSIGN, "0", NULL, tmp);
                add_quad(IR_SUB, tmp, operand, tmp);
            } else {
                add_quad(IR_NOT, operand, NULL, tmp);
            }
            return tmp;
        }
        case NODE_BINARY_EXPR: {
            char *left = gen_expr(node->child, table);
            char *right = gen_expr(node->child ? node->child->sibling : NULL, table);
            char *tmp = new_temp();
            IROp op;
            switch (node->attr.op) {
                case '+': op = IR_ADD; break;
                case '-': op = IR_SUB; break;
                case '*': op = IR_MUL; break;
                case '/': op = IR_DIV; break;
                case '%': op = IR_MOD; break;
                case '<': op = IR_LT; break;
                case '>': op = IR_GT; break;
                case 'l': op = IR_LE; break;   // <=
                case 'g': op = IR_GE; break;   // >=
                case '=': op = IR_EQ; break;
                case '!': op = IR_NE; break;
                case '&': op = IR_AND; break;
                case '|': op = IR_OR; break;
                default: op = IR_ADD;
            }
            add_quad(op, left, right, tmp);
            return tmp;
        }
        case NODE_FUNC_CALL: {
            char *result = new_temp();
            add_quad(IR_CALL, node->attr.name, NULL, result);
            // 参数传递简化：实际需要逐个压栈，这里省略，只生成call
            return result;
        }
        default:
            return NULL;
    }
}

static char *gen_condition(ASTNode *node, SymbolTable *table, char *true_label, char *false_label) {
    if (node->type == NODE_BINARY_EXPR && (node->attr.op == '<' || node->attr.op == '>' || node->attr.op == 'l' || node->attr.op == 'g' ||
                                           node->attr.op == '=' || node->attr.op == '!' || node->attr.op == '&' || node->attr.op == '|')) {
        char *left = gen_expr(node->child, table);
        char *right = gen_expr(node->child->sibling, table);
        IROp op;
        switch (node->attr.op) {
            case '<': op = IR_LT; break;
            case '>': op = IR_GT; break;
            case 'l': op = IR_LE; break;
            case 'g': op = IR_GE; break;
            case '=': op = IR_EQ; break;
            case '!': op = IR_NE; break;
            case '&': op = IR_AND; break;
            case '|': op = IR_OR; break;
            default: op = IR_EQ;
        }
        // 生成类似 if (left op right) goto true else goto false
        char *tmp = new_temp();
        add_quad(op, left, right, tmp);
        add_quad(IR_IF_FALSE_GOTO, tmp, NULL, false_label);
        add_quad(IR_GOTO, NULL, NULL, true_label);
        return NULL;
    } else {
        char *cond = gen_expr(node, table);
        add_quad(IR_IF_FALSE_GOTO, cond, NULL, false_label);
        add_quad(IR_GOTO, NULL, NULL, true_label);
        return NULL;
    }
}

static void gen_stmt(ASTNode *node, SymbolTable *table, char *break_label, char *continue_label) {
    if (!node) return;
    switch (node->type) {
        case NODE_BLOCK:
            for (ASTNode *p = node->child; p; p = p->sibling)
                gen_stmt(p, table, break_label, continue_label);
            break;
        case NODE_VAR_DECL:
            // 声明不生成代码，除非有初始化
            if (node->child) {
                ASTNode *def = node->child;
                char *init_val = gen_expr(def->child, table);
                add_quad(IR_ASSIGN, init_val, NULL, def->attr.name);
            }
            break;
        case NODE_ASSIGN_STMT: {
            ASTNode *lval = node->child;
            ASTNode *rval = lval->sibling;
            char *r = gen_expr(rval, table);
            add_quad(IR_ASSIGN, r, NULL, lval->attr.name);
            break;
        }
        case NODE_IF_STMT: {
            ASTNode *cond = node->child;
            ASTNode *then_stmt = cond->sibling;
            ASTNode *else_stmt = then_stmt ? then_stmt->sibling : NULL;
            char *else_label = new_label();
            char *end_label = new_label();
            gen_condition(cond, table, else_label, else_label); // 条件为假跳转
            gen_stmt(then_stmt, table, break_label, continue_label);
            add_quad(IR_GOTO, NULL, NULL, end_label);
            add_quad(IR_LABEL, else_label, NULL, NULL);
            gen_stmt(else_stmt, table, break_label, continue_label);
            add_quad(IR_LABEL, end_label, NULL, NULL);
            break;
        }
        case NODE_WHILE_STMT: {
            ASTNode *cond = node->child;
            ASTNode *body = cond->sibling;
            char *loop_start = new_label();
            char *loop_end = new_label();
            add_quad(IR_LABEL, loop_start, NULL, NULL);
            gen_condition(cond, table, loop_start, loop_end); // 条件假跳出
            // 循环体
            gen_stmt(body, table, loop_end, loop_start);
            add_quad(IR_GOTO, NULL, NULL, loop_start);
            add_quad(IR_LABEL, loop_end, NULL, NULL);
            break;
        }
        case NODE_RETURN_STMT: {
            if (node->child) {
                char *ret_val = gen_expr(node->child, table);
                add_quad(IR_RETURN, ret_val, NULL, NULL);
            } else {
                add_quad(IR_RETURN, NULL, NULL, NULL);
            }
            break;
        }
        case NODE_EXPR_STMT:
            if (node->child) gen_expr(node->child, table);
            break;
        default:
            break;
    }
    gen_stmt(node->sibling, table, break_label, continue_label);
}

IRCode *gen_ir(ASTNode *root) {
    ir = safe_malloc(sizeof(IRCode));
    ir->quads = NULL;
    ir->count = 0;
    ir->capacity = 0;
    temp_counter = 0;
    label_counter = 0;

    // 遍历所有函数定义
    for (ASTNode *p = root->child; p; p = p->sibling) {
        if (p->type == NODE_FUNC_DEF) {
            add_quad(IR_FUNC_ENTER, p->attr.name, NULL, NULL);
            // 函数体
            ASTNode *body = p->child;
            while (body && body->sibling) body = body->sibling;
            gen_stmt(body, NULL, NULL, NULL);
            add_quad(IR_FUNC_EXIT, NULL, NULL, NULL);
        }
    }
    return ir;
}