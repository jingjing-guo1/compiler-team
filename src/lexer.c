/**
 * @file lexer.c
 * @brief 词法分析器（C语言子集）
 */

#include "common.h"
#include <ctype.h>

static char *my_strndup(const char *s, size_t n) {
    char *p = (char*)safe_malloc(n + 1);
    strncpy(p, s, n);
    p[n] = '\0';
    return p;
}

static void add_token(TokenList *list, TokenType type, const char *value, int line) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity == 0 ? 128 : list->capacity * 2;
        list->tokens = (Token*)safe_realloc(list->tokens, list->capacity * sizeof(Token));
    }
    Token *tok = &list->tokens[list->count++];
    tok->type = type;
    tok->value = value ? strdup(value) : NULL;
    tok->line = line;
    tok->column = 0;  // 简化
}

static TokenType keyword_type(const char *word) {
    if (strcmp(word, "int") == 0) return TOKEN_KW_INT;
    if (strcmp(word, "void") == 0) return TOKEN_KW_VOID;
    if (strcmp(word, "if") == 0) return TOKEN_KW_IF;
    if (strcmp(word, "else") == 0) return TOKEN_KW_ELSE;
    if (strcmp(word, "while") == 0) return TOKEN_KW_WHILE;
    if (strcmp(word, "return") == 0) return TOKEN_KW_RETURN;
    if (strcmp(word, "const") == 0) return TOKEN_KW_CONST;
    if (strcmp(word, "break") == 0) return TOKEN_KW_BREAK;
    if (strcmp(word, "continue") == 0) return TOKEN_KW_CONTINUE;
    return TOKEN_IDENTIFIER;
}

TokenList *lex_analyze(const char *source_code) {
    TokenList *list = (TokenList*)safe_malloc(sizeof(TokenList));
    list->tokens = NULL;
    list->count = 0;
    list->capacity = 0;

    const char *p = source_code;
    int line = 1;

    while (*p) {
        if (*p == ' ' || *p == '\t') { p++; continue; }
        if (*p == '\n') { line++; p++; continue; }
        if (*p == '\r') { p++; continue; }

        // 注释处理
        if (*p == '/') {
            if (*(p+1) == '/') {  // 单行注释
                p += 2;
                while (*p && *p != '\n') p++;
                continue;
            } else if (*(p+1) == '*') {  // 多行注释
                p += 2;
                while (*p && !(*p == '*' && *(p+1) == '/')) {
                    if (*p == '\n') line++;
                    p++;
                }
                if (*p == '*' && *(p+1) == '/') p += 2;
                else report_error(COMPILE_ERR_LEXICAL, "Unclosed comment at line %d", line);
                continue;
            }
        }

        // 数字
        if (isdigit(*p)) {
            const char *start = p;
            while (isdigit(*p)) p++;
            char *num = my_strndup(start, p - start);
            add_token(list, TOKEN_INT_LITERAL, num, line);
            free(num);
            continue;
        }

        // 标识符/关键字
        if (isalpha(*p) || *p == '_') {
            const char *start = p;
            while (isalnum(*p) || *p == '_') p++;
            char *word = my_strndup(start, p - start);
            TokenType type = keyword_type(word);
            add_token(list, type, (type == TOKEN_IDENTIFIER) ? word : NULL, line);
            free(word);
            continue;
        }

        // 双字符运算符
        if (*p == '=' && *(p+1) == '=') {
            add_token(list, TOKEN_EQ, NULL, line);
            p += 2;
            continue;
        }
        if (*p == '!' && *(p+1) == '=') {
            add_token(list, TOKEN_NEQ, NULL, line);
            p += 2;
            continue;
        }
        if (*p == '<' && *(p+1) == '=') {
            add_token(list, TOKEN_LE, NULL, line);
            p += 2;
            continue;
        }
        if (*p == '>' && *(p+1) == '=') {
            add_token(list, TOKEN_GE, NULL, line);
            p += 2;
            continue;
        }
        if (*p == '&' && *(p+1) == '&') {
            add_token(list, TOKEN_AND, NULL, line);
            p += 2;
            continue;
        }
        if (*p == '|' && *(p+1) == '|') {
            add_token(list, TOKEN_OR, NULL, line);
            p += 2;
            continue;
        }

        // 单字符运算符/界符
        switch (*p) {
            case '+': add_token(list, TOKEN_PLUS, NULL, line); p++; break;
            case '-': add_token(list, TOKEN_MINUS, NULL, line); p++; break;
            case '*': add_token(list, TOKEN_MUL, NULL, line); p++; break;
            case '/': add_token(list, TOKEN_DIV, NULL, line); p++; break;
            case '%': add_token(list, TOKEN_MOD, NULL, line); p++; break;
            case '=': add_token(list, TOKEN_ASSIGN, NULL, line); p++; break;
            case '<': add_token(list, TOKEN_LT, NULL, line); p++; break;
            case '>': add_token(list, TOKEN_GT, NULL, line); p++; break;
            case '!': add_token(list, TOKEN_NOT, NULL, line); p++; break;
            case ';': add_token(list, TOKEN_SEMICOLON, NULL, line); p++; break;
            case ',': add_token(list, TOKEN_COMMA, NULL, line); p++; break;
            case '(': add_token(list, TOKEN_LPAREN, NULL, line); p++; break;
            case ')': add_token(list, TOKEN_RPAREN, NULL, line); p++; break;
            case '{': add_token(list, TOKEN_LBRACE, NULL, line); p++; break;
            case '}': add_token(list, TOKEN_RBRACE, NULL, line); p++; break;
            case '[': add_token(list, TOKEN_LBRACKET, NULL, line); p++; break;
            case ']': add_token(list, TOKEN_RBRACKET, NULL, line); p++; break;
            default:
                if (isprint(*p))
                    report_error(COMPILE_ERR_LEXICAL, "Illegal character '%c' at line %d", *p, line);
                else
                    report_error(COMPILE_ERR_LEXICAL, "Illegal character (ASCII %d) at line %d", *p, line);
                p++;
                break;
        }
    }
    add_token(list, TOKEN_EOF, NULL, line);
    return list;
}