int main() {
    int value = 3;
    while (value > 0) {
        value = value - 1;
        if (value == 1) {
            continue;
        }
        if (value == 0) {
            break;
        }
    }
    return value;
}
