/**
 * @file irgen.c
 * @brief 中间代码生成（C语言子集）- 支持表达式短路求值
 * @author 成员5
 */

#include "common.h"

static IRCode* ir;
static int temp_counter = 0;
static int label_counter = 0;

/* 生成新的临时变量名 */
char* new_temp(void) {
    char* buf = safe_malloc(16);
    sprintf(buf, "t%d", temp_counter++);
    return buf;
}

/* 生成新的标签名 */
static char* new_label(void) {
    char* buf = safe_malloc(16);
    sprintf(buf, "L%d", label_counter++);
    return buf;
}

/* 添加一条四元式到中间代码 */
static void add_quad(IROp op, const char* arg1, const char* arg2, const char* result) {
    if (ir->count >= ir->capacity) {
        ir->capacity = ir->capacity == 0 ? 64 : ir->capacity * 2;
        ir->quads = safe_realloc(ir->quads, ir->capacity * sizeof(IRQuad));
    }
    IRQuad* q = &ir->quads[ir->count++];
    q->op = op;
    q->arg1 = arg1 ? strdup(arg1) : NULL;
    q->arg2 = arg2 ? strdup(arg2) : NULL;
    q->result = result ? strdup(result) : NULL;
}

/* 前向声明 */
static char* gen_expr(ASTNode* node);
static void gen_stmt(ASTNode* node, char* break_label, char* continue_label);

/* ---------- 条件跳转代码生成（短路求值核心） ---------- */
static void gen_condition(ASTNode* node, char* true_label, char* false_label) {
    if (!node) return;
    // 处理常量整数条件
    if (node->type == NODE_NUMBER) {
        if (node->attr.int_val != 0)
            add_quad(IR_GOTO, NULL, NULL, true_label);
        else
            add_quad(IR_GOTO, NULL, NULL, false_label);
        return;
    }

    if (node->type == NODE_BINARY_EXPR && node->attr.op == '&') {
        /* 逻辑与：左边为假则直接跳转 false_label，否则继续右边 */
        char* mid_label = new_label();
        gen_condition(node->child, mid_label, false_label);
        add_quad(IR_LABEL, mid_label, NULL, NULL);
        gen_condition(node->child->sibling, true_label, false_label);
    }
    else if (node->type == NODE_BINARY_EXPR && node->attr.op == '|') {
        /* 逻辑或：左边为真则直接跳转 true_label，否则继续右边 */
        char* mid_label = new_label();
        gen_condition(node->child, true_label, mid_label);
        add_quad(IR_LABEL, mid_label, NULL, NULL);
        gen_condition(node->child->sibling, true_label, false_label);
    }
    else {
        /* 普通条件：计算表达式值，若为假则跳转 false_label */
        char* cond = gen_expr(node);
        add_quad(IR_IF_FALSE_GOTO, cond, NULL, false_label);
    }
}

/* ---------- 表达式生成（含短路） ---------- */
static char* gen_expr(ASTNode* node) {
    if (!node) return NULL;

    switch (node->type) {
    case NODE_NUMBER: {
        char* num = safe_malloc(32);
        sprintf(num, "%d", node->attr.int_val);
        return num;
    }

    case NODE_LVAL: {
        char* name = strdup(node->attr.name);
        return name;
        
    }

    case NODE_ARRAY_ACCESS: {
        ASTNode* index = node->child;
        char* idx = gen_expr(index);
        char* tmp = new_temp();
        char arr_access[128];
        sprintf(arr_access, "%s[%s]", node->attr.name, idx);
        add_quad(IR_ASSIGN, arr_access, NULL, tmp);
        return tmp;
    }

    case NODE_UNARY_EXPR: {
        char* operand = gen_expr(node->child);
        char* tmp = new_temp();
        if (node->attr.op == '-') {
            char* zero = new_temp();
            add_quad(IR_ASSIGN, "0", NULL, zero);
            add_quad(IR_SUB, zero, operand, tmp);
        }
        else if (node->attr.op == '!') {
            add_quad(IR_NOT, operand, NULL, tmp);
        }
        return tmp;
    }

    case NODE_BINARY_EXPR: {
        char op = node->attr.op;
        /* 处理逻辑与 && 和逻辑或 || 的短路求值（表达式上下文） */
        if (op == '&' || op == '|') {
            char* tmp = new_temp();
            char* true_label = new_label();
            char* false_label = new_label();
            char* end_label = new_label();

            if (op == '&') {
                /* 左假 -> 结果为0，左真 -> 结果为右表达式值 */
                gen_condition(node->child, true_label, false_label);
                add_quad(IR_LABEL, true_label, NULL, NULL);
                char* right_val = gen_expr(node->child->sibling);
                add_quad(IR_ASSIGN, right_val, NULL, tmp);
                add_quad(IR_GOTO, NULL, NULL, end_label);
                add_quad(IR_LABEL, false_label, NULL, NULL);
                add_quad(IR_ASSIGN, "0", NULL, tmp);
            }
            else { /* op == '|' */
                /* 左真 -> 结果为1，左假 -> 结果为右表达式值 */
                gen_condition(node->child, true_label, false_label);
                add_quad(IR_LABEL, true_label, NULL, NULL);
                add_quad(IR_ASSIGN, "1", NULL, tmp);
                add_quad(IR_GOTO, NULL, NULL, end_label);
                add_quad(IR_LABEL, false_label, NULL, NULL);
                char* right_val = gen_expr(node->child->sibling);
                add_quad(IR_ASSIGN, right_val, NULL, tmp);
            }
            add_quad(IR_LABEL, end_label, NULL, NULL);
            return tmp;
        }

        /* 普通二元运算（算术、关系、相等） */
        char* left = gen_expr(node->child);
        char* right = gen_expr(node->child->sibling);
        char* tmp = new_temp();

        IROp irop;
        switch (op) {
        case '+': irop = IR_ADD; break;
        case '-': irop = IR_SUB; break;
        case '*': irop = IR_MUL; break;
        case '/': irop = IR_DIV; break;
        case '%': irop = IR_MOD; break;
        case '<': irop = IR_LT; break;
        case '>': irop = IR_GT; break;
        case 'l': irop = IR_LE; break;
        case 'g': irop = IR_GE; break;
        case '=': irop = IR_EQ; break;
        case '!': irop = IR_NE; break;
        default:
            report_error(COMPILE_ERR_IRGEN, "Unknown operator: %c", op);
            irop = IR_ADD;
        }
        add_quad(irop, left, right, tmp);
        return tmp;
    }

    case NODE_FUNC_CALL: {
        ASTNode* func_name = node;
        ASTNode* args = node->child;
        int arg_count = 0;
        char* arg_temps[16];
        for (ASTNode* arg = args; arg; arg = arg->sibling) {
            arg_temps[arg_count++] = gen_expr(arg);
        }
        for (int i = 0; i < arg_count; i++) {
            add_quad(IR_PARAM, arg_temps[i], NULL, NULL);
        }
        char* result = new_temp();
        add_quad(IR_CALL, func_name->attr.name, NULL, result);
        return result;
    }

    case NODE_ASSIGN_STMT: {
        ASTNode* lval = node->child;
        ASTNode* rval = lval->sibling;
        char* r = gen_expr(rval);
        if (lval->type == NODE_ARRAY_ACCESS) {
            ASTNode* index = lval->child;
            char* idx = gen_expr(index);
            char arr_access[128];
            sprintf(arr_access, "%s[%s]", lval->attr.name, idx);
            add_quad(IR_ASSIGN, r, NULL, arr_access);
        }
        else {
            add_quad(IR_ASSIGN, r, NULL, lval->attr.name);
        }
        return r;
    }

    default:
        report_error(COMPILE_ERR_IRGEN, "Unknown expression type: %d", node->type);
        return NULL;
    }
}

/* ---------- 语句生成 ---------- */
static void gen_stmt(ASTNode* node, char* break_label, char* continue_label) {
    if (!node) return;

    switch (node->type) {
    case NODE_BLOCK: {
        for (ASTNode* p = node->child; p; p = p->sibling)
            gen_stmt(p, break_label, continue_label);
        break;
    }

    case NODE_VAR_DECL:
    case NODE_CONST_DECL:
    case NODE_ARRAY_DECL: {
        for (ASTNode* def = node->child; def; def = def->sibling) {
            if ((def->type == NODE_VAR_DEF || def->type == NODE_CONST_DEF) && def->child) {
                char* init_val = gen_expr(def->child);
                add_quad(IR_ASSIGN, init_val, NULL, def->attr.name);
            }
        }
        break;
    }

    case NODE_ASSIGN_STMT: {
        ASTNode* lval = node->child;
        ASTNode* rval = lval->sibling;
        char* r = gen_expr(rval);
        if (lval->type == NODE_ARRAY_ACCESS) {
            ASTNode* index = lval->child;
            char* idx = gen_expr(index);
            char arr_access[128];
            sprintf(arr_access, "%s[%s]", lval->attr.name, idx);
            add_quad(IR_ASSIGN, r, NULL, arr_access);
        }
        else {
            add_quad(IR_ASSIGN, r, NULL, lval->attr.name);
        }
        break;
    }

    case NODE_IF_STMT: {
        ASTNode* cond = node->child;
        ASTNode* then_stmt = cond->sibling;
        ASTNode* else_stmt = then_stmt ? then_stmt->sibling : NULL;
        char* then_label = new_label();
        char* end_label = new_label();

        if (else_stmt) {
            char* else_label = new_label();
            gen_condition(cond, then_label, else_label);
            add_quad(IR_LABEL, then_label, NULL, NULL);
            gen_stmt(then_stmt, break_label, continue_label);
            add_quad(IR_GOTO, NULL, NULL, end_label);
            add_quad(IR_LABEL, else_label, NULL, NULL);
            gen_stmt(else_stmt, break_label, continue_label);
        }
        else {
            gen_condition(cond, then_label, end_label);
            add_quad(IR_LABEL, then_label, NULL, NULL);
            gen_stmt(then_stmt, break_label, continue_label);
        }
        add_quad(IR_LABEL, end_label, NULL, NULL);
        break;
    }

    case NODE_WHILE_STMT: {
        ASTNode* cond = node->child;
        ASTNode* body = cond->sibling;
        char* loop_start = new_label();
        char* loop_end = new_label();
        add_quad(IR_LABEL, loop_start, NULL, NULL);
        gen_condition(cond, loop_start, loop_end);
        gen_stmt(body, loop_end, loop_start);
        add_quad(IR_GOTO, NULL, NULL, loop_start);
        add_quad(IR_LABEL, loop_end, NULL, NULL);
        break;
    }

    case NODE_BREAK_STMT:
        if (break_label) add_quad(IR_GOTO, NULL, NULL, break_label);
        else report_error(COMPILE_ERR_IRGEN, "break statement not within loop");
        break;

    case NODE_CONTINUE_STMT:
        if (continue_label) add_quad(IR_GOTO, NULL, NULL, continue_label);
        else report_error(COMPILE_ERR_IRGEN, "continue statement not within loop");
        break;

    case NODE_RETURN_STMT:
        if (node->child) {
            char* ret_val = gen_expr(node->child);
            add_quad(IR_RETURN, ret_val, NULL, NULL);
        }
        else {
            add_quad(IR_RETURN, NULL, NULL, NULL);
        }
        break;

    case NODE_EXPR_STMT:
        if (node->child) gen_expr(node->child);
        break;

    default:
        report_error(COMPILE_ERR_IRGEN, "Unknown statement type: %d", node->type);
        break;
    }
}

/* ---------- 顶层IR生成 ---------- */
IRCode* gen_ir(ASTNode* root) {
    ir = safe_malloc(sizeof(IRCode));
    ir->quads = NULL;
    ir->count = 0;
    ir->capacity = 0;
    temp_counter = 0;
    label_counter = 0;

    if (!root) return ir;

    for (ASTNode* p = root->child; p; p = p->sibling) {
        if (p->type == NODE_FUNC_DEF) {
            add_quad(IR_FUNC_ENTER, p->attr.name, NULL, NULL);
            int param_idx = 0;
            ASTNode* param = p->child;
            ASTNode* body = NULL;
            while (param && param->type == NODE_FUNC_FPARAM) {
                char reg_idx_str[8];
                sprintf(reg_idx_str, "%d", param_idx);
                add_quad(IR_PARAM_LOAD, reg_idx_str, NULL, param->attr.name);
                param_idx++;
                param = param->sibling;
            }
            body = param;
            while (body && body->sibling) body = body->sibling;
            gen_stmt(body, NULL, NULL);
            add_quad(IR_FUNC_EXIT, NULL, NULL, NULL);
        }
    }
    for (int i = 0; i < ir->count; i++)
    {
        IRQuad* q = &ir->quads[i];

        printf(
            "%d: op=%d arg1=%s arg2=%s result=%s\n",
            i,
            q->op,
            q->arg1 ? q->arg1 : "NULL",
            q->arg2 ? q->arg2 : "NULL",
            q->result ? q->result : "NULL"
        );
    }
    
    return ir;
}