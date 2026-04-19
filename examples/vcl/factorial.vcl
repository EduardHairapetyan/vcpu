// factorial.vcl — 5! = 120, computed iteratively
func multiply(a, b) {
    var result = 0;
    var i = 0;
    while (i < b) {
        result = result + a;
        i = i + 1;
    }
    return result;
}

func factorial(n) {
    var result = 1;
    var i = 2;
    while (i <= n) {
        result = multiply(result, i);
        i = i + 1;
    }
    return result;
}

func main() {
    return factorial(5);
}
