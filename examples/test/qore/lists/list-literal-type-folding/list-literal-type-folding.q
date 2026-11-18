%modern

# a list literal whose element's parse type does not allow its value (qore#5891): the parse type of "b + h" (a bool and
# a hash) is wrong until it is fixed in the operator, and the list literal's type folding then raised an exception
# that an assertion said could never be raised; a debug build aborted.  The program must end with either the value of
# the expression or the type error, never with an abort.

auto sub f(bool b, hash<auto> h) {
    return (b + h,);
}

try {
    printf("value: %y\n", f(True, {"a": 1}));
} catch (hash<ExceptionInfo> ex) {
    printf("caught %s\n", ex.err);
}

# an element that holds an object, before the element that raises
class Tracked {
    destructor() {
        # nothing: the object is released without an error
    }
}

auto sub g(bool b, hash<auto> h) {
    return (new Tracked(), b + h);
}

try {
    printf("value: %y\n", g(True, {"a": 1}).size());
} catch (hash<ExceptionInfo> ex) {
    printf("caught %s\n", ex.err);
}
printf("done\n");
