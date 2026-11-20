%modern
%no-debugging
%disable-warning duplicate-local-vars

class Marker {
    private { string label; }
    constructor(string name) { self.label = name; }
    destructor() { throw self.label, "during cleanup"; }
}

our code alwaysRaises = sub (bool nested) {
    Marker item("OUTER");
    if (nested) {
        if (nested) {
            Marker item("INNER");
            throw "BODY", "nested";
        }
    }
    throw "BODY", "outer";
};

foreach bool nested in (True, False) {
    try {
        alwaysRaises(
            nested);
    } catch (hash<ExceptionInfo> ex) {
        while (True) {
            printf("%s %d-%d\n", ex.err, ex.callstack[0].line, ex.callstack[0].endline);
            if (!ex.next) { break; }
            ex = ex.next;
        }
    }
}
