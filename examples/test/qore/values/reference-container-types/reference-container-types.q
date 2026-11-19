#!/usr/bin/env qore
# the types of containers holding references: a container built from reference expressions has the type of the
# same declared container (qore#6575)

%modern
%enable-all-warnings
%requires reflection

class T {
    public {
        string n;
    }

    constructor(string n) {
        self.n = n;
    }
}

our *T g_o = new T("x");
our *T g_p;
our int g_i = 1;

# prints whether the types of two values are the same type and their names
sub same(string label, auto a, auto b) {
    Type ta = Type::getType(a);
    Type tb = Type::getType(b);
    printf("%s: %s %s: equal %y\n", label, ta.getName(), tb.getName(), ta.isEqual(tb) && tb.isEqual(ta));
}

# reads the variable that a reference refers to
string sub deref(reference<*T> r) {
    return r.n;
}

# reads the variable that the reference in the hash refers to
string sub read_hash(hash<string, reference<*T>> h) {
    return deref(h.a);
}

# reads the variable that the reference in the list refers to
string sub read_list(list<reference<*T>> l) {
    return deref(l[0]);
}

sub take_hash(hash<string, reference<*T>> h) {
    printf("take hash: %s\n", h.fullType());
}

sub take_list(list<reference<*T>> l) {
    printf("take list: %s\n", l.fullType());
}

# a literal hash of lists of references was rejected for this type
sub take_nested(hash<string, list<reference<int>>> h) {
    printf("take nested: %s\n", h.fullType());
}

# the type of a literal container holding references and that of the declared container
{
    hash<string, reference<*T>> decl = {"a": \g_o};
    same("hash literal", {"a": \g_o}, decl);
    list<reference<*T>> ldecl = (\g_o,);
    same("list literal", (\g_o, \g_o), ldecl);
    hash<string, reference<int>> idecl = {"a": \g_i};
    same("int hash literal", {"a": \g_i}, idecl);
    hash<string, hash<string, reference<*T>>> ndecl = {"a": {"b": \g_o}};
    same("nested hash literal", {"a": {"b": \g_o}}, ndecl);
    hash<string, list<reference<*T>>> nldecl = {"a": (\g_o,)};
    same("nested list literal", {"a": (\g_o,)}, nldecl);
    same("hash values", {"a": \g_o}.values(), decl.values());
    same("list of literal hash values", ({"a": \g_o}.values(),), (decl.values(),));
}

# the references in a literal container still refer to their variables
{
    g_p = new T("p");
    printf("read hash: %s\n", read_hash({"a": \g_p}));
    printf("read list: %s\n", read_list((\g_p,)));
    g_p = new T("q");
    auto h = {"a": \g_p};
    printf("read variable: %s\n", deref(h.a));
    take_hash({"a": \g_o});
    take_list((\g_o,));
    take_nested({"a": (\g_i,)});
}

# a value that is not a reference is still not accepted
{
    auto h = {"a": \g_o};
    try {
        h.b = new T("y");
        printf("object accepted\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("object: %s\n", ex.err);
    }
    try {
        h.b = NOTHING;
        printf("NOTHING accepted\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("NOTHING: %s\n", ex.err);
    }
    list<auto> l = (\g_o,);
    try {
        push l, 1;
        printf("int accepted\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("int: %s\n", ex.err);
    }
    push l, \g_o;
    printf("push reference: %s %d\n", l.fullType(), l.size());
}
