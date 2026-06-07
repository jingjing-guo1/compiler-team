/**
 * @file common.h
 * @brief 编译器公共头文件：数据结构、枚举、函数声明
 * @version 2.0 (多错误支持)
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

/* ========== 错误处理（多错误收集） ========== */

/** 错误码（用于报告错误类型，不强制停止） */
typedef enum {
    COMPILE_SUCCESS = 0,
    COMPILE_ERR_LEXICAL,
    COMPILE_ERR_SYNTAX,
    COMPILE_ERR_SEMANTIC,
    COMPILE_ERR_IRGEN,
    COMPILE_ERR_CODEGEN,
    COMPILE_ERR_FILE_IO,
    COMPILE_ERR_MEMORY
} CompileErrorCode;

/** 编译器全局状态 */
typedef struct CompilerState {
    int error_count;              /**< 已发生的错误总数 */
    int max_errors;               /**< 最大允许错误数，超过则停止 */
    CompileErrorCode last_error_code;
    bool verbose;                 /**< 是否输出详细信息 */
    bool dump_tokens;             /**< 是否打印Token流 */
    bool dump_ast;                /**< 是否打印语法树 */
    bool dump_ir;                 /**< 是否打印中间代码 */
    const char *input_filename;
    const char *output_filename;
} CompilerState;

extern CompilerState g_state;     /**< 全局状态，在 main.c 中定义 */

/**
 * @brief 报告一个错误（不立即停止，增加错误计数）
 * @param code 错误码
 * @param fmt 格式化字符串（类似 printf）
 */
void report_error(CompileErrorCode code, const char *fmt, ...);

/**
 * @brief 检查是否应该停止编译（超过最大错误数或内存严重错误）
 * @return true 表示应该立即停止
 */
bool should_stop(void);

/**
 * @brief 安全内存分配，失败时报告致命错误并退出
 */
void *safe_malloc(size_t size);

/**
 * @brief 安全内存重分配
 */
void *safe_realloc(void *ptr, size_t size);

/* ========== 词法分析 ========== */

/** Token 类型（TINY 语言） */
typedef enum {
    TOKEN_IF, TOKEN_THEN, TOKEN_ELSE, TOKEN_END,
    TOKEN_REPEAT, TOKEN_UNTIL, TOKEN_READ, TOKEN_WRITE,
    TOKEN_ID, TOKEN_NUM,
    TOKEN_ASSIGN,   // :=
    TOKEN_EQ,       // =
    TOKEN_LT,       // <
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_TIMES, TOKEN_OVER,
    TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_SEMICOLON,
    TOKEN_EOF, TOKEN_ERROR
} TokenType;

/** Token 结构 */
typedef struct Token {
    TokenType type;
    char *value;      /**< 对于 ID/NUM 存储词素字符串，其他为 NULL */
    int line;         /**< 行号（用于错误报告） */
} Token;

/** Token 动态数组 */
typedef struct TokenList {
    Token *tokens;
    int count;
    int capacity;
} TokenList;

/**
 * @brief 词法分析主函数（由成员2实现，但框架已提供基本错误恢复）
 * @param source_code 源代码字符串
 * @return Token 列表
 */
TokenList *lex_analyze(const char *source_code);

void free_token_list(TokenList *list);
void dump_tokens(TokenList *list);

/* ========== 语法树 ========== */

/** 语法树节点类型 */
typedef enum {
    NODE_PROGRAM,
    NODE_IF_STMT, NODE_REPEAT_STMT, NODE_ASSIGN_STMT,
    NODE_READ_STMT, NODE_WRITE_STMT,
    NODE_OP_EXPR, NODE_CONST_EXPR, NODE_ID_EXPR,
    NODE_ERROR      /**< 错误节点，用于错误恢复时占位 */
} NodeType;

typedef struct ASTNode ASTNode;

struct ASTNode {
    NodeType type;
    int data_type;           /**< 语义分析后填充（1=int） */
    ASTNode *child[3];       /**< 子节点（最多3个） */
    ASTNode *sibling;        /**< 兄弟节点（用于语句序列） */
    int line_no;             /**< 行号 */
    union {
        struct { char *name; } id;   /**< 标识符名 */
        int int_val;                 /**< 整数值 */
        char op;                     /**< 运算符 '+','-','*','/','<','=' */
    } attr;
};

/**
 * @brief 语法分析主函数（由成员3实现，但框架已提供错误恢复骨架）
 * @param tokens Token 列表
 * @return 语法树根节点（可能包含 NODE_ERROR 节点）
 */
ASTNode *parse(TokenList *tokens);

void free_ast(ASTNode *node);
void dump_ast(ASTNode *node, int indent);

/* ========== 语义分析 ========== */

/** 符号表节点（简单链表） */
typedef struct Symbol {
    char *name;
    int type;          /**< 1=int (简化) */
    struct Symbol *next;
} Symbol;

void init_symbol_table(void);
void insert_symbol(const char *name, int type);
int lookup_symbol(const char *name);
void semantic_analyze(ASTNode *root);
void free_symbol_table(void);

/* ========== 中间代码生成 ========== */

/** 三地址码操作码 */
typedef enum {
    IR_ADD, IR_SUB, IR_MUL, IR_DIV,
    IR_ASSIGN, IR_LABEL, IR_GOTO, IR_IF_FALSE_GOTO,
    IR_EQ, IR_LT, IR_READ, IR_WRITE
} IROp;

/** 四元式 */
typedef struct IRQuad {
    IROp op;
    char *arg1;
    char *arg2;
    char *result;
} IRQuad;

/** 中间代码列表 */
typedef struct IRCode {
    IRQuad *quads;
    int count;
    int capacity;
} IRCode;

IRCode *gen_ir(ASTNode *root);
char *new_temp(void);
void free_ir(IRCode *ir);
void dump_ir(IRCode *ir);

/* ========== 目标代码生成 ========== */
void gen_assembly(IRCode *ir, const char *output_filename);

#endif