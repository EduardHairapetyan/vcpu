// add.vcl — function call and arithmetic
func add(a, b) {
    return a + b;
}

func main() {
    var x = add(10, 20);
    var y = add(x, 5);
    return y;
}
