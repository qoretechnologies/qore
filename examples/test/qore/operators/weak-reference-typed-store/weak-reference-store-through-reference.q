%modern
%allow-weak-references
%allow-opaque-references
%new-style
%strict-args
%require-types
%enable-all-warnings

# a weak or opaque assignment to a variable that holds a reference assigns the referenced lvalue with its type: the
# value is converted to the type of the referenced lvalue (a softlist takes a list of the value), and the weak
# reference designates the converted value, which lives until the end of the statement (qore#7164); a container that
# the conversion would replace with a new one raises RUNTIME-TYPE-ERROR (qore#7237)

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
    show("reference local: value", (r := 1));
    show("reference local: after", l);
    show("reference local: read through", r);
}

sub reference_local_opaque() {
    softlist<auto> l;
    reference r = \l;
    show("reference local opaque: value", (r @= 1));
    show("reference local opaque: after", l);
}

sub auto_local() {
    softlist<auto> l;
    auto a = \l;
    show("auto local: value", (a := 1));
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
    show("reference parameter: value", (r := 1));
}

sub auto_param(auto a) {
    show("auto parameter: value", (a := 1));
}

# a local that is given a reference as a value (an element of a list), in a function that creates no reference
sub reference_element(list<auto> refs) {
    auto a = refs[0];
    show("reference element: value", (a := 1));
}

# the variable of a foreach loop that is given a reference by an element, in a function that creates no reference
sub reference_foreach(list<auto> refs) {
    foreach auto v in (refs) {
        show("foreach reference element: value", (v := 1));
    }
}

sub closure_reference() {
    softlist<auto> l;
    reference r = \l;
    code c = sub () {
        show("closure reference: value", (r := 1));
    };
    c();
    show("closure reference: after", l);
}

sub string_softlist() {
    *softlist<softint> l;
    reference r = \l;
    show("softlist<softint>: value", (r := "3"));
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
    show("global: value", (r := 1));
    show("global: after", gsl);
}

sub member_target() {
    Holder o();
    reference r = \o.sl;
    show("member: value", (r := 1));
    show("member: after", o.sl);
}

sub statement_lifetime() {
    softlist<auto> l;
    reference r = \l;
    # the converted value lives until the end of the statement, so the weak reference reads it in the same statement
    show("statement lifetime: same statement", (r := 1, l)[1]);
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
