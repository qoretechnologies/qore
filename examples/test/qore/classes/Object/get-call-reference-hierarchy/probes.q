# probes of get-call-reference-hierarchy.qtest (qore#5866): a call reference to a non-static method of a class that is
# not in the hierarchy of the object raises CALL-REFERENCE-ERROR and gives no reference, with
# <object>::getCallReference(); the object is destroyed when it goes out of scope; a method of the object's own
# hierarchy gives a reference that calls it.  probes() returns one line per pass, so that the code runs on every tier
# with tiered execution

%modern

class A {
    int m() { return 1; }
}

class Base {
    int bm() { return 2; }
}

class B inherits Base {
    public {
        static int destroyed = 0;
    }
    destructor() { ++destroyed; }
    code get(string path) { return self.getCallReference(path); }
}

string sub err(hash<ExceptionInfo> ex) {
    return sprintf("%s: %s", ex.err, ex.desc);
}

string sub t(string path) {
    int destroyed = B::destroyed;
    string rv;
    {
        B b();
        try {
            code c = b.get(path);
            rv = sprintf("%y", c());
        } catch (hash<ExceptionInfo> ex) {
            rv = err(ex);
        }
    }
    return sprintf("%s (destroyed %d)", rv, B::destroyed - destroyed);
}

string sub probes() {
    string rv;
    for (int i = 0; i < 8; ++i) {
        rv += sprintf("%d: %s; %s\n", i, t("A::m"), t("Base::bm"));
    }
    return rv;
}
