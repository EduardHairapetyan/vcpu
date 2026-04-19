// min3.vcl — Minimum of three values using a helper
//
// min3(42, 17, 99) = 17
// Demonstrates: chained function calls, comparison, if/else

func min2(a, b) {
    if (a < b) {
        return a;
    }
    return b;
}

func min3(a, b, c) {
    return min2(min2(a, b), c);
}

func main() {
    return min3(42, 17, 99);
}
