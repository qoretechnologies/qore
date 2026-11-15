# Copyright (C) 2026 Qore Technologies, s.r.o.
# Reads every character of an ASCII string in UTF-8 by index (issue #6138), and prints the time of each loop in
# seconds: a character offset in the ASCII prefix of a UTF-8 string is O(1), so the loops are linear
%modern

int sub reads(string src, int n) {
    int c = 0;
    for (int i = 0; i < n; ++i) {
        c += src[i].size();
    }
    return c;
}

int sub reads_length(string src) {
    int c = 0;
    # length() in the condition is evaluated in every iteration in AST code
    for (int i = 0; i < src.length(); ++i) {
        c += src[i].size();
    }
    return c;
}

int sub reads_backward(string src, int n) {
    int c = 0;
    for (int i = n - 1; i >= 0; --i) {
        c += src[i].size();
    }
    return c;
}

int n = ARGV[0] ? ARGV[0].toInt() : 200000;
string src = strmul("a", n);

date start = now_us();
int c = reads(src, n);
float index_time = (now_us() - start).durationSecondsFloat();
if (c != n) {
    throw "TIMING-ERROR", sprintf("index: %d != %d", c, n);
}

# a new string, whose cache is empty
src = strmul("b", n);
start = now_us();
c = reads_length(src);
float length_time = (now_us() - start).durationSecondsFloat();
if (c != n) {
    throw "TIMING-ERROR", sprintf("length: %d != %d", c, n);
}

src = strmul("c", n);
start = now_us();
c = reads_backward(src, n);
float backward_time = (now_us() - start).durationSecondsFloat();
if (c != n) {
    throw "TIMING-ERROR", sprintf("backward: %d != %d", c, n);
}

printf("index %.6f\nlength %.6f\nbackward %.6f\n", index_time, length_time, backward_time);
