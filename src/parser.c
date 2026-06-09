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
    node->size = -1;
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
            bool is_const = false;
            if (current_token().type == TOKEN_KW_CONST) {
                is_const = true;
                advance();
            }
            DataType type = parse_type_spec();
            
            ASTNode *first_decl = NULL;
            ASTNode *last_decl = NULL;
            
            while (true) {
                if (current_token().type != TOKEN_IDENTIFIER) {
                    report_error(COMPILE_ERR_SYNTAX, "Expected identifier in declaration at line %d", current_token().line);
                    synchronize();
                    break;
                }
                Token id = current_token();
                advance();
                
                NodeType node_type = is_const ? NODE_CONST_DECL : NODE_VAR_DECL;
                int array_size = -1;

                // 检查是否是数组声明: id[10]
                if (current_token().type == TOKEN_LBRACKET) {
                    advance();
                    if (current_token().type != TOKEN_INT_LITERAL) {
                        report_error(COMPILE_ERR_SYNTAX, "Array size must be an integer constant at line %d", current_token().line);
                    } else {
                        array_size = atoi(current_token().value);
                        advance();
                    }
                    expect(TOKEN_RBRACKET);
                    node_type = NODE_ARRAY_DECL;
                }

                ASTNode *decl = new_node(node_type, id.line);
                decl->data_type = type;
                decl->attr.name = strdup(id.value);
                if (node_type == NODE_ARRAY_DECL) {
                    decl->size = array_size;
                }

                if (current_token().type == TOKEN_ASSIGN) {
                    if (node_type == NODE_ARRAY_DECL) {
                        report_error(COMPILE_ERR_SYNTAX, "Array initialization not supported yet at line %d", id.line);
                    }
                    advance();
                    ASTNode *init = expression();
                    ASTNode *def = new_node(is_const ? NODE_CONST_DEF : NODE_VAR_DEF, id.line);
                    def->child = init;
                    def->attr.name = strdup(id.value);
                    add_child(decl, def);
                } else if (is_const) {
                    report_error(COMPILE_ERR_SYNTAX, "Constant declaration '%s' must be initialized at line %d", id.value, id.line);
                }
                
                if (!first_decl) first_decl = decl;
                else last_decl->sibling = decl;
                last_decl = decl;

                if (current_token().type == TOKEN_COMMA) {
                    advance();
                } else {
                    break;
                }
            }
            expect(TOKEN_SEMICOLON);
            item = first_decl;
        } else {
            item = statement();
        }
        
        if (item) {
            if (!head) {
                head = item;
            } else {
                tail->sibling = item;
            }
            // 更新 tail 到链表的末尾
            tail = item;
            while (tail->sibling) tail = tail->sibling;
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
            if (!expr) {
                synchronize();
                return NULL;
            }
            expect(TOKEN_SEMICOLON);
            ASTNode *node = new_node(NODE_EXPR_STMT, expr->line_no);
            node->child = expr;
            return node;
        }
    }
}

// 表达式解析（优先级从低到高）
// 表达式解析（优先级从低到高，采用递归下降法）
// assignment_expr -> logical_or_expr [ '=' assignment_expr ]
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

// logical_or_expr -> logical_and_expr { '||' logical_and_expr }
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

// logical_and_expr -> equality_expr { '&&' equality_expr }
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

// equality_expr -> relational_expr { ('=='|'!=') relational_expr }
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

// relational_expr -> additive_expr { ('<'|'<='|'>'|'>=') additive_expr }
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

// additive_expr -> multiplicative_expr { ('+'|'-') multiplicative_expr }
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

// multiplicative_expr -> unary_expr { ('*'|'/'|'%') unary_expr }
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

// unary_expr -> primary_expr | ('-'|'!'|'+') unary_expr
static ASTNode *unary_expr(void) {
    if (current_token().type == TOKEN_MINUS || current_token().type == TOKEN_NOT || current_token().type == TOKEN_PLUS) {
        int line = current_token().line;
        TokenType op_type = current_token().type;
        advance();
        ASTNode *operand = unary_expr();
        if (op_type == TOKEN_PLUS) return operand; // 一元 '+' 不产生节点，直接返回操作数
        
        ASTNode *unary = new_node(NODE_UNARY_EXPR, line);
        unary->attr.op = (op_type == TOKEN_MINUS) ? '-' : '!';
        unary->child = operand;
        return unary;
    }
    return primary_expr();
}

// primary_expr -> '(' expression ')' | ID | ID '[' expression ']' | ID '(' [args] ')' | NUMBER
static ASTNode *primary_expr(void) {
    Token tok = current_token();
    if (tok.type == TOKEN_INT_LITERAL) {
        advance();
        ASTNode *node = new_node(NODE_NUMBER, tok.line);
        node->attr.int_val = atoi(tok.value);
        return node;
    } else if (tok.type == TOKEN_IDENTIFIER) {
        advance();
        // 检查是否为数组访问: a[i]
        if (current_token().type == TOKEN_LBRACKET) {
            advance();
            ASTNode *index = expression();
            expect(TOKEN_RBRACKET);
            ASTNode *node = new_node(NODE_ARRAY_ACCESS, tok.line);
            node->attr.name = strdup(tok.value);
            node->child = index;
            return node;
        }
        // 检查是否为函数调用
        if (current_token().type == TOKEN_LPAREN) {
            advance();
            ASTNode *call = new_node(NODE_FUNC_CALL, tok.line);
            call->attr.name = strdup(tok.value);
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
            return call;
        }
        ASTNode *node = new_node(NODE_LVAL, tok.line);
        node->attr.name = strdup(tok.value);
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

// 外部声明: 函数定义、全局变量或全局常量
static ASTNode *external_declaration(void) {
    bool is_const = false;
    if (current_token().type == TOKEN_KW_CONST) {
        is_const = true;
        advance();
    }
    
    DataType type = parse_type_spec();
    if (current_token().type != TOKEN_IDENTIFIER) {
        report_error(COMPILE_ERR_SYNTAX, "Expected identifier at line %d", current_token().line);
        synchronize();
        return NULL;
    }
    
    Token id = current_token();
    advance();
    
    // 情况 1: 函数定义
    if (current_token().type == TOKEN_LPAREN) {
        if (is_const) {
            report_error(COMPILE_ERR_SYNTAX, "Functions cannot be declared as const at line %d", id.line);
        }
        advance();
        ASTNode *params = NULL;
        if (current_token().type != TOKEN_RPAREN) {
            params = param_list();
        }
        expect(TOKEN_RPAREN);
        ASTNode *body = compound_stmt();
        ASTNode *func = new_node(NODE_FUNC_DEF, id.line);
        func->data_type = type;
        func->attr.name = strdup(id.value);
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
    
    // 情况 2: 全局变量或数组声明 (支持逗号分隔多个变量)
    ASTNode *first_decl = NULL;
    ASTNode *last_decl = NULL;
    
    bool first_time = true;
    while (true) {
        Token current_id;
        if (first_time) {
            current_id = id;
            first_time = false;
        } else {
            if (current_token().type != TOKEN_IDENTIFIER) {
                report_error(COMPILE_ERR_SYNTAX, "Expected identifier in multiple declaration at line %d", current_token().line);
                break;
            }
            current_id = current_token();
            advance();
        }

        NodeType node_type = is_const ? NODE_CONST_DECL : NODE_VAR_DECL;
        int array_size = -1;

        // 检查数组
        if (current_token().type == TOKEN_LBRACKET) {
            advance();
            if (current_token().type != TOKEN_INT_LITERAL) {
                report_error(COMPILE_ERR_SYNTAX, "Array size must be an integer constant at line %d", current_token().line);
            } else {
                array_size = atoi(current_token().value);
                advance();
            }
            expect(TOKEN_RBRACKET);
            node_type = NODE_ARRAY_DECL;
        }

        ASTNode *decl = new_node(node_type, current_id.line);
        decl->data_type = type;
        decl->attr.name = strdup(current_id.value);
        if (node_type == NODE_ARRAY_DECL) {
            decl->size = array_size;
        }

        // 初始化处理
        if (current_token().type == TOKEN_ASSIGN) {
            if (node_type == NODE_ARRAY_DECL) {
                report_error(COMPILE_ERR_SYNTAX, "Global array initialization not supported yet at line %d", current_id.line);
            }
            advance();
            ASTNode *init = expression();
            ASTNode *def = new_node(is_const ? NODE_CONST_DEF : NODE_VAR_DEF, current_id.line);
            def->child = init;
            def->attr.name = strdup(current_id.value);
            add_child(decl, def);
        } else if (is_const) {
            report_error(COMPILE_ERR_SYNTAX, "Global constant '%s' must be initialized at line %d", current_id.value, current_id.line);
        }

        if (!first_decl) first_decl = decl;
        else last_decl->sibling = decl;
        last_decl = decl;

        if (current_token().type == TOKEN_COMMA) {
            advance();
        } else {
            break;
        }
    }
    
    expect(TOKEN_SEMICOLON);
    return first_decl;
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
            if (!prog->child) {
                prog->child = decl;
            } else {
                tail->sibling = decl;
            }
            // 更新 tail 到链表的末尾
            tail = decl;
            while (tail->sibling) tail = tail->sibling;
        } else {
            synchronize();
        }
    }
    return prog;
}