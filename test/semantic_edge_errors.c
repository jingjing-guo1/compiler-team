int add(int a, int b) {
    return a + b;
}

void no_value() {
    return;
}

int same_scope(int value) {
    int value;
    return value;
}

int main() {
    int scalar;
    int array[2];
    scalar[0] = 1;
    array = scalar;
    scalar = add(no_value(), 1);
    continue;
    return add(1);
}
