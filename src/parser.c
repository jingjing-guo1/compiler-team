/**
 * @file parser.c
 * @brief 语法分析器（C语言子集，递归下降）
 */

#include "common.h"

static TokenList *tokens;
static int pos;

static Token current_token(void) {
    if (pos >= tokens->count) {
        static Token eof = {TOKEN_EOF, NULL, 0, 0};
        return eof;
    }
    return tokens->tokens[pos];
}

static void advance(void) { if (pos < tokens->count) pos++; }

static bool match(TokenType expected) {
    if (current_token().type == expected) {
        advance();
        return true;
    }
    return false;
}

static void expect(TokenType expected) {
    if (!match(expected)) {
        report_error(COMPILE_ERR_SYNTAX, "Expected token %d, got %d at line %d",
                     expected, current_token().type, current_token().line);
    }
}

static void synchronize(void) {
    while (pos < tokens->count) {
        TokenType type = current_token().type;
        if (type == TOKEN_SEMICOLON || type == TOKEN_RBRACE ||
            type == TOKEN_KW_INT || type == TOKEN_KW_VOID ||
            type == TOKEN_KW_IF || type == TOKEN_KW_WHILE ||
            type == TOKEN_KW_RETURN || type == TOKEN_EOF)
            return;
        advance();
    }
}

// 前向声明
static ASTNode *declaration(void);
static ASTNode *statement(void);
static ASTNode *expression(void);
static ASTNode *primary_expr(void);
static ASTNode *unary_expr(void);
static ASTNode *multiplicative_expr(void);
static ASTNode *additive_expr(void);
static ASTNode *relational_expr(void);
static ASTNode *equality_expr(void);
static ASTNode *logical_and_expr(void);
static ASTNode *logical_or_expr(void);
static ASTNode *assignment_expr(void);

static ASTNode *new_node(NodeType type, int line) {
    ASTNode *node = (ASTNode*)safe_malloc(sizeof(ASTNode));
    node->type = type;
    node->data_type = TYPE_UNKNOWN;
    node->child = NULL;
    node->sibling = NULL;
    node->line_no = line;
    memset(&node->attr, 0, sizeof(node->attr));
    return node;
}

static void add_child(ASTNode *parent, ASTNode *child) {
    if (!parent->child) parent->child = child;
    else {
        ASTNode *p = parent->child;
        while (p->sibling) p = p->sibling;
        p->sibling = child;
    }
}

// 解析类型说明符
static DataType parse_type_spec(void) {
    if (current_token().type == TOKEN_KW_INT) {
        advance();
        return TYPE_INT;
    } else if (current_token().type == TOKEN_KW_VOID) {
        advance();
        return TYPE_VOID;
    } else {
        report_error(COMPILE_ERR_SYNTAX, "Expected type specifier at line %d", current_token().line);
        return TYPE_UNKNOWN;
    }
}

// 解析参数列表（函数形参）
static ASTNode *param_list(void) {
    ASTNode *head = NULL, *tail = NULL;
    do {
        DataType type = parse_type_spec();
        if (current_token().type != TOKEN_IDENTIFIER) {
            report_error(COMPILE_ERR_SYNTAX, "Expected parameter name at line %d", current_token().line);
            synchronize();
            return head;
        }
        Token id = current_token();
        advance();
        ASTNode *param = new_node(NODE_FUNC_FPARAM, id.line);
        param->attr.name = strdup(id.value);
        param->data_type = type;
        if (!head) head = param;
        else tail->sibling = param;
        tail = param;
        if (current_token().type == TOKEN_COMMA) advance();
        else break;
    } while (1);
    return head;
}

// 解析复合语句 { ... }
static ASTNode *block_item_list(void);
static ASTNode *compound_stmt(void) {
    int line = current_token().line;
    expect(TOKEN_LBRACE);
    ASTNode *block = new_node(NODE_BLOCK, line);
    ASTNode *list = block_item_list();
    block->child = list;
    expect(TOKEN_RBRACE);
    return block;
}

static ASTNode *block_item_list(void) {
    ASTNode *head = NULL, *tail = NULL;
    while (current_token().type != TOKEN_RBRACE && current_token().type != TOKEN_EOF) {
        ASTNode *item = NULL;
        if (current_token().type == TOKEN_KW_INT || current_token().type == TOKEN_KW_VOID || current_token().type == TOKEN_KW_CONST) {
            // 变量声明或常量声明（简化：只处理变量声明）
            DataType type = parse_type_spec();
            if (current_token().type != TOKEN_IDENTIFIER) {
                report_error(COMPILE_ERR_SYNTAX, "Expected identifier in declaration at line %d", current_token().line);
                synchronize();
                continue;
            }
            Token id = current_token();
            advance();
            ASTNode *decl = new_node(NODE_VAR_DECL, id.line);
            decl->data_type = type;
            decl->attr.name = strdup(id.value);
            // 初始化可选
            if (current_token().type == TOKEN_ASSIGN) {
                advance();
                ASTNode *init = expression();
                ASTNode *def = new_node(NODE_VAR_DEF, id.line);
                def->child = init;
                def->attr.name = strdup(id.value);
                add_child(decl, def);
            }
            expect(TOKEN_SEMICOLON);
            item = decl;
        } else {
            item = statement();
        }
        if (item) {
            if (!head) head = item;
            else tail->sibling = item;
            tail = item;
        }
    }
    return head;
}

// 语句解析
static ASTNode *statement(void) {
    Token tok = current_token();
    switch (tok.type) {
        case TOKEN_LBRACE:
            return compound_stmt();
        case TOKEN_KW_IF: {
            int line = tok.line;
            advance();
            expect(TOKEN_LPAREN);
            ASTNode *cond = expression();
            expect(TOKEN_RPAREN);
            ASTNode *then_stmt = statement();
            ASTNode *else_stmt = NULL;
            if (current_token().type == TOKEN_KW_ELSE) {
                advance();
                else_stmt = statement();
            }
            ASTNode *node = new_node(NODE_IF_STMT, line);
            add_child(node, cond);
            add_child(node, then_stmt);
            add_child(node, else_stmt);
            return node;
        }
        case TOKEN_KW_WHILE: {
            int line = tok.line;
            advance();
            expect(TOKEN_LPAREN);
            ASTNode *cond = expression();
            expect(TOKEN_RPAREN);
            ASTNode *body = statement();
            ASTNode *node = new_node(NODE_WHILE_STMT, line);
            add_child(node, cond);
            add_child(node, body);
            return node;
        }
        case TOKEN_KW_RETURN: {
            int line = tok.line;
            advance();
            ASTNode *ret_expr = NULL;
            if (current_token().type != TOKEN_SEMICOLON)
                ret_expr = expression();
            expect(TOKEN_SEMICOLON);
            ASTNode *node = new_node(NODE_RETURN_STMT, line);
            node->child = ret_expr;
            return node;
        }
        case TOKEN_KW_BREAK: {
            int line = tok.line;
            advance();
            expect(TOKEN_SEMICOLON);
            return new_node(NODE_BREAK_STMT, line);
        }
        case TOKEN_KW_CONTINUE: {
            int line = tok.line;
            advance();
            expect(TOKEN_SEMICOLON);
            return new_node(NODE_CONTINUE_STMT, line);
        }
        case TOKEN_SEMICOLON:
            advance();
            return NULL;  // 空语句
        default: {
            ASTNode *expr = expression();
            expect(TOKEN_SEMICOLON);
            ASTNode *node = new_node(NODE_EXPR_STMT, expr ? expr->line_no : tok.line);
            node->child = expr;
            return node;
        }
    }
}

// 表达式解析（优先级从低到高）
static ASTNode *logical_or_expr(void) {
    ASTNode *node = logical_and_expr();
    while (current_token().type == TOKEN_OR) {
        int line = current_token().line;
        advance();
        ASTNode *right = logical_and_expr();
        ASTNode *bin = new_node(NODE_BINARY_EXPR, line);
        bin->attr.op = '|';
        bin->child = node;
        bin->child->sibling = right;
        node = bin;
    }
    return node;
}

static ASTNode *logical_and_expr(void) {
    ASTNode *node = equality_expr();
    while (current_token().type == TOKEN_AND) {
        int line = current_token().line;
        advance();
        ASTNode *right = equality_expr();
        ASTNode *bin = new_node(NODE_BINARY_EXPR, line);
        bin->attr.op = '&';
        bin->child = node;
        bin->child->sibling = right;
        node = bin;
    }
    return node;
}

static ASTNode *equality_expr(void) {
    ASTNode *node = relational_expr();
    while (current_token().type == TOKEN_EQ || current_token().type == TOKEN_NEQ) {
        int line = current_token().line;
        TokenType op = current_token().type;
        advance();
        ASTNode *right = relational_expr();
        ASTNode *bin = new_node(NODE_BINARY_EXPR, line);
        bin->attr.op = (op == TOKEN_EQ) ? '=' : '!';
        bin->child = node;
        bin->child->sibling = right;
        node = bin;
    }
    return node;
}

static ASTNode *relational_expr(void) {
    ASTNode *node = additive_expr();
    while (current_token().type == TOKEN_LT || current_token().type == TOKEN_LE ||
           current_token().type == TOKEN_GT || current_token().type == TOKEN_GE) {
        int line = current_token().line;
        TokenType op = current_token().type;
        advance();
        ASTNode *right = additive_expr();
        ASTNode *bin = new_node(NODE_BINARY_EXPR, line);
        char c;
        if (op == TOKEN_LT) c = '<';
        else if (op == TOKEN_LE) c = 'l';
        else if (op == TOKEN_GT) c = '>';
        else c = 'g';
        bin->attr.op = c;
        bin->child = node;
        bin->child->sibling = right;
        node = bin;
    }
    return node;
}

static ASTNode *additive_expr(void) {
    ASTNode *node = multiplicative_expr();
    while (current_token().type == TOKEN_PLUS || current_token().type == TOKEN_MINUS) {
        int line = current_token().line;
        char op = (current_token().type == TOKEN_PLUS) ? '+' : '-';
        advance();
        ASTNode *right = multiplicative_expr();
        ASTNode *bin = new_node(NODE_BINARY_EXPR, line);
        bin->attr.op = op;
        bin->child = node;
        bin->child->sibling = right;
        node = bin;
    }
    return node;
}

static ASTNode *multiplicative_expr(void) {
    ASTNode *node = unary_expr();
    while (current_token().type == TOKEN_MUL || current_token().type == TOKEN_DIV || current_token().type == TOKEN_MOD) {
        int line = current_token().line;
        char op = (current_token().type == TOKEN_MUL) ? '*' : (current_token().type == TOKEN_DIV) ? '/' : '%';
        advance();
        ASTNode *right = unary_expr();
        ASTNode *bin = new_node(NODE_BINARY_EXPR, line);
        bin->attr.op = op;
        bin->child = node;
        bin->child->sibling = right;
        node = bin;
    }
    return node;
}

static ASTNode *unary_expr(void) {
    if (current_token().type == TOKEN_MINUS || current_token().type == TOKEN_NOT) {
        int line = current_token().line;
        char op = (current_token().type == TOKEN_MINUS) ? '-' : '!';
        advance();
        ASTNode *operand = unary_expr();
        ASTNode *unary = new_node(NODE_UNARY_EXPR, line);
        unary->attr.op = op;
        unary->child = operand;
        return unary;
    }
    return primary_expr();
}

static ASTNode *primary_expr(void) {
    Token tok = current_token();
    if (tok.type == TOKEN_INT_LITERAL) {
        advance();
        ASTNode *node = new_node(NODE_NUMBER, tok.line);
        node->attr.int_val = atoi(tok.value);
        return node;
    } else if (tok.type == TOKEN_IDENTIFIER) {
        advance();
        ASTNode *node = new_node(NODE_LVAL, tok.line);
        node->attr.name = strdup(tok.value);
        // 检查是否为函数调用
        if (current_token().type == TOKEN_LPAREN) {
            advance();
            ASTNode *call = new_node(NODE_FUNC_CALL, tok.line);
            call->attr.name = node->attr.name;
            // 解析实参列表
            if (current_token().type != TOKEN_RPAREN) {
                do {
                    ASTNode *arg = expression();
                    add_child(call, arg);
                    if (current_token().type == TOKEN_COMMA) advance();
                    else break;
                } while (1);
            }
            expect(TOKEN_RPAREN);
            free_ast(node);  // 释放LVAL节点，用call代替
            return call;
        }
        return node;
    } else if (tok.type == TOKEN_LPAREN) {
        advance();
        ASTNode *expr = expression();
        expect(TOKEN_RPAREN);
        return expr;
    } else {
        report_error(COMPILE_ERR_SYNTAX, "Unexpected token in expression at line %d", tok.line);
        return NULL;
    }
}

static ASTNode *expression(void) {
    return assignment_expr();
}

static ASTNode *assignment_expr(void) {
    ASTNode *left = logical_or_expr();
    if (current_token().type == TOKEN_ASSIGN) {
        advance();
        ASTNode *right = assignment_expr();
        ASTNode *assign = new_node(NODE_ASSIGN_STMT, left ? left->line_no : 0);
        assign->child = left;
        assign->child->sibling = right;
        return assign;
    }
    return left;
}

// 外部声明: 函数定义或声明（简化只处理函数定义）
static ASTNode *external_declaration(void) {
    DataType ret_type = parse_type_spec();
    if (current_token().type != TOKEN_IDENTIFIER) {
        report_error(COMPILE_ERR_SYNTAX, "Expected function name at line %d", current_token().line);
        synchronize();
        return NULL;
    }
    Token func_name = current_token();
    advance();
    if (current_token().type != TOKEN_LPAREN) {
        report_error(COMPILE_ERR_SYNTAX, "Expected '(' after function name at line %d", current_token().line);
        synchronize();
        return NULL;
    }
    advance();
    ASTNode *params = NULL;
    if (current_token().type != TOKEN_RPAREN) {
        params = param_list();
    }
    expect(TOKEN_RPAREN);
    ASTNode *body = compound_stmt();
    ASTNode *func = new_node(NODE_FUNC_DEF, func_name.line);
    func->data_type = ret_type;
    func->attr.name = strdup(func_name.value);
    func->child = params;
    if (params) {
        ASTNode *p = params;
        while (p->sibling) p = p->sibling;
        p->sibling = body;
    } else {
        func->child = body;
    }
    return func;
}

// 程序入口
ASTNode *parse(TokenList *token_list) {
    tokens = token_list;
    pos = 0;
    ASTNode *prog = new_node(NODE_PROG, 1);
    ASTNode *tail = NULL;
    while (current_token().type != TOKEN_EOF) {
        ASTNode *decl = external_declaration();
        if (decl) {
            if (!prog->child) prog->child = decl;
            else tail->sibling = decl;
            tail = decl;
        } else {
            synchronize();
        }
    }
    return prog;
}