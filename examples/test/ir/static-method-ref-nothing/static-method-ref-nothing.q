%modern
# a reference to a static method of a class through a value of the class's type that is NOTHING raises the error of a
# pseudo-method of NOTHING, with the name of the method (qore#6684)
class C {
    public {
        static C n;
        static *C on;
    }
    static int sm() { return 1; }
}
foreach string what in (("static member", "optional static member")) {
    try {
        code c = what == "static member" ? \C::n.sm() : \C::on.sm();
        printf("%s: created %y\n", what, c);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", what, ex.err, ex.desc);
    }
}
