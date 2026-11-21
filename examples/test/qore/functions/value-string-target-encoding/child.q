%modern
%enable-all-warnings

# run with QORE_CHARSET=ISO-8859-1: the string literals of this UTF-8 source are UTF-8 (#6816), the default encoding is
# ISO-8859-1; the numbers, booleans and dates converted for a UTF-8 target must take that encoding
sub out(string key, string val) {
    printf("%s=%s=%s\n", key, val.encoding(), make_hex_string(binary(val)));
}

printf("default=%s\n", get_default_encoding());
out("join-int", join(",", (1, 23)));
out("join-float", join(",", (1.5, 2.25)));
out("join-bool", join(",", (True, False)));
out("join-number", join(",", (1.5n, 3n)));
out("join-date", join(",", (2026-01-02, 2026-03-04)));
out("join-mixed", join(",", (1, 2.5, True, 3n, "x")));
out("plus-int", "," + 7);
out("plus-float", "," + 0.5);
out("plus-bool", "," + True);
out("plus-number", "," + 2.5n);
