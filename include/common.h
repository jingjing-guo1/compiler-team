/**
 * @file common.h
 * @brief 编译器公共头文件（整合C语言子集）
 * @version 4.0
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

/* ========== 错误处理（多错误收集） ========== */

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

typedef struct CompilerState {
    int error_count;
    int max_errors;
    CompileErrorCode last_error_code;
    bool verbose;
    bool dump_tokens;
    bool dump_ast;
    bool dump_ir;
    const char *input_filename;
    const char *output_filename;
} CompilerState;

extern CompilerState g_state;

void report_error(CompileErrorCode code, const char *fmt, ...);
bool should_stop(void);
void *safe_malloc(size_t size);
void *safe_realloc(void *ptr, size_t size);

/* ========== 词法分析（C语言子集，成员2清单） ========== */

typedef enum {
    TOKEN_EOF,
    TOKEN_UNKNOWN,

    /* 关键字 */
    TOKEN_KW_INT,
    TOKEN_KW_VOID,
    TOKEN_KW_IF,
    TOKEN_KW_ELSE,
    TOKEN_KW_WHILE,
    TOKEN_KW_RETURN,
    TOKEN_KW_CONST,
    TOKEN_KW_BREAK,
    TOKEN_KW_CONTINUE,

    /* 标识符与常量 */
    TOKEN_IDENTIFIER,
    TOKEN_INT_LITERAL,

    /* 算术运算符 */
    TOKEN_PLUS,     // +
    TOKEN_MINUS,    // -
    TOKEN_MUL,      // *
    TOKEN_DIV,      // /
    TOKEN_MOD,      // %

    /* 赋值与比较运算符 */
    TOKEN_ASSIGN,   // =
    TOKEN_EQ,       // ==
    TOKEN_NEQ,      // !=
    TOKEN_LT,       // <
    TOKEN_LE,       // <=
    TOKEN_GT,       // >
    TOKEN_GE,       // >=

    /* 逻辑运算符 */
    TOKEN_NOT,      // !
    TOKEN_AND,      // &&
    TOKEN_OR,       // ||

    /* 界符 */
    TOKEN_SEMICOLON,
    TOKEN_COMMA,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,

    /* 注释（不输出，但用于识别） */
    TOKEN_COMMENT
} TokenType;

typedef struct Token {
    TokenType type;
    char *value;      // 标识符或数字的字符串
    int line;
    int column;       // 保留，可扩展
} Token;

typedef struct TokenList {
    Token *tokens;
    int count;
    int capacity;
} TokenList;

TokenList *lex_analyze(const char *source_code);
void free_token_list(TokenList *list);
void dump_tokens(TokenList *list);

/* ========== 语法树（成员3清单） ========== */

typedef struct ASTNode ASTNode;

typedef enum {
    /* 顶层结构 */
    NODE_PROG,
    NODE_COMP_UNIT,
    /* 声明与定义 */
    NODE_VAR_DECL,
    NODE_CONST_DECL,
    NODE_ARRAY_DECL,
    NODE_VAR_DEF,
    NODE_CONST_DEF,
    NODE_FUNC_DEF,
    NODE_FUNC_FPARAM,
    /* 语句 */
    NODE_BLOCK,
    NODE_ASSIGN_STMT,
    NODE_IF_STMT,
    NODE_WHILE_STMT,
    NODE_BREAK_STMT,
    NODE_CONTINUE_STMT,
    NODE_RETURN_STMT,
    NODE_EXPR_STMT,
    /* 表达式 */
    NODE_BINARY_EXPR,
    NODE_UNARY_EXPR,
    NODE_FUNC_CALL,
    NODE_LVAL,
    NODE_ARRAY_ACCESS,
    NODE_NUMBER
} NodeType;

/* 前置声明数据类型（成员4） */
typedef enum {
    TYPE_INT,
    TYPE_VOID,
    TYPE_UNKNOWN,
    TYPE_ERROR
} DataType;   // 简化版，去掉BOOL

struct ASTNode {
    NodeType type;
    DataType data_type;        // 语义分析后填充
    ASTNode *child;            // 第一个子节点（用于列表或单个）
    ASTNode *sibling;          // 兄弟节点
    int line_no;
    int size;                  // 数组长度（若为数组声明）
    /* 联合属性 */
    union {
        char *name;            // 标识符、变量名、函数名
        int int_val;           // 数字常量
        char op;               // 运算符 '+','-','*','/','%','='等
    } attr;
};

ASTNode *parse(TokenList *tokens);
void free_ast(ASTNode *node);
void dump_ast(ASTNode *node, int indent);

/* ========== 语义分析（成员4接口，简化但完整） ========== */

#define MAX_NAME_LEN 64

typedef enum {
    SYMBOL_VARIABLE,
    SYMBOL_FUNCTION,
    SYMBOL_PARAMETER
} SymbolKind;

typedef struct Symbol {
    char name[MAX_NAME_LEN];
    DataType type;
    SymbolKind kind;
    int size;          // 数组长度（若是数组）
    int scope_level;
    int line;
    struct Symbol *next;
} Symbol;

typedef struct SymbolTable {
    Symbol *head;
    int current_scope;
} SymbolTable;

void init_symbol_table(SymbolTable *table);
void enter_scope(SymbolTable *table);
void exit_scope(SymbolTable *table);
int insert_symbol(SymbolTable *table, const char *name, DataType type, SymbolKind kind, int line);
int insert_symbol_array(SymbolTable *table, const char *name, DataType type, SymbolKind kind, int size, int line);
Symbol *lookup_symbol(SymbolTable *table, const char *name);
Symbol *lookup_symbol_current_scope(SymbolTable *table, const char *name);
int semantic_analyze(ASTNode *root, SymbolTable *table);
DataType analyze_expression(ASTNode *node, SymbolTable *table);
void semantic_error(const char *message, int line);
void print_symbol_table(SymbolTable *table);
void free_symbol_table(SymbolTable *table);

/* ========== 中间代码生成（成员5清单，扩展） ========== */

typedef enum {
    IR_ADD, IR_SUB, IR_MUL, IR_DIV, IR_MOD,
    IR_ASSIGN,
    IR_LT, IR_LE, IR_GT, IR_GE, IR_EQ, IR_NE,
    IR_AND, IR_OR, IR_NOT,
    IR_LABEL, IR_GOTO, IR_IF_FALSE_GOTO,
    IR_RETURN, IR_CALL,
    IR_FUNC_ENTER, IR_FUNC_EXIT
} IROp;   // 增加函数相关和逻辑运算

typedef struct IRQuad {
    IROp op;
    char *arg1;
    char *arg2;
    char *result;
} IRQuad;

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