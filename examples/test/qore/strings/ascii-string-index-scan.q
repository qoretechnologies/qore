# Copyright (C) 2026 Qore Technologies, s.r.o.
# Reads the characters of ASCII strings in UTF-8 by index (issue #6138), and prints how many bytes the character
# offset functions walked or scanned for them (dbg_get_string_scan_bytes(), debug builds): a character offset in the
# ASCII text at the start of a UTF-8 string is O(1), so a loop over the characters scans each byte about once, and a
# read at any offset of a string whose ASCII text is known scans nothing.  With the cache disabled
# (dbg_set_ascii_prefix_cache()), as before the change, a read at offset i walks i bytes.
%modern

#! returns the bytes scanned so far
int sub scanned() {
    return call_function("dbg_get_string_scan_bytes");
}

#! checks that the loop read every character
sub check(string what, int c, int n) {
    if (c != n) {
        throw "SCAN-ERROR", sprintf("%s: %d characters read, expected %d", what, c, n);
    }
}

#! returns the bytes scanned by a loop that reads every character of a new string of n characters
int sub loop_index(int n) {
    string src = strmul("a", n);
    int start = scanned();
    int c = 0;
    for (int i = 0; i < n; ++i) {
        c += src[i].size();
    }
    check("index", c, n);
    return scanned() - start;
}

#! as loop_index(), with length() in the loop condition, which is evaluated in every iteration in AST code
int sub loop_length(int n) {
    string src = strmul("b", n);
    int start = scanned();
    int c = 0;
    for (int i = 0; i < src.length(); ++i) {
        c += src[i].size();
    }
    check("length", c, n);
    return scanned() - start;
}

#! as loop_index(), from the last character to the first
int sub loop_backward(int n) {
    string src = strmul("c", n);
    int start = scanned();
    int c = 0;
    for (int i = n - 1; i >= 0; --i) {
        c += src[i].size();
    }
    check("backward", c, n);
    return scanned() - start;
}

#! returns the bytes scanned by a read at each of the offsets of a string whose ASCII text is known
hash<string, int> sub reads_at(list<int> offsets) {
    string src = strmul("d", offsets.last() + 1);
    # the first call scans the string
    src.length();
    hash<string, int> rv;
    foreach int i in (offsets) {
        int start = scanned();
        string ch = src[i];
        if (ch != "d") {
            throw "SCAN-ERROR", sprintf("offset %d: %y", i, ch);
        }
        rv{i} = scanned() - start;
    }
    int start = scanned();
    int len = src.length();
    if (len != offsets.last() + 1) {
        throw "SCAN-ERROR", sprintf("length: %d", len);
    }
    rv.length = scanned() - start;
    return rv;
}

# the counter is only available in debug builds; the test that runs this script skips on this line
if (!exists_function("dbg_get_string_scan_bytes")) {
    printf("skip: dbg_get_string_scan_bytes() is only available in debug builds\n");
    exit(0);
}

list<int> offsets = (1000, 10000, 100000);
foreach int n in ((50000, 200000)) {
    printf("index %d %d\n", n, loop_index(n));
    printf("length %d %d\n", n, loop_length(n));
    printf("backward %d %d\n", n, loop_backward(n));
}
hash<string, int> reads = reads_at(offsets);
foreach hash<auto> i in (reads.pairIterator()) {
    printf("read %s %d\n", i.key, i.value);
}
# with the cache disabled, as before the change: the counter sees the walk from the start of the string
bool enabled = call_function("dbg_set_ascii_prefix_cache", False);
on_exit call_function("dbg_set_ascii_prefix_cache", enabled);
reads = reads_at(offsets);
foreach hash<auto> i in (reads.pairIterator()) {
    printf("uncached-read %s %d\n", i.key, i.value);
}
