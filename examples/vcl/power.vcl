// power.vcl — Fast integer exponentiation using a multiply helper
//
// 2^7 = 128
// Demonstrates: nested function calls, while loops

func multiply(a, b) {
    var result = 0;
    var i = 0;
    while (i < b) {
        result = result + a;
        i = i + 1;
    }
    return result;
}

func power(base, exp) {
    var result = 1;
    var i = 0;
    while (i < exp) {
        result = multiply(result, base);
        i = i + 1;
    }
    return result;
}

func main() {
    return power(2, 7);
}
