// gcd.vcl — Greatest Common Divisor via Euclidean subtraction
//
// gcd(48, 36) = 12
// Demonstrates: while loop, if/else, comparison operators

func gcd(a, b) {
    while (a != b) {
        if (a > b) {
            a = a - b;
        } else {
            b = b - a;
        }
    }
    return a;
}

func main() {
    return gcd(48, 36);
}
