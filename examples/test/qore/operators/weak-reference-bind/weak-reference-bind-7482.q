%modern
%allow-weak-references

# a program of weak-reference-bind.qtest: a reference to a variable that holds a weak reference to a list or a hash is
# bound with the plain and the or-nothing types too, also where the reference is folded into a typed list
# (qore#7482); every mode must print the output of the AST interpreter; a change through the reference is an element
# assignment, as += on a variable that holds a weak reference is qore#7297

sub show(string label, code c) {
    try {
        printf("%s: %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

list v = (1,);
auto b;
b := v;
hash vh = {"a": 1};
auto hb;
hb := vh;

# the references folded into typed lists (the form of the issue)
show("list<reference<list>>", auto sub () {
    list<reference<list>> l = (\b,);
    l[0][1] = 2;
    return v;
});
show("list<reference<*list>>", auto sub () {
    list<reference<*list>> l = (\b,);
    l[0][2] = 3;
    return v;
});
show("list<reference<hash>>", auto sub () {
    list<reference<hash>> l = (\hb,);
    l[0].b = 2;
    return vh;
});
show("list<reference<*hash>>", auto sub () {
    list<reference<*hash>> l = (\hb,);
    l[0].c = 3;
    return vh;
});

# an assignment of the variable to a variable of the plain type gives the target, as before
show("assign list", auto sub () { list pl = b; return pl; });
show("assign hash", auto sub () { hash ph = hb; return ph; });

# the variables still hold the weak references, not copies: a change of the targets is seen through them
show("weak list", auto sub () { v[5] = 9; return b; });
show("weak hash", auto sub () { vh.d = 4; return hb; });
