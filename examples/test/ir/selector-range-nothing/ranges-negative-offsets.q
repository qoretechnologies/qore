%modern
%negative-offsets
# ranges with a bound that evaluates to NOTHING in a list of selectors, and other ranges in selector lists; see
# selector-range-nothing.qtest

list<int> l = (0, 1, 2, 3, 4);
*int k;

#! runs a case and prints its result or its exception
sub t(string name, code<auto()> c) {
    try {
        printf("%s: %y\n", name, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", name, ex.err, ex.desc);
    }
}

list<auto> sub multi_start() { return l[k..3, 0]; }
list<auto> sub multi_end() { return l[1..k, 0]; }
list<auto> sub multi_both() { return l[k..k, 0]; }
list<auto> sub multi_second() { return l[0, k..2]; }
list<auto> sub multi_negative() { return l[-2..-1, 0]; }
list<auto> sub multi_descending() { return l[3..1, 0]; }
list<auto> sub multi_mixed() { return l[1..-1, 0]; }
list<auto> sub single_start() { return l[k..3]; }
list<auto> sub single_end() { return l[1..k]; }
auto sub string_start() { return "abcde"[k..2, 0]; }
auto sub string_range() { return "abcde"[1..2, 0]; }

sub main() {
    # every case runs more than once, so that the functions are compiled with tiered execution
    for (int i = 0; i < 3; ++i) {
        t("multi start", \multi_start());
        t("multi end", \multi_end());
        t("multi both", \multi_both());
        t("multi second selector", \multi_second());
        t("multi negative", \multi_negative());
        t("multi descending", \multi_descending());
        t("multi mixed", \multi_mixed());
        t("single start", \single_start());
        t("single end", \single_end());
        t("string start", \string_start());
        t("string range", \string_range());
    }
}

main();
