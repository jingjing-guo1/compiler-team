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

static char *my_strdup(const char *s) {
    return my_strndup(s, strlen(s));
}

static void add_token(TokenList *list, TokenType type, const char *value, int line, int column) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity == 0 ? 128 : list->capacity * 2;
        list->tokens = (Token*)safe_realloc(list->tokens, list->capacity * sizeof(Token));
    }
    Token *tok = &list->tokens[list->count++];
    tok->type = type;
    tok->value = value ? my_strdup(value) : NULL;
    tok->line = line;
    tok->column = column;
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
    int column = 1;

    while (*p) {
        unsigned char ch = (unsigned char)*p;

        if (ch == ' ' || ch == '\t' || ch == '\v' || ch == '\f') {
            p++;
            column++;
            continue;
        }
        if (ch == '\n') {
            line++;
            column = 1;
            p++;
            continue;
        }
        if (ch == '\r') {
            if (*(p + 1) == '\n') p++;
            line++;
            column = 1;
            p++;
            continue;
        }

        // 注释处理
        if (*p == '/') {
            if (*(p+1) == '/') {  // 单行注释
                p += 2;
                column += 2;
                while (*p && *p != '\n') p++;
                continue;
            } else if (*(p+1) == '*') {  // 多行注释
                int start_line = line;
                int start_column = column;
                p += 2;
                column += 2;
                while (*p && !(*p == '*' && *(p+1) == '/')) {
                    if (*p == '\n') {
                        line++;
                        column = 1;
                    } else if (*p == '\r') {
                        if (*(p + 1) == '\n') p++;
                        line++;
                        column = 1;
                    } else {
                        column++;
                    }
                    p++;
                }
                if (*p == '*' && *(p+1) == '/') {
                    p += 2;
                    column += 2;
                } else {
                    report_error(COMPILE_ERR_LEXICAL,
                                 "Unclosed comment starting at line %d, column %d",
                                 start_line, start_column);
                    if (should_stop()) break;
                }
                continue;
            }
        }

        // 数字
        if (isdigit(ch)) {
            const char *start = p;
            int start_column = column;
            while (isdigit((unsigned char)*p)) {
                p++;
                column++;
            }
            char *num = my_strndup(start, p - start);
            add_token(list, TOKEN_INT_LITERAL, num, line, start_column);
            free(num);
            continue;
        }

        // 标识符/关键字
        if (isalpha(ch) || *p == '_') {
            const char *start = p;
            int start_column = column;
            while (isalnum((unsigned char)*p) || *p == '_') {
                p++;
                column++;
            }
            char *word = my_strndup(start, p - start);
            TokenType type = keyword_type(word);
            add_token(list, type, word, line, start_column);
            free(word);
            continue;
        }

        // 双字符运算符
        if (*p == '=' && *(p+1) == '=') {
            add_token(list, TOKEN_EQ, "==", line, column);
            p += 2;
            column += 2;
            continue;
        }
        if (*p == '!' && *(p+1) == '=') {
            add_token(list, TOKEN_NEQ, "!=", line, column);
            p += 2;
            column += 2;
            continue;
        }
        if (*p == '<' && *(p+1) == '=') {
            add_token(list, TOKEN_LE, "<=", line, column);
            p += 2;
            column += 2;
            continue;
        }
        if (*p == '>' && *(p+1) == '=') {
            add_token(list, TOKEN_GE, ">=", line, column);
            p += 2;
            column += 2;
            continue;
        }
        if (*p == '&' && *(p+1) == '&') {
            add_token(list, TOKEN_AND, "&&", line, column);
            p += 2;
            column += 2;
            continue;
        }
        if (*p == '|' && *(p+1) == '|') {
            add_token(list, TOKEN_OR, "||", line, column);
            p += 2;
            column += 2;
            continue;
        }

        // 单字符运算符/界符
        switch (*p) {
            case '+': add_token(list, TOKEN_PLUS, "+", line, column); p++; column++; break;
            case '-': add_token(list, TOKEN_MINUS, "-", line, column); p++; column++; break;
            case '*': add_token(list, TOKEN_MUL, "*", line, column); p++; column++; break;
            case '/': add_token(list, TOKEN_DIV, "/", line, column); p++; column++; break;
            case '%': add_token(list, TOKEN_MOD, "%", line, column); p++; column++; break;
            case '=': add_token(list, TOKEN_ASSIGN, "=", line, column); p++; column++; break;
            case '<': add_token(list, TOKEN_LT, "<", line, column); p++; column++; break;
            case '>': add_token(list, TOKEN_GT, ">", line, column); p++; column++; break;
            case '!': add_token(list, TOKEN_NOT, "!", line, column); p++; column++; break;
            case ';': add_token(list, TOKEN_SEMICOLON, ";", line, column); p++; column++; break;
            case ',': add_token(list, TOKEN_COMMA, ",", line, column); p++; column++; break;
            case '(': add_token(list, TOKEN_LPAREN, "(", line, column); p++; column++; break;
            case ')': add_token(list, TOKEN_RPAREN, ")", line, column); p++; column++; break;
            case '{': add_token(list, TOKEN_LBRACE, "{", line, column); p++; column++; break;
            case '}': add_token(list, TOKEN_RBRACE, "}", line, column); p++; column++; break;
            case '[': add_token(list, TOKEN_LBRACKET, "[", line, column); p++; column++; break;
            case ']': add_token(list, TOKEN_RBRACKET, "]", line, column); p++; column++; break;
            default:
                if (isprint(ch))
                    report_error(COMPILE_ERR_LEXICAL, "Illegal character '%c' at line %d, column %d", *p, line, column);
                else
                    report_error(COMPILE_ERR_LEXICAL, "Illegal character (ASCII %d) at line %d, column %d", ch, line, column);
                p++;
                column++;
                if (should_stop()) break;
                break;
        }
    }
    add_token(list, TOKEN_EOF, "", line, column);
    return list;
}
