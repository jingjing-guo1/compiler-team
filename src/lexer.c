/**
 * @file lexer.c
 * @brief 词法分析器（支持跳过非法字符，收集多个错误）
 */

#include "common.h"
#include <ctype.h>

static char *my_strndup(const char *s, size_t n) {
    char *p = (char*)safe_malloc(n + 1);
    if (!p) return NULL;
    strncpy(p, s, n);
    p[n] = '\0';
    return p;
}

static void add_token(TokenList *list, TokenType type, const char *value, int line) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity == 0 ? 16 : list->capacity * 2;
        list->tokens = (Token*)safe_realloc(list->tokens, list->capacity * sizeof(Token));
    }
    Token *tok = &list->tokens[list->count++];
    tok->type = type;
    tok->value = value ? strdup(value) : NULL;
    tok->line = line;
}

static TokenType keyword_type(const char *word) {
    if (strcmp(word, "if") == 0) return TOKEN_IF;
    if (strcmp(word, "then") == 0) return TOKEN_THEN;
    if (strcmp(word, "else") == 0) return TOKEN_ELSE;
    if (strcmp(word, "end") == 0) return TOKEN_END;
    if (strcmp(word, "repeat") == 0) return TOKEN_REPEAT;
    if (strcmp(word, "until") == 0) return TOKEN_UNTIL;
    if (strcmp(word, "read") == 0) return TOKEN_READ;
    if (strcmp(word, "write") == 0) return TOKEN_WRITE;
    return TOKEN_ID;
}

TokenList *lex_analyze(const char *source_code) {
    TokenList *list = (TokenList*)safe_malloc(sizeof(TokenList));
    list->tokens = NULL;
    list->count = 0;
    list->capacity = 0;

    const char *p = source_code;
    int line = 1;

    while (*p) {
        // 跳过空格、制表符
        if (*p == ' ' || *p == '\t') { p++; continue; }
        // 换行（支持 Windows \r\n 和 Unix \n）
        if (*p == '\n') { line++; p++; continue; }
        if (*p == '\r') { p++; continue; }   // 关键：忽略回车符
        // 注释 { ... }
        if (*p == '{') {
            p++;
            while (*p && *p != '}') {
                if (*p == '\n') line++;
                p++;
            }
            if (*p == '}') p++;
            else report_error(COMPILE_ERR_LEXICAL, "Unclosed comment at line %d", line);
            continue;
        }
        // 识别数字
        if (isdigit(*p)) {
            const char *start = p;
            while (isdigit(*p)) p++;
            char *num = my_strndup(start, p - start);
            add_token(list, TOKEN_NUM, num, line);
            free(num);
            continue;
        }
        // 识别标识符或关键字
        if (isalpha(*p)) {
            const char *start = p;
            while (isalnum(*p)) p++;
            char *word = my_strndup(start, p - start);
            TokenType type = keyword_type(word);
            add_token(list, type, type == TOKEN_ID ? word : NULL, line);
            free(word);
            continue;
        }
        // 识别运算符和界符
        switch (*p) {
            case ':':
                if (*(p+1) == '=') {
                    add_token(list, TOKEN_ASSIGN, NULL, line);
                    p += 2;
                } else {
                    report_error(COMPILE_ERR_LEXICAL, "Unexpected ':' at line %d", line);
                    p++;
                }
                break;
            case '=': add_token(list, TOKEN_EQ, NULL, line); p++; break;
            case '<': add_token(list, TOKEN_LT, NULL, line); p++; break;
            case '+': add_token(list, TOKEN_PLUS, NULL, line); p++; break;
            case '-': add_token(list, TOKEN_MINUS, NULL, line); p++; break;
            case '*': add_token(list, TOKEN_TIMES, NULL, line); p++; break;
            case '/': add_token(list, TOKEN_OVER, NULL, line); p++; break;
            case '(': add_token(list, TOKEN_LPAREN, NULL, line); p++; break;
            case ')': add_token(list, TOKEN_RPAREN, NULL, line); p++; break;
            case ';': add_token(list, TOKEN_SEMICOLON, NULL, line); p++; break;
            default:
                // 更安全的错误报告：只打印可打印字符
                if (isprint((unsigned char)*p))
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