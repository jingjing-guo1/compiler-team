/**
 * @file main.c
 * @brief 主控程序：参数解析、各阶段调用、错误处理（多错误支持）
 */

#include "common.h"

CompilerState g_state = {0};

/** 报告错误（增加计数，不退出） */
void report_error(CompileErrorCode code, const char *fmt, ...) {
    g_state.error_count++;
    g_state.last_error_code = code;
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    if (g_state.verbose) {
        fprintf(stderr, "[Total errors: %d]\n", g_state.error_count);
    }
}

/** 检查是否应停止（超过最大错误数） */
bool should_stop(void) {
    if (g_state.error_count >= g_state.max_errors) {
        fprintf(stderr, "Fatal: too many errors (%d), compilation aborted.\n", g_state.error_count);
        return true;
    }
    return false;
}

/** 安全内存分配 */
void *safe_malloc(size_t size) {
    void *p = malloc(size);
    if (!p) {
        report_error(COMPILE_ERR_MEMORY, "Out of memory");
        exit(1);
    }
    return p;
}

/** 安全内存重分配 */
void *safe_realloc(void *ptr, size_t size) {
    void *p = realloc(ptr, size);
    if (!p && size) {
        report_error(COMPILE_ERR_MEMORY, "Out of memory");
        exit(1);
    }
    return p;
}

/** 打印帮助信息 */
static void print_usage(const char *prog) {
    printf("Usage: %s [options] <input.sy>\n", prog);
    printf("Options:\n");
    printf("  -o <file>     Output assembly file (default: output.s)\n");
    printf("  -v            Verbose output\n");
    printf("  --dump-tokens Print token list\n");
    printf("  --dump-ast    Print abstract syntax tree\n");
    printf("  --dump-ir     Print intermediate code\n");
    printf("  -h, --help    Show this help\n");
}

/** 解析命令行参数 */
static void parse_args(int argc, char *argv[]) {
    g_state.output_filename = "output.s";
    g_state.max_errors = 20;      // 最多报告20个错误
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            g_state.verbose = true;
        } else if (strcmp(argv[i], "--dump-tokens") == 0) {
            g_state.dump_tokens = true;
        } else if (strcmp(argv[i], "--dump-ast") == 0) {
            g_state.dump_ast = true;
        } else if (strcmp(argv[i], "--dump-ir") == 0) {
            g_state.dump_ir = true;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            g_state.output_filename = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else if (argv[i][0] != '-') {
            g_state.input_filename = argv[i];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            exit(1);
        }
    }
    if (!g_state.input_filename) {
        fprintf(stderr, "No input file.\n");
        print_usage(argv[0]);
        exit(1);
    }
}

/** 读取整个文件到字符串 */
static char *read_file(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        report_error(COMPILE_ERR_FILE_IO, "Cannot open file: %s", filename);
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    rewind(fp);
    char *buf = (char*)safe_malloc(len + 1);
    if (!buf) {
        fclose(fp);
        return NULL;
    }
    size_t read_len = fread(buf, 1, len, fp);
    buf[read_len] = '\0';
    fclose(fp);
    return buf;
}

int main(int argc, char *argv[]) {
    parse_args(argc, argv);
    g_state.error_count = 0;

    if (g_state.verbose) {
        printf("TINY Compiler (multi-error support)\n");
        printf("Input:  %s\n", g_state.input_filename);
        printf("Output: %s\n", g_state.output_filename);
    }

    char *source = read_file(g_state.input_filename);
    if (!source) return 1;

    // 1. 词法分析
    TokenList *tokens = lex_analyze(source);
    if (!tokens) {
        report_error(COMPILE_ERR_LEXICAL, "Failed to create token list");
        free(source);
        return 1;
    }
    if (g_state.dump_tokens) dump_tokens(tokens);
    if (g_state.verbose) printf("Lexical analysis: %d tokens, %d errors\n",
                                tokens->count, g_state.error_count);
    if (should_stop()) goto free_tokens;

    // 2. 语法分析
    ASTNode *ast = parse(tokens);
    if (g_state.dump_ast && ast) dump_ast(ast, 0);
    if (g_state.verbose) printf("Syntax analysis: %d errors\n", g_state.error_count);
    if (should_stop()) goto free_ast;

    // 3. 语义分析
    init_symbol_table();
    semantic_analyze(ast);
    if (g_state.verbose) printf("Semantic analysis: %d errors\n", g_state.error_count);
    if (should_stop()) goto free_sym;

    // 4. 中间代码生成（仅当无错误时才生成）
    IRCode *ir = NULL;
    if (g_state.error_count == 0) {
        ir = gen_ir(ast);
        if (!ir) {
            report_error(COMPILE_ERR_IRGEN, "IR generation failed");
            goto free_sym;
        }
        if (g_state.dump_ir) dump_ir(ir);
        if (g_state.verbose) printf("IR generation: %d quads\n", ir->count);
    } else {
        if (g_state.verbose) printf("Skipping IR generation due to %d errors.\n", g_state.error_count);
    }

    // 5. 目标代码生成（仅当无错误时）
    if (g_state.error_count == 0 && ir) {
        gen_assembly(ir, g_state.output_filename);
        if (g_state.verbose) printf("Assembly written to %s\n", g_state.output_filename);
    } else {
        fprintf(stderr, "Compilation failed with %d errors, no assembly generated.\n", g_state.error_count);
    }

    if (ir) free_ir(ir);
free_sym:
    free_symbol_table();
free_ast:
    free_ast(ast);
free_tokens:
    free_token_list(tokens);
    free(source);
    return g_state.error_count == 0 ? 0 : 1;
}