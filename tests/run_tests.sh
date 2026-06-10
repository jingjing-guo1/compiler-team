#!/bin/bash

# 编译器路径（根据实际位置修改）
COMPILER="../mycc"
GCC="gcc"
TIMEOUT=5

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

PASSED=0
FAILED=0

# 执行单个测试
# 参数: 测试名称 源代码(多行) 期望结果类型(positive/negative) 期望返回值(仅positive)
run_test() {
    local name="$1"
    local src="$2"
    local type="$3"
    local expected_ret="$4"

    local tmp_dir=$(mktemp -d)
    local src_file="$tmp_dir/test.c"
    local asm_file="$tmp_dir/test.s"
    local exe_file="$tmp_dir/test"

    echo "$src" > "$src_file"

    # 编译
    $COMPILER "$src_file" -o "$asm_file" 2> "$tmp_dir/compiler_err"
    local comp_ret=$?

    if [ "$type" == "positive" ]; then
        if [ $comp_ret -ne 0 ]; then
            echo -e "${RED}[FAIL]${NC} $name : compilation failed (expected success)"
            cat "$tmp_dir/compiler_err"
            FAILED=$((FAILED+1))
            rm -rf "$tmp_dir"
            return
        fi
        # 汇编并链接
        $GCC -no-pie "$asm_file" -o "$exe_file" 2> "$tmp_dir/gcc_err"
        if [ $? -ne 0 ]; then
            echo -e "${RED}[FAIL]${NC} $name : assembly/linking failed"
            cat "$tmp_dir/gcc_err"
            FAILED=$((FAILED+1))
            rm -rf "$tmp_dir"
            return
        fi
        # 运行并捕获返回值
        timeout $TIMEOUT "$exe_file"
        local run_ret=$?
        if [ $run_ret -eq $expected_ret ]; then
            echo -e "${GREEN}[PASS]${NC} $name (return $run_ret)"
            PASSED=$((PASSED+1))
        else
            echo -e "${RED}[FAIL]${NC} $name : expected return $expected_ret, got $run_ret"
            FAILED=$((FAILED+1))
        fi
    else   # negative test
        if [ $comp_ret -eq 0 ]; then
            echo -e "${RED}[FAIL]${NC} $name : compilation succeeded but should have failed"
            FAILED=$((FAILED+1))
        else
            echo -e "${GREEN}[PASS]${NC} $name (compilation failed as expected)"
            PASSED=$((PASSED+1))
        fi
    fi

    rm -rf "$tmp_dir"
}

# ===================== 正向测试（应成功并返回值正确） =====================
run_test "basic arithmetic" '
int main() {
    int a;
    int b;
    a = 10;
    b = a + 2 * 3;
    return b;
}
' positive 16

run_test "relational and logical" '
int main() {
    int x;
    int y;
    x = 5;
    y = 3;
    if (x > y && y != 0)
        return 1;
    else
        return 0;
}
' positive 1

run_test "array access" '
int main() {
    int arr[5];
    int i;
    i = 2;
    arr[0] = 10;
    arr[i] = 20;
    return arr[0] + arr[2];
}
' positive 30

run_test "while loop with break/continue" '
int main() {
    int i;
    int sum;
    i = 0;
    sum = 0;
    while (i < 10) {
        i = i + 1;
        if (i == 5) continue;
        sum = sum + i;
        if (i == 8) break;
    }
    return sum;
}
' positive 31

run_test "function call" '
int add(int a, int b) {
    return a + b;
}
int main() {
    int x;
    int y;
    x = 7;
    y = 8;
    return add(x, y);
}
' positive 15

run_test "const variable" '
int main() {
    const int c = 100;
    return c;
}
' positive 100

run_test "logical short-circuit" '
int side_effect() {
    return 1;
}
int main() {
    int a = 0;
    int b = 1;
    if (a && side_effect())
        return 0;
    else
        return 1;
}
' positive 1

run_test "nested if-else" '
int main() {
    int x = 2;
    if (x > 0)
        if (x < 10)
            return 5;
        else
            return 6;
    else
        return 7;
}
' positive 5

run_test "array index with variable" '
int main() {
    int a[4];
    int idx = 1;
    a[0] = 10;
    a[idx] = 20;
    a[2] = 30;
    a[3] = 40;
    return a[0] + a[1] + a[2] + a[3];
}
' positive 100

run_test "void function call" '
void do_nothing() {
    return;
}
int main() {
    do_nothing();
    return 123;
}
' positive 123

run_test "local scope shadowing (current impl)" '
int main() {
    int x = 1;
    if (1) {
        int x = 2;
    }
    return x;
}
' positive 1

run_test "operator precedence" '
int main() {
    int a = 2;
    int b = 3;
    int c = 4;
    return a + b * c - (c / a);
}
' positive 12

run_test "modulo operator" '
int main() {
    return 17 % 5;
}
' positive 2

run_test "unary minus and logical not" '
int main() {
    int a = -5;
    int b = !a;
    if (b == 0)
        return 1;
    else
        return 0;
}
' positive 1

run_test "many arguments (6)" '
int many(int a, int b, int c, int d, int e, int f) {
    return a+b+c+d+e+f;
}
int main() {
    return many(1,2,3,4,5,6);
}
' positive 21

# ===================== 负向测试（应编译失败） =====================
run_test "missing semicolon" '
int main() {
    int a
    return 0;
}
' negative

run_test "undefined variable" '
int main() {
    return x;
}
' negative

run_test "assign to constant" '
int main() {
    const int c = 10;
    c = 20;
    return c;
}
' negative

run_test "array index non-integer (float literal)" '
int main() {
    int arr[5];
    arr[1.5] = 0;
    return 0;
}
' negative

run_test "function argument count mismatch" '
int foo(int a, int b) {
    return a + b;
}
int main() {
    return foo(5);
}
' negative

run_test "missing return in non-void function" '
int foo() {
    return;
}
' negative

run_test "void function returns value" '
void foo() {
    return 5;
}
' negative

run_test "break outside loop" '
int main() {
    break;
    return 0;
}
' negative

run_test "array used without index" '
int main() {
    int arr[5];
    arr = 10;
    return 0;
}
' negative

run_test "variable redefinition" '
int main() {
    int a;
    int a;
    return 0;
}
' negative

run_test "call undefined function" '
int main() {
    return unknown();
}
' negative

run_test "const without initializer" '
int main() {
    const int c;
    return c;
}
' negative

run_test "array size zero or negative" '
int main() {
    int arr[0];
    return 0;
}
' negative

run_test "void operand in comparison" '
void foo() {}
int main() {
    return foo() > 0;
}
' negative

# 汇总结果
echo ""
echo "=========================="
echo "Tests passed: $PASSED"
echo "Tests failed: $FAILED"
echo "=========================="

if [ $FAILED -eq 0 ]; then
    exit 0
else
    exit 1
fi