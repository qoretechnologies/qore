%modern

# the example of qore#6282: an int stored in a local of the type union<float, int> at the top level (in a try block) is
# kept as in a function: the union has an int member (qore#6407)

try {
    auto a = 1;
    union<float, int> u = a;
    printf("top: %y %s\n", u, u.fullType());
} catch (hash<ExceptionInfo> ex) {
    printf("top: %s\n", ex.err);
}

sub f() {
    auto a = 1;
    union<float, int> u = a;
    printf("sub: %y %s\n", u, u.fullType());
}

f();
