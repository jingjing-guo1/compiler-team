int main() {
    int value = 1;
    {
        int value = 2;
        value = value + 1;
    }
    return value;
}
