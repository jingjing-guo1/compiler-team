#!/bin/bash

# 编译器路径（根据实际路径调整，Windows下如果用WSL需注意路径映射）
CC="../mycc.exe"
# 测试用例根目录
TEST_CASE_ROOT="./test_cases"
# 合法用例目录
VALID_DIR="${TEST_CASE_ROOT}/valid"
# 非法用例目录
INVALID_DIR="${TEST_CASE_ROOT}/invalid"

# 颜色定义（方便输出区分）
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # 重置颜色

# 统计变量
total=0
pass=0
fail=0

# 检查编译器是否存在
if [ ! -f "${CC}" ]; then
    echo -e "${RED}错误：编译器 ${CC} 不存在！${NC}"
    exit 1
fi

# 检查测试目录是否存在
if [ ! -d "${TEST_CASE_ROOT}" ]; then
    echo -e "${RED}错误：测试用例根目录 ${TEST_CASE_ROOT} 不存在！${NC}"
    exit 1
fi

# 测试单个文件的函数
# 参数1：文件路径；参数2：是否期望成功（0=失败，1=成功）
test_file() {
    local file="$1"
    local expect_success="$2"
    local filename=$(basename "${file}")
    total=$((total + 1))

    # 执行编译器，捕获退出码和输出
    output=$("${CC}" "${file}" 2>&1)
    exit_code=$?

    # 判断是否符合预期
    if [ "${expect_success}" -eq 1 ]; then
        # 合法用例：期望编译成功（退出码0）
        if [ "${exit_code}" -eq 0 ]; then
            echo -e "${GREEN}[PASS]${NC} 合法用例: ${filename}"
            pass=$((pass + 1))
        else
            echo -e "${RED}[FAIL]${NC} 合法用例: ${filename} (编译失败，退出码: ${exit_code})"
            echo -e "${YELLOW}输出: ${output}${NC}"
            fail=$((fail + 1))
        fi
    else
        # 非法用例：期望编译失败（退出码非0）
        if [ "${exit_code}" -ne 0 ]; then
            echo -e "${GREEN}[PASS]${NC} 非法用例: ${filename}"
            pass=$((pass + 1))
        else
            echo -e "${RED}[FAIL]${NC} 非法用例: ${filename} (编译成功，不符合预期)"
            echo -e "${YELLOW}输出: ${output}${NC}"
            fail=$((fail + 1))
        fi
    fi
}

# 测试合法用例
echo -e "\n========== 测试合法用例 (valid) =========="
if [ -d "${VALID_DIR}" ]; then
    for file in "${VALID_DIR}"/*.c; do
        # 跳过空文件（如果目录下无.c文件）
        [ -f "${file}" ] || continue
        test_file "${file}" 1
    done
else
    echo -e "${YELLOW}警告：合法用例目录 ${VALID_DIR} 不存在！${NC}"
fi

# 测试非法用例
echo -e "\n========== 测试非法用例 (invalid) =========="
if [ -d "${INVALID_DIR}" ]; then
    for file in "${INVALID_DIR}"/*.c; do
        # 跳过空文件（如果目录下无.c文件）
        [ -f "${file}" ] || continue
        test_file "${file}" 0
    done
else
    echo -e "${YELLOW}警告：非法用例目录 ${INVALID_DIR} 不存在！${NC}"
fi

# 输出测试总结
echo -e "\n========== 测试总结 =========="
echo "总用例数: ${total}"
echo -e "${GREEN}通过数: ${pass}${NC}"
echo -e "${RED}失败数: ${fail}${NC}"
echo "通过率: $(( pass * 100 / total ))%"

# 脚本退出码：0=全部通过，1=有失败
if [ "${fail}" -eq 0 ]; then
    exit 0
else
    exit 1
fi