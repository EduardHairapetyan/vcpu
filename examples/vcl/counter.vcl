// counter.vcl — count from 0 to 4, store result in global
var result = 0;

func main() {
    var i = 0;
    while (i < 5) {
        result = result + 1;
        i = i + 1;
    }
    return result;
}
