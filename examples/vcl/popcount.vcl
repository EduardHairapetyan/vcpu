// popcount.vcl — Count the number of 1-bits in a byte
//
// popcount(181) = popcount(0b10110101) = 5
// Demonstrates: bitwise AND, loop counter, 8-bit arithmetic wrap

func popcount(n) {
    var count = 0;
    var mask = 1;
    var i = 0;
    while (i < 8) {
        if ((n & mask) != 0) {
            count = count + 1;
        }
        mask = mask + mask;   // mask <<= 1  (wraps after bit 7, but i exits first)
        i = i + 1;
    }
    return count;
}

func main() {
    return popcount(181);   // 10110101 -> 5 set bits
}
