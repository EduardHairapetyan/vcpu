// max.vcl — if/else, comparison
func max(a, b) {
    if (a > b) {
        return a;
    } else {
        return b;
    }
    return 0;
}

func main() {
    var m = max(42, 17);
    return m;
}
