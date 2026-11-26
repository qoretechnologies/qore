%modern
%allow-weak-references
%allow-opaque-references
%new-style
%strict-args
%require-types
%enable-all-warnings

# a weak or opaque assignment to a variable that holds a reference assigns the referenced lvalue with its type: the
# value is checked and converted with the type of the referenced lvalue, not with that of the variable (qore#7164); a
# container that the conversion would replace with a new one raises RUNTIME-TYPE-ERROR (qore#7237), as does a value that
# is not a list, which a softlist lvalue would wrap in a new list that the reference would designate (qore#7619)

softlist<auto> gsl;

class Holder {
    public {
        softlist<auto> sl;
    }
}

sub show(string name, auto v) {
    printf("%s: %y\n", name, v);
}

sub reference_local() {
    softlist<auto> l;
    reference r = \l;
    try {
        show("reference local: value", (r := 1));
    } catch (hash<ExceptionInfo> ex) {
        printf("reference local: value: %s\n", ex.err);
    }
    show("reference local: after", l);
    show("reference local: read through", r);
}

sub reference_local_opaque() {
    softlist<auto> l;
    reference r = \l;
    try {
        show("reference local opaque: value", (r @= 1));
    } catch (hash<ExceptionInfo> ex) {
        printf("reference local opaque: value: %s\n", ex.err);
    }
    show("reference local opaque: after", l);
}

sub auto_local() {
    softlist<auto> l;
    auto a = \l;
    try {
        show("auto local: value", (a := 1));
    } catch (hash<ExceptionInfo> ex) {
        printf("auto local: value: %s\n", ex.err);
    }
    show("auto local: after", l);
}

sub auto_local_without_reference(bool give) {
    softlist<auto> l;
    auto a = 1;
    if (give) {
        a = \l;
    }
    list<auto> held = (1, 2);
    show("auto local without reference: value", (a := held));
    show("auto local without reference: after", a);
    show("auto local without reference: target", l);
}

sub reference_param(reference r) {
    try {
        show("reference parameter: value", (r := 1));
    } catch (hash<ExceptionInfo> ex) {
        printf("reference parameter: value: %s\n", ex.err);
    }
}

sub auto_param(auto a) {
    try {
        show("auto parameter: value", (a := 1));
    } catch (hash<ExceptionInfo> ex) {
        printf("auto parameter: value: %s\n", ex.err);
    }
}

# a local that is given a reference as a value (an element of a list), in a function that creates no reference
sub reference_element(list<auto> refs) {
    auto a = refs[0];
    try {
        show("reference element: value", (a := 1));
    } catch (hash<ExceptionInfo> ex) {
        printf("reference element: value: %s\n", ex.err);
    }
}

# the variable of a foreach loop that is given a reference by an element, in a function that creates no reference
sub reference_foreach(list<auto> refs) {
    foreach auto v in (refs) {
        try {
            show("foreach reference element: value", (v := 1));
        } catch (hash<ExceptionInfo> ex) {
            printf("foreach reference element: value: %s\n", ex.err);
        }
    }
}

sub closure_reference() {
    softlist<auto> l;
    reference r = \l;
    code c = sub () {
        try {
            show("closure reference: value", (r := 1));
        } catch (hash<ExceptionInfo> ex) {
            printf("closure reference: value: %s\n", ex.err);
        }
    };
    c();
    show("closure reference: after", l);
}

sub string_softlist() {
    *softlist<softint> l;
    reference r = \l;
    try {
        show("softlist<softint>: value", (r := "3"));
    } catch (hash<ExceptionInfo> ex) {
        printf("softlist<softint>: value: %s\n", ex.err);
    }
    show("softlist<softint>: after", l);
}

# a weak assignment that would convert the container assigned to the type of the referenced lvalue raises, as the
# weak reference would designate a copy that nothing else holds (#7237); the lvalue and the source are unchanged
sub converted_hash() {
    hash<string, softint> h;
    reference r = \h;
    hash<auto> src = {"a": "2"};
    try {
        r := src;
        show("hash<string, softint>: no exception", h);
    } catch (hash<ExceptionInfo> ex) {
        printf("hash<string, softint>: %s\n", ex.err);
    }
    show("hash<string, softint>: after", h);
    show("hash<string, softint>: source", src);
}

sub held_container() {
    softlist<auto> l;
    reference r = \l;
    list<auto> src = (1, 2);
    show("held container: value", (r := src));
    show("held container: after", l);
    src[0] = 10;
    show("held container: owner's write", l);
    src = ();
    show("held container: owner released", l);
}

sub type_error() {
    list<int> l;
    reference r = \l;
    try {
        r := "x";
        show("type error: no exception", l);
    } catch (hash<ExceptionInfo> ex) {
        printf("type error: %s\n", ex.err);
    }
    show("type error: after", l);
}

sub global_target() {
    reference r = \gsl;
    try {
        show("global: value", (r := 1));
    } catch (hash<ExceptionInfo> ex) {
        printf("global: value: %s\n", ex.err);
    }
    show("global: after", gsl);
}

sub member_target() {
    Holder o();
    reference r = \o.sl;
    try {
        show("member: value", (r := 1));
    } catch (hash<ExceptionInfo> ex) {
        printf("member: value: %s\n", ex.err);
    }
    show("member: after", o.sl);
}

sub statement_lifetime() {
    softlist<auto> l;
    reference r = \l;
    # a list made by the statement lives until its end, so the weak reference reads it in the same statement
    show("statement lifetime: same statement", (r := list(1), l)[1]);
    show("statement lifetime: next statement", l);
}

reference_local();
reference_local_opaque();
auto_local();
auto_local_without_reference(False);
auto_local_without_reference(True);
{
    softlist<auto> l;
    reference_param(\l);
    show("reference parameter: after", l);
}
{
    softlist<auto> l;
    auto_param(\l);
    show("auto parameter: after", l);
}
{
    softlist<auto> l;
    reference_element((\l,));
    show("reference element: after", l);
}
{
    softlist<auto> l;
    reference_foreach((\l,));
    show("foreach reference element: after", l);
}
closure_reference();
string_softlist();
converted_hash();
held_container();
type_error();
global_target();
member_target();
statement_lifetime();
