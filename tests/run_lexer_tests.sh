#!/usr/bin/env bash
set -euo pipefail

mkdir -p build
gcc -Wall -g -Iinclude tests/lexer_unit_test.c src/lexer.c src/utils.c -o build/lexer_unit_test
./build/lexer_unit_test
