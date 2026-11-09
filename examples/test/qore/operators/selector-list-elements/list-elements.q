%modern
# lists of selectors on a list whose elements are lists; see selector-list-elements.qtest

list<auto> ll = ((1, 2), (3, 4), (5, 6));
list<list<int>> tl = ((1, 2), (3, 4), (5, 6));
list<auto> mixed = ((1, 2), "a", {"k": 1}, (3,));
list<int> il = (1, 2, 3);

#! runs a case and prints its result and the type of the result
sub t(string name, code<auto()> c) {
    try {
        auto v = c();
        printf("%s: %y (%s)\n", name, v, v.fullType());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", name, ex.err, ex.desc);
    }
}

auto sub range_and_index() { return ll[0..1, 2]; }
auto sub index_and_range() { return ll[2, 0..1]; }
auto sub indexes() { return ll[0, 2]; }
auto sub typed() { return tl[0..1, 2]; }
auto sub typed_indexes() { return tl[2, 0]; }
auto sub mixed_elements() { return mixed[0..1, 2, 3]; }
auto sub nested() { return ll[0..1, 2][1][0]; }
auto sub int_indexes() { return il[0, 2]; }
auto sub variable_indexes() { auto x = (0, 2); return il[x]; }
auto sub variable_index() { auto x = 1; return il[x]; }

sub main() {
    # every case runs more than once, so that the functions are compiled with tiered execution
    for (int i = 0; i < 3; ++i) {
        t("range and index", \range_and_index());
        t("index and range", \index_and_range());
        t("indexes", \indexes());
        t("typed", \typed());
        t("typed indexes", \typed_indexes());
        t("mixed elements", \mixed_elements());
        t("nested", \nested());
        t("int indexes", \int_indexes());
        t("variable indexes", \variable_indexes());
        t("variable index", \variable_index());
    }
}

main();
