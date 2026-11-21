%modern

class Marker {
    private { string label; }
    constructor(string label) { self.label = label; }
    destructor() { printf("release %s\n", label); }
}

int sub consume(list<auto> values, bool fail) {
    int count = values.size();
    if (fail) {
        throw "CONSUME-ERROR";
    }
    values = ();
    printf("cleared\n");
    return count;
}

int sub firstCaller(int i) {
    try {
        return consume((i, new Marker("first")), False);
    } catch (hash<ExceptionInfo> ex) {
        rethrow;
    }
}

int sub secondCaller(int i, bool fail) {
    try {
        return consume((i, new Marker("second")), fail);
    } catch (hash<ExceptionInfo> ex) {
        printf("caught %s\n", ex.err);
        return -1;
    }
}

printf("%d %d %d\n", firstCaller(1), secondCaller(2, False), secondCaller(3, True));
