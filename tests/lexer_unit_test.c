#include "common.h"

#include <stdarg.h>

CompilerState g_state = {
    .error_count = 0,
    .max_errors = 20,
    .last_error_code = COMPILE_SUCCESS,
    .verbose = false,
    .dump_tokens = false,
    .dump_ast = false,
    .dump_ir = false,
    .input_filename = NULL,
    .output_filename = NULL
};

void report_error(CompileErrorCode code, const char *fmt, ...) {
    g_state.error_count++;
    g_state.last_error_code = code;
    if (g_state.verbose) {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
        va_end(args);
    }
}

bool should_stop(void) {
    return g_state.error_count >= g_state.max_errors;
}

void *safe_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    return ptr;
}

void *safe_realloc(void *ptr, size_t size) {
    void *next = realloc(ptr, size);
    if (!next && size > 0) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    return next;
}

static int failures = 0;

static void check_int(const char *name, int actual, int expected) {
    if (actual != expected) {
        fprintf(stderr, "FAIL %s: expected %d, got %d\n", name, expected, actual);
        failures++;
    }
}

static void check_str(const char *name, const char *actual, const char *expected) {
    if (!actual || strcmp(actual, expected) != 0) {
        fprintf(stderr, "FAIL %s: expected \"%s\", got \"%s\"\n",
                name, expected, actual ? actual : "(null)");
        failures++;
    }
}

static void check_token(TokenList *list, int index, TokenType type,
                        const char *value, int line, int column) {
    char label[64];
    snprintf(label, sizeof(label), "token[%d].type", index);
    check_int(label, list->tokens[index].type, type);
    snprintf(label, sizeof(label), "token[%d].value", index);
    check_str(label, list->tokens[index].value, value);
    snprintf(label, sizeof(label), "token[%d].line", index);
    check_int(label, list->tokens[index].line, line);
    snprintf(label, sizeof(label), "token[%d].column", index);
    check_int(label, list->tokens[index].column, column);
}

static void test_keywords_symbols_and_positions(void) {
    g_state.error_count = 0;
    TokenList *tokens = lex_analyze("int main() {\n  return a >= 10 && !b;\n}\n");

    check_int("token count", tokens->count, 15);
    check_int("lexical errors", g_state.error_count, 0);
    check_token(tokens, 0, TOKEN_KW_INT, "int", 1, 1);
    check_token(tokens, 1, TOKEN_IDENTIFIER, "main", 1, 5);
    check_token(tokens, 2, TOKEN_LPAREN, "(", 1, 9);
    check_token(tokens, 3, TOKEN_RPAREN, ")", 1, 10);
    check_token(tokens, 4, TOKEN_LBRACE, "{", 1, 12);
    check_token(tokens, 5, TOKEN_KW_RETURN, "return", 2, 3);
    check_token(tokens, 6, TOKEN_IDENTIFIER, "a", 2, 10);
    check_token(tokens, 7, TOKEN_GE, ">=", 2, 12);
    check_token(tokens, 8, TOKEN_INT_LITERAL, "10", 2, 15);
    check_token(tokens, 9, TOKEN_AND, "&&", 2, 18);
    check_token(tokens, 10, TOKEN_NOT, "!", 2, 21);
    check_token(tokens, 11, TOKEN_IDENTIFIER, "b", 2, 22);
    check_token(tokens, 12, TOKEN_SEMICOLON, ";", 2, 23);
    check_token(tokens, 13, TOKEN_RBRACE, "}", 3, 1);
    check_token(tokens, 14, TOKEN_EOF, "", 4, 1);

    free_token_list(tokens);
}

static void test_comments_and_illegal_characters(void) {
    g_state.error_count = 0;
    TokenList *tokens = lex_analyze("int a; // skip me\n/* block\ncomment */ a = @ + 1;\n");

    check_int("comment token count", tokens->count, 9);
    check_int("illegal character errors", g_state.error_count, 1);
    check_token(tokens, 0, TOKEN_KW_INT, "int", 1, 1);
    check_token(tokens, 1, TOKEN_IDENTIFIER, "a", 1, 5);
    check_token(tokens, 2, TOKEN_SEMICOLON, ";", 1, 6);
    check_token(tokens, 3, TOKEN_IDENTIFIER, "a", 3, 12);
    check_token(tokens, 4, TOKEN_ASSIGN, "=", 3, 14);
    check_token(tokens, 5, TOKEN_PLUS, "+", 3, 18);
    check_token(tokens, 6, TOKEN_INT_LITERAL, "1", 3, 20);
    check_token(tokens, 7, TOKEN_SEMICOLON, ";", 3, 21);
    check_token(tokens, 8, TOKEN_EOF, "", 4, 1);

    free_token_list(tokens);
}

static void test_unclosed_block_comment_reports_error(void) {
    g_state.error_count = 0;
    TokenList *tokens = lex_analyze("int a;\n/* unclosed\n");

    check_int("unclosed comment token count", tokens->count, 4);
    check_int("unclosed comment errors", g_state.error_count, 1);
    check_token(tokens, 0, TOKEN_KW_INT, "int", 1, 1);
    check_token(tokens, 1, TOKEN_IDENTIFIER, "a", 1, 5);
    check_token(tokens, 2, TOKEN_SEMICOLON, ";", 1, 6);
    check_token(tokens, 3, TOKEN_EOF, "", 3, 1);

    free_token_list(tokens);
}

int main(void) {
    test_keywords_symbols_and_positions();
    test_comments_and_illegal_characters();
    test_unclosed_block_comment_reports_error();

    if (failures > 0) {
        fprintf(stderr, "%d lexer test failure(s)\n", failures);
        return 1;
    }

    printf("lexer unit tests passed\n");
    return 0;
}
