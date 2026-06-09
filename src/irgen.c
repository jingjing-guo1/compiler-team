/**
 * @file irgen.c
 * @brief 中间代码生成（C语言子集）
 * @author 成员5
 */

#include "common.h"

static IRCode *ir;
static int temp_counter = 0;
static int label_counter = 0;

/* 生成新的临时变量名 */
char *new_temp(void) {
    char *buf = safe_malloc(16);
    sprintf(buf, "t%d", temp_counter++);
    return buf;
}

/* 生成新的标签名 */
static char *new_label(void) {
    char *buf = safe_malloc(16);
    sprintf(buf, "L%d", label_counter++);
    return buf;
}

/* 添加一条四元式到中间代码 */
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

/* 前向声明 */
static char *gen_expr(ASTNode *node);
static void gen_stmt(ASTNode *node, char *break_label, char *continue_label);

/**
 * @brief 生成表达式的中间代码
 * @param node 表达式节点
 * @return 存放表达式结果的临时变量名
 */
static char *gen_expr(ASTNode *node) {
    if (!node) return NULL;

    switch (node->type) {
        case NODE_NUMBER: {
            /* 数字常量：生成 t = num */
            char *tmp = new_temp();
            char num[32];
            sprintf(num, "%d", node->attr.int_val);
            add_quad(IR_ASSIGN, num, NULL, tmp);
            return tmp;
        }

        case NODE_LVAL: {
            /* 变量：生成 t = var */
            char *tmp = new_temp();
            add_quad(IR_ASSIGN, node->attr.name, NULL, tmp);
            return tmp;
        }

        case NODE_ARRAY_ACCESS: {
            /* 数组访问：生成 t = arr[index] */
            ASTNode *arr = node->child;
            ASTNode *index = arr->sibling;
            char *idx = gen_expr(index);
            char *tmp = new_temp();
            /* 使用特殊格式表示数组访问：arg1=数组名, arg2=下标, result=临时变量 */
            char arr_access[128];
            sprintf(arr_access, "%s[%s]", arr->attr.name, idx);
            add_quad(IR_ASSIGN, arr_access, NULL, tmp);
            return tmp;
        }

        case NODE_UNARY_EXPR: {
            /* 一元表达式：-expr 或 !expr */
            char *operand = gen_expr(node->child);
            char *tmp = new_temp();

            if (node->attr.op == '-') {
                /* 负号：t = 0 - operand */
                char *zero = new_temp();
                add_quad(IR_ASSIGN, "0", NULL, zero);
                add_quad(IR_SUB, zero, operand, tmp);
            } else if (node->attr.op == '!') {
                /* 逻辑非：t = !operand */
                add_quad(IR_NOT, operand, NULL, tmp);
            }
            return tmp;
        }

        case NODE_BINARY_EXPR: {
            /* 二元表达式：left op right */
            char *left = gen_expr(node->child);
            char *right = gen_expr(node->child->sibling);
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
                case '=': op = IR_EQ; break;   // ==
                case '!': op = IR_NE; break;   // !=
                case '&': op = IR_AND; break;  // &&
                case '|': op = IR_OR; break;   // ||
                default:
                    report_error(COMPILE_ERR_IRGEN, "Unknown operator: %c", node->attr.op);
                    op = IR_ADD;
            }
            add_quad(op, left, right, tmp);
            return tmp;
        }

        case NODE_FUNC_CALL: {
            /* 函数调用：t = call func(args) */
            ASTNode *func_name = node;
            ASTNode *args = node->child;

            /* 处理参数：逐个计算参数值 */
            int arg_count = 0;
            char *arg_temps[16];  // 最多支持16个参数

            for (ASTNode *arg = args; arg; arg = arg->sibling) {
                if (arg_count < 16) {
                    arg_temps[arg_count++] = gen_expr(arg);
                }
            }

            /* 生成函数调用 */
            char *result = new_temp();
            add_quad(IR_CALL, func_name->attr.name, NULL, result);

            return result;
        }

        case NODE_ASSIGN_STMT: {
            /* 赋值语句作为表达式（在表达式语句中）*/
            ASTNode *lval = node->child;
            ASTNode *rval = lval->sibling;
            char *r = gen_expr(rval);

            if (lval->type == NODE_ARRAY_ACCESS) {
                /* 数组元素赋值：arr[index] = r */
                ASTNode *arr = lval->child;
                ASTNode *index = arr->sibling;
                char *idx = gen_expr(index);
                char arr_access[128];
                sprintf(arr_access, "%s[%s]", arr->attr.name, idx);
                add_quad(IR_ASSIGN, r, NULL, arr_access);
            } else {
                /* 普通变量赋值 */
                add_quad(IR_ASSIGN, r, NULL, lval->attr.name);
            }
            return r;
        }

        default:
            report_error(COMPILE_ERR_IRGEN, "Unknown expression type: %d", node->type);
            return NULL;
    }
}

/**
 * @brief 生成条件跳转代码
 * @param node 条件表达式节点
 * @param true_label 条件为真时跳转的标签
 * @param false_label 条件为假时跳转的标签
 */
static void gen_condition(ASTNode *node, char *true_label, char *false_label) {
    if (!node) return;

    /* 对于逻辑与和逻辑或，需要短路求值 */
    if (node->type == NODE_BINARY_EXPR && node->attr.op == '&') {
        /* 逻辑与：如果左边为假，直接跳转到false；否则计算右边 */
        char *mid_label = new_label();
        gen_condition(node->child, mid_label, false_label);
        add_quad(IR_LABEL, mid_label, NULL, NULL);
        gen_condition(node->child->sibling, true_label, false_label);
    } else if (node->type == NODE_BINARY_EXPR && node->attr.op == '|') {
        /* 逻辑或：如果左边为真，直接跳转到true；否则计算右边 */
        char *mid_label = new_label();
        gen_condition(node->child, true_label, mid_label);
        add_quad(IR_LABEL, mid_label, NULL, NULL);
        gen_condition(node->child->sibling, true_label, false_label);
    } else {
        /* 普通条件：计算表达式值，然后根据结果跳转 */
        char *cond = gen_expr(node);
        add_quad(IR_IF_FALSE_GOTO, cond, NULL, false_label);
        add_quad(IR_GOTO, NULL, NULL, true_label);
    }
}

/**
 * @brief 生成语句的中间代码
 * @param node 语句节点
 * @param break_label break语句跳转的目标标签
 * @param continue_label continue语句跳转的目标标签
 */
static void gen_stmt(ASTNode *node, char *break_label, char *continue_label) {
    if (!node) return;

    switch (node->type) {
        case NODE_BLOCK: {
            /* 代码块：依次处理每个语句 */
            for (ASTNode *p = node->child; p; p = p->sibling) {
                gen_stmt(p, break_label, continue_label);
            }
            break;
        }

        case NODE_VAR_DECL:
        case NODE_CONST_DECL:
        case NODE_ARRAY_DECL: {
            /* 变量/常量/数组声明：如果有初始化则生成赋值代码 */
            for (ASTNode *def = node->child; def; def = def->sibling) {
                if (def->type == NODE_VAR_DEF || def->type == NODE_CONST_DEF) {
                    if (def->child) {
                        /* 有初始化表达式 */
                        char *init_val = gen_expr(def->child);
                        add_quad(IR_ASSIGN, init_val, NULL, def->attr.name);
                    }
                }
            }
            break;
        }

        case NODE_ASSIGN_STMT: {
            /* 赋值语句：left = right */
            ASTNode *lval = node->child;
            ASTNode *rval = lval->sibling;
            char *r = gen_expr(rval);

            if (lval->type == NODE_ARRAY_ACCESS) {
                /* 数组元素赋值：arr[index] = r */
                ASTNode *arr = lval->child;
                ASTNode *index = arr->sibling;
                char *idx = gen_expr(index);
                char arr_access[128];
                sprintf(arr_access, "%s[%s]", arr->attr.name, idx);
                add_quad(IR_ASSIGN, r, NULL, arr_access);
            } else {
                /* 普通变量赋值 */
                add_quad(IR_ASSIGN, r, NULL, lval->attr.name);
            }
            break;
        }

        case NODE_IF_STMT: {
            /* if语句：if (cond) then_stmt else else_stmt */
            ASTNode *cond = node->child;
            ASTNode *then_stmt = cond->sibling;
            ASTNode *else_stmt = then_stmt ? then_stmt->sibling : NULL;

            char *then_label = new_label();
            char *end_label = new_label();

            /* 生成条件判断：条件为真跳转到then，条件为假跳转到else/end */
            if (else_stmt) {
                /* 有else分支：条件为真跳转到then，条件为假跳转到else */
                char *else_label = new_label();
                gen_condition(cond, then_label, else_label);

                /* then块 */
                add_quad(IR_LABEL, then_label, NULL, NULL);
                gen_stmt(then_stmt, break_label, continue_label);
                add_quad(IR_GOTO, NULL, NULL, end_label);

                /* else块 */
                add_quad(IR_LABEL, else_label, NULL, NULL);
                gen_stmt(else_stmt, break_label, continue_label);
                add_quad(IR_GOTO, NULL, NULL, end_label);
            } else {
                /* 无else分支：条件为真执行then，条件为假跳转到end */
                gen_condition(cond, then_label, end_label);

                /* then块 */
                add_quad(IR_LABEL, then_label, NULL, NULL);
                gen_stmt(then_stmt, break_label, continue_label);
            }

            /* 结束标签 */
            add_quad(IR_LABEL, end_label, NULL, NULL);
            break;
        }

        case NODE_WHILE_STMT: {
            /* while语句：while (cond) body */
            ASTNode *cond = node->child;
            ASTNode *body = cond->sibling;

            char *loop_start = new_label();
            char *loop_end = new_label();

            /* 循环开始标签 */
            add_quad(IR_LABEL, loop_start, NULL, NULL);

            /* 条件判断：条件为假则跳出循环 */
            gen_condition(cond, loop_start, loop_end);

            /* 循环体 */
            gen_stmt(body, loop_end, loop_start);

            /* 跳回循环开始 */
            add_quad(IR_GOTO, NULL, NULL, loop_start);

            /* 循环结束标签 */
            add_quad(IR_LABEL, loop_end, NULL, NULL);
            break;
        }

        case NODE_BREAK_STMT: {
            /* break语句：跳转到循环结束 */
            if (break_label) {
                add_quad(IR_GOTO, NULL, NULL, break_label);
            } else {
                report_error(COMPILE_ERR_IRGEN, "break statement not within loop");
            }
            break;
        }

        case NODE_CONTINUE_STMT: {
            /* continue语句：跳转到循环开始 */
            if (continue_label) {
                add_quad(IR_GOTO, NULL, NULL, continue_label);
            } else {
                report_error(COMPILE_ERR_IRGEN, "continue statement not within loop");
            }
            break;
        }

        case NODE_RETURN_STMT: {
            /* return语句 */
            if (node->child) {
                char *ret_val = gen_expr(node->child);
                add_quad(IR_RETURN, ret_val, NULL, NULL);
            } else {
                add_quad(IR_RETURN, NULL, NULL, NULL);
            }
            break;
        }

        case NODE_EXPR_STMT: {
            /* 表达式语句 */
            if (node->child) {
                gen_expr(node->child);
            }
            break;
        }

        default:
            report_error(COMPILE_ERR_IRGEN, "Unknown statement type: %d", node->type);
            break;
    }
}

/**
 * @brief 生成整个程序的中间代码
 * @param root 语法树根节点
 * @return 中间代码结构
 */
IRCode *gen_ir(ASTNode *root) {
    ir = safe_malloc(sizeof(IRCode));
    ir->quads = NULL;
    ir->count = 0;
    ir->capacity = 0;
    temp_counter = 0;
    label_counter = 0;

    if (!root) return ir;

    /* 遍历所有顶层定义 */
    for (ASTNode *p = root->child; p; p = p->sibling) {
        if (p->type == NODE_FUNC_DEF) {
            /* 函数定义 */
            add_quad(IR_FUNC_ENTER, p->attr.name, NULL, NULL);

            /* 找到函数体（最后一个child） */
            ASTNode *body = p->child;
            while (body && body->sibling) {
                body = body->sibling;
            }

            /* 生成函数体的中间代码 */
            gen_stmt(body, NULL, NULL);

            add_quad(IR_FUNC_EXIT, NULL, NULL, NULL);
        }
    }

    return ir;
}