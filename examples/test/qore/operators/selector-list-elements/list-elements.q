%modern
# selectors and indexes of every kind on lists, lists of lists, strings and binaries; see selector-list-elements.qtest

list<auto> ll = ((1, 2), (3, 4), (5, 6));
list<list<int>> tl = ((1, 2), (3, 4), (5, 6));
list<auto> mixed = ((1, 2), "a", {"k": 1}, (3,));
list<int> il = (1, 2, 3);
list<int> m = (10, 20, 30, 40, 50);
string str = "abcde";
binary bin = binary("abcde");

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
int sub f(int n) { return n; }
auto sub call_selectors() { return m[f(1), 4]; }
auto sub variable_selectors() { int i = 1; return m[i, 4]; }
auto sub typed_index_list() { list<int> x = (0, 2); return m[x]; }
auto sub soft_index_list() { softlist<int> x = (3, 1); return m[x]; }
auto sub param_index(auto x) { return m[x]; }
auto sub param_index_list() { return param_index((1, 3)); }
auto sub param_negative_index_list() { return param_index((-1, 0)); }
auto sub param_scalar_index() { return param_index(2); }
auto sub string_indexes() { return str[0, 2]; }
auto sub string_variable_indexes() { auto x = (4, 1); return str[x]; }
auto sub binary_indexes() { return bin[0, 2]; }
auto sub binary_variable_indexes() { auto x = (4, 1); return bin[x]; }
# scalar indexes that are not known to be integers: these take the native element access when they hold an integer
auto sub nullable_index() { *int x = 2; return m[x]; }
auto sub nullable_nothing_index() { *int x; return m[x]; }
auto sub auto_indexes() {
    list<auto> rv = ();
    foreach auto x in (1, (0, 2), NOTHING, "3", 1.0, -1, 9) {
        push rv, m[x];
    }
    return rv;
}
auto sub nullable_loop() {
    int s = 0;
    *int x = 0;
    for (int k = 0; k < 10; ++k) {
        s += m[x];
        x = (x + 1) % 5;
    }
    return s;
}
auto sub auto_loop() {
    int s = 0;
    auto x = 0;
    for (int k = 0; k < 10; ++k) {
        s += m[x];
        x = (x + 1) % 5;
    }
    return s;
}

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
        t("call selectors", \call_selectors());
        t("variable selectors", \variable_selectors());
        t("typed index list", \typed_index_list());
        t("soft index list", \soft_index_list());
        t("param index list", \param_index_list());
        t("param negative index list", \param_negative_index_list());
        t("param index", \param_scalar_index());
        t("string indexes", \string_indexes());
        t("string variable indexes", \string_variable_indexes());
        t("binary indexes", \binary_indexes());
        t("binary variable indexes", \binary_variable_indexes());
        t("nullable index", \nullable_index());
        t("nullable nothing index", \nullable_nothing_index());
        t("auto indexes", \auto_indexes());
        t("nullable loop", \nullable_loop());
        t("auto loop", \auto_loop());
    }
}

main();
