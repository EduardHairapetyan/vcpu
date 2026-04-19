// clamp.vcl — Restrict a value to [lo, hi]
//
// clamp(200, 10, 100) = 100
// Demonstrates: multi-branch if, all six comparison ops

func clamp(val, lo, hi) {
    if (val < lo) {
        return lo;
    }
    if (val > hi) {
        return hi;
    }
    return val;
}

func main() {
    var a = clamp(200, 10, 100);   // too high  -> 100
    var b = clamp(3,   10, 100);   // too low   -> 10
    var c = clamp(50,  10, 100);   // in range  -> 50
    // return a + b + c = 100 + 10 + 50 = 160
    return a + b + c;
}
