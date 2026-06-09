/**
 * @file ir_test.c
 * @brief 三地址码生成测试示例
 * @author 成员5
 *
 * 本文件展示如何使用三地址码生成功能
 */

#include "common.h"
#include <stdio.h>

/* 辅助函数：创建数字节点 */
ASTNode* make_number(int val) {
    ASTNode* node = (ASTNode*)safe_malloc(sizeof(ASTNode));
    node->type = NODE_NUMBER;
    node->data_type = TYPE_INT;
    node->child = NULL;
    node->sibling = NULL;
    node->line_no = 1;
    node->attr.int_val = val;
    return node;
}

/* 辅助函数：创建变量节点 */
ASTNode* make_lval(const char* name) {
    ASTNode* node = (ASTNode*)safe_malloc(sizeof(ASTNode));
    node->type = NODE_LVAL;
    node->data_type = TYPE_INT;
    node->child = NULL;
    node->sibling = NULL;
    node->line_no = 1;
    node->attr.name = strdup(name);
    return node;
}

/* 辅助函数：创建二元表达式节点 */
ASTNode* make_binary(char op, ASTNode* left, ASTNode* right) {
    ASTNode* node = (ASTNode*)safe_malloc(sizeof(ASTNode));
    node->type = NODE_BINARY_EXPR;
    node->data_type = TYPE_INT;
    node->child = left;
    left->sibling = right;
    node->sibling = NULL;
    node->line_no = 1;
    node->attr.op = op;
    return node;
}

/* 辅助函数：创建赋值语句节点 */
ASTNode* make_assign(const char* name, ASTNode* expr) {
    ASTNode* node = (ASTNode*)safe_malloc(sizeof(ASTNode));
    node->type = NODE_ASSIGN_STMT;
    node->data_type = TYPE_INT;
    node->child = make_lval(name);
    node->child->sibling = expr;
    node->sibling = NULL;
    node->line_no = 1;
    return node;
}

/* 辅助函数：创建块节点 */
ASTNode* make_block(ASTNode* stmt) {
    ASTNode* node = (ASTNode*)safe_malloc(sizeof(ASTNode));
    node->type = NODE_BLOCK;
    node->data_type = TYPE_INT;
    node->child = stmt;
    node->sibling = NULL;
    node->line_no = 1;
    return node;
}

/* 辅助函数：创建函数定义节点 */
ASTNode* make_func(const char* name, ASTNode* body) {
    ASTNode* node = (ASTNode*)safe_malloc(sizeof(ASTNode));
    node->type = NODE_FUNC_DEF;
    node->data_type = TYPE_INT;
    node->child = body;
    node->sibling = NULL;
    node->line_no = 1;
    node->attr.name = strdup(name);
    return node;
}

/* 辅助函数：创建程序节点 */
ASTNode* make_prog(ASTNode* func) {
    ASTNode* node = (ASTNode*)safe_malloc(sizeof(ASTNode));
    node->type = NODE_PROG;
    node->data_type = TYPE_INT;
    node->child = func;
    node->sibling = NULL;
    node->line_no = 1;
    return node;
}

/* 测试1：简单赋值表达式 a = 10 + b */
void test_simple_assign() {
    printf("\n========== 测试1: a = 10 + b ==========\n");

    /* 构建语法树：a = 10 + b */
    ASTNode* num10 = make_number(10);
    ASTNode* var_b = make_lval("b");
    ASTNode* add_expr = make_binary('+', num10, var_b);
    ASTNode* assign_stmt = make_assign("a", add_expr);
    ASTNode* block = make_block(assign_stmt);
    ASTNode* func = make_func("main", block);
    ASTNode* prog = make_prog(func);

    /* 生成三地址码 */
    IRCode* ir = gen_ir(prog);

    /* 打印结果 */
    dump_ir(ir);

    /* 释放内存 */
    free_ir(ir);
    free_ast(prog);
}

/* 测试2：复杂表达式 a = (b + c) * (d - e) */
void test_complex_expr() {
    printf("\n========== 测试2: a = (b + c) * (d - e) ==========\n");

    /* 构建语法树 */
    ASTNode* var_b = make_lval("b");
    ASTNode* var_c = make_lval("c");
    ASTNode* add_expr = make_binary('+', var_b, var_c);

    ASTNode* var_d = make_lval("d");
    ASTNode* var_e = make_lval("e");
    ASTNode* sub_expr = make_binary('-', var_d, var_e);

    ASTNode* mul_expr = make_binary('*', add_expr, sub_expr);
    ASTNode* assign_stmt = make_assign("a", mul_expr);
    ASTNode* block = make_block(assign_stmt);
    ASTNode* func = make_func("main", block);
    ASTNode* prog = make_prog(func);

    /* 生成三地址码 */
    IRCode* ir = gen_ir(prog);

    /* 打印结果 */
    dump_ir(ir);

    /* 释放内存 */
    free_ir(ir);
    free_ast(prog);
}

/* 测试3：比较表达式 a = b < c */
void test_comparison() {
    printf("\n========== 测试3: a = b < c ==========\n");

    /* 构建语法树 */
    ASTNode* var_b = make_lval("b");
    ASTNode* var_c = make_lval("c");
    ASTNode* cmp_expr = make_binary('<', var_b, var_c);
    ASTNode* assign_stmt = make_assign("a", cmp_expr);
    ASTNode* block = make_block(assign_stmt);
    ASTNode* func = make_func("main", block);
    ASTNode* prog = make_prog(func);

    /* 生成三地址码 */
    IRCode* ir = gen_ir(prog);

    /* 打印结果 */
    dump_ir(ir);

    /* 释放内存 */
    free_ir(ir);
    free_ast(prog);
}

/* 辅助函数：创建if语句节点 */
ASTNode* make_if(ASTNode* cond, ASTNode* then_stmt, ASTNode* else_stmt) {
    ASTNode* node = (ASTNode*)safe_malloc(sizeof(ASTNode));
    node->type = NODE_IF_STMT;
    node->data_type = TYPE_INT;
    node->child = cond;
    cond->sibling = then_stmt;
    then_stmt->sibling = else_stmt;
    node->sibling = NULL;
    node->line_no = 1;
    return node;
}

/* 测试4：if语句 */
void test_if_stmt() {
    printf("\n========== 测试4: if (a < 10) b = 1; else b = 0; ==========\n");

    /* 构建语法树 */
    ASTNode* var_a = make_lval("a");
    ASTNode* num10 = make_number(10);
    ASTNode* cond = make_binary('<', var_a, num10);

    ASTNode* num1 = make_number(1);
    ASTNode* then_assign = make_assign("b", num1);

    ASTNode* num0 = make_number(0);
    ASTNode* else_assign = make_assign("b", num0);

    ASTNode* if_stmt = make_if(cond, then_assign, else_assign);
    ASTNode* block = make_block(if_stmt);
    ASTNode* func = make_func("main", block);
    ASTNode* prog = make_prog(func);

    /* 生成三地址码 */
    IRCode* ir = gen_ir(prog);

    /* 打印结果 */
    dump_ir(ir);

    /* 释放内存 */
    free_ir(ir);
    free_ast(prog);
}

/* 辅助函数：创建while语句节点 */
ASTNode* make_while(ASTNode* cond, ASTNode* body) {
    ASTNode* node = (ASTNode*)safe_malloc(sizeof(ASTNode));
    node->type = NODE_WHILE_STMT;
    node->data_type = TYPE_INT;
    node->child = cond;
    cond->sibling = body;
    node->sibling = NULL;
    node->line_no = 1;
    return node;
}

/* 测试5：while循环 */
void test_while_stmt() {
    printf("\n========== 测试5: while (n > 1) { result = result * n; } ==========\n");

    /* 构建语法树 */
    ASTNode* var_n = make_lval("n");
    ASTNode* num1 = make_number(1);
    ASTNode* cond = make_binary('>', var_n, num1);

    ASTNode* var_result = make_lval("result");
    ASTNode* var_n2 = make_lval("n");
    ASTNode* mul_expr = make_binary('*', var_result, var_n2);
    ASTNode* assign_stmt = make_assign("result", mul_expr);

    ASTNode* while_stmt = make_while(cond, assign_stmt);
    ASTNode* block = make_block(while_stmt);
    ASTNode* func = make_func("main", block);
    ASTNode* prog = make_prog(func);

    /* 生成三地址码 */
    IRCode* ir = gen_ir(prog);

    /* 打印结果 */
    dump_ir(ir);

    /* 释放内存 */
    free_ir(ir);
    free_ast(prog);
}

/* 主函数 */
int main() {
    printf("========================================\n");
    printf("  三地址码生成器测试程序\n");
    printf("  作者: 成员5\n");
    printf("========================================\n");

    /* 运行所有测试 */
    test_simple_assign();
    test_complex_expr();
    test_comparison();
    test_if_stmt();
    test_while_stmt();

    printf("\n========== 所有测试完成 ==========\n");

    return 0;
}
