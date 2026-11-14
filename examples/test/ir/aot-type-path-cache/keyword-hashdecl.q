%modern
%disable-warning unreferenced-variable

# a hashdecl can have the name of a builtin type keyword, so hash<softint> names that hashdecl and not a builtin type:
# its path depends on the namespaces of the Program and is not kept at the address of the path (qore#5979)

hashdecl softint {
    int a;
}

hashdecl any {
    string s;
}

sub show(string label, auto value) {
    printf("%s: %y (%s)\n", label, value, value.fullType());
}

sub run() {
    for (int i = 0; i < 3; ++i) {
        hash<softint> h1 = cast<hash<softint>>({"a": i});
        hash<any> h2 = cast<hash<any>>({"s": "x" + i});
        hash<string, int> h3 = {"k": i};
        hash<auto> h4 = {"z": i};
        if (i == 2) {
            show("hash<softint>", h1);
            show("hash<any>", h2);
            show("hash<string, int>", h3);
            show("hash<auto>", h4);
        }
    }
}

run();
