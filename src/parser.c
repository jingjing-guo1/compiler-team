/**
 * @file parser.c
 * @brief 语法分析器（支持错误恢复，通过同步点收集多个语法错误）
 * @note 成员3 可在此基础上实现完整的 TINY 文法解析
 */

#include "common.h"

static TokenList *tokens;
static int pos;

/** 获取当前 Token */
static Token current_token(void) {
    if (pos >= tokens->count) {
        static Token eof = {TOKEN_EOF, NULL, 0};
        return eof;
    }
    return tokens->tokens[pos];
}

/** 匹配预期 Token，成功则前进，否则报告错误并返回 false */
static bool match(TokenType expected) {
    Token tok = current_token();
    if (tok.type == expected) {
        pos++;
        return true;
    }
    report_error(COMPILE_ERR_SYNTAX, "Expected token %d, got %d at line %d",
                 expected, tok.type, tok.line);
    return false;
}

/** 同步点：跳过直到遇到能够开始新语句的 Token */
static void synchronize(void) {
    while (pos < tokens->count) {
        TokenType type = current_token().type;
        if (type == TOKEN_SEMICOLON || type == TOKEN_END ||
            type == TOKEN_REPEAT || type == TOKEN_IF ||
            type == TOKEN_READ || type == TOKEN_WRITE ||
            type == TOKEN_ID || type == TOKEN_EOF) {
            return;
        }
        pos++;
    }
}

/* ----- 前向声明 ----- */
static ASTNode *stmt_sequence(void);
static ASTNode *statement(void);
static ASTNode *expr(void);

/** 创建一个错误节点（用于占位） */
static ASTNode *error_node(int line) {
    ASTNode *node = (ASTNode*)safe_malloc(sizeof(ASTNode));
    node->type = NODE_ERROR;
    node->line_no = line;
    for (int i = 0; i < 3; i++) node->child[i] = NULL;
    node->sibling = NULL;
    return node;
}

/** 解析赋值语句: id := expr */
static ASTNode *assign_stmt(void) {
    Token id_tok = current_token();
    if (id_tok.type != TOKEN_ID) {
        report_error(COMPILE_ERR_SYNTAX, "Expected identifier at line %d", id_tok.line);
        synchronize();
        return NULL;
    }
    pos++; // consume id
    if (!match(TOKEN_ASSIGN)) {
        synchronize();
        return NULL;
    }
    ASTNode *rhs = expr();
    if (!rhs) {
        synchronize();
        return NULL;
    }
    ASTNode *node = (ASTNode*)safe_malloc(sizeof(ASTNode));
    node->type = NODE_ASSIGN_STMT;
    node->line_no = id_tok.line;
    node->child[0] = (ASTNode*)safe_malloc(sizeof(ASTNode));
    node->child[0]->type = NODE_ID_EXPR;
    node->child[0]->attr.id.name = strdup(id_tok.value);
    node->child[0]->line_no = id_tok.line;
    node->child[1] = rhs;
    node->child[2] = NULL;
    node->sibling = NULL;
    return node;
}

/** 解析 if 语句 (简化) */
static ASTNode *if_stmt(void) {
    int line = current_token().line;
    pos++; // consume 'if'
    ASTNode *cond = expr();
    if (!cond) {
        synchronize();
        return error_node(line);
    }
    if (!match(TOKEN_THEN)) {
        synchronize();
        return error_node(line);
    }
    ASTNode *then_part = stmt_sequence();
    ASTNode *else_part = NULL;
    if (current_token().type == TOKEN_ELSE) {
        pos++;
        else_part = stmt_sequence();
    }
    if (!match(TOKEN_END)) {
        synchronize();
        return error_node(line);
    }
    ASTNode *node = (ASTNode*)safe_malloc(sizeof(ASTNode));
    node->type = NODE_IF_STMT;
    node->line_no = line;
    node->child[0] = cond;
    node->child[1] = then_part;
    node->child[2] = else_part;
    return node;
}

/** 解析 repeat 语句 */
static ASTNode *repeat_stmt(void) {
    int line = current_token().line;
    pos++; // consume 'repeat'
    ASTNode *body = stmt_sequence();
    if (!match(TOKEN_UNTIL)) {
        synchronize();
        return error_node(line);
    }
    ASTNode *cond = expr();
    if (!cond) {
        synchronize();
        return error_node(line);
    }
    ASTNode *node = (ASTNode*)safe_malloc(sizeof(ASTNode));
    node->type = NODE_REPEAT_STMT;
    node->line_no = line;
    node->child[0] = body;
    node->child[1] = cond;
    return node;
}

/** 解析 read 语句 */
static ASTNode *read_stmt(void) {
    int line = current_token().line;
    pos++; // consume 'read'
    if (current_token().type != TOKEN_ID) {
        report_error(COMPILE_ERR_SYNTAX, "Expected identifier after 'read' at line %d", line);
        synchronize();
        return error_node(line);
    }
    Token id_tok = current_token();
    pos++;
    ASTNode *node = (ASTNode*)safe_malloc(sizeof(ASTNode));
    node->type = NODE_READ_STMT;
    node->line_no = line;
    node->child[0] = (ASTNode*)safe_malloc(sizeof(ASTNode));
    node->child[0]->type = NODE_ID_EXPR;
    node->child[0]->attr.id.name = strdup(id_tok.value);
    return node;
}

/** 解析 write 语句 */
static ASTNode *write_stmt(void) {
    int line = current_token().line;
    pos++; // consume 'write'
    ASTNode *exp = expr();
    if (!exp) {
        synchronize();
        return error_node(line);
    }
    ASTNode *node = (ASTNode*)safe_malloc(sizeof(ASTNode));
    node->type = NODE_WRITE_STMT;
    node->line_no = line;
    node->child[0] = exp;
    return node;
}

/** 解析一条语句 */
static ASTNode *statement(void) {
    Token tok = current_token();
    switch (tok.type) {
        case TOKEN_ID:
            return assign_stmt();
        case TOKEN_IF:
            return if_stmt();
        case TOKEN_REPEAT:
            return repeat_stmt();
        case TOKEN_READ:
            return read_stmt();
        case TOKEN_WRITE:
            return write_stmt();
        default:
            report_error(COMPILE_ERR_SYNTAX, "Unexpected token at line %d", tok.line);
            synchronize();
            return NULL;
    }
}

/** 解析语句序列 (分号分隔) */
static ASTNode *stmt_sequence(void) {
    ASTNode *head = NULL, *tail = NULL;
    while (1) {
        TokenType type = current_token().type;
        if (type == TOKEN_END || type == TOKEN_UNTIL || type == TOKEN_EOF)
            break;
        ASTNode *stmt = statement();
        if (stmt) {
            if (!head) head = stmt;
            else tail->sibling = stmt;
            tail = stmt;
        }
        // 如果当前 token 是分号，则消费它；否则如果语句解析失败，可能已经同步到正确位置，这里不强制
        if (current_token().type == TOKEN_SEMICOLON) {
            pos++;
        }
    }
    return head;
}

/** 解析表达式（简化，仅处理常量和标识符）*/
static ASTNode *expr(void) {
    Token tok = current_token();
    ASTNode *node = NULL;
    if (tok.type == TOKEN_NUM) {
        node = (ASTNode*)safe_malloc(sizeof(ASTNode));
        node->type = NODE_CONST_EXPR;
        node->attr.int_val = atoi(tok.value);
        node->line_no = tok.line;
        pos++;
    } else if (tok.type == TOKEN_ID) {
        node = (ASTNode*)safe_malloc(sizeof(ASTNode));
        node->type = NODE_ID_EXPR;
        node->attr.id.name = strdup(tok.value);
        node->line_no = tok.line;
        pos++;
    } else if (tok.type == TOKEN_LPAREN) {
        pos++;
        node = expr();
        if (!match(TOKEN_RPAREN)) {
            report_error(COMPILE_ERR_SYNTAX, "Missing ')' at line %d", tok.line);
            synchronize();
            return NULL;
        }
    } else {
        report_error(COMPILE_ERR_SYNTAX, "Expected expression at line %d", tok.line);
        synchronize();
        return NULL;
    }
    // 可在此处理二元运算符（简化）
    return node;
}

/** 程序入口 */
ASTNode *parse(TokenList *token_list) {
    tokens = token_list;
    pos = 0;
    ASTNode *root = stmt_sequence();
    if (current_token().type != TOKEN_EOF) {
        report_error(COMPILE_ERR_SYNTAX, "Extra tokens after end of program");
    }
    return root;
}