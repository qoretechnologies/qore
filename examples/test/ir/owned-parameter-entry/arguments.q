%modern

our int made;
our int destroyed;

class Marker {
    constructor() { ++made; }
    destructor() { ++destroyed; }
}

sub observe(list<auto> values) {
    if (values.size() != 2) {
        throw "SIZE", "expected two elements";
    }
    throw "OBSERVED", "finished reading";
}

sub replaceArgument(list<auto> values) {
    values = (1, new Marker());
    observe(values);
}

sub run() {
    list<auto> values = (0, new Marker());
    try {
        replaceArgument(values);
    } catch (hash<ExceptionInfo> ex) {
        if (ex.err != "OBSERVED") {
            rethrow;
        }
    }
}

for (int i = 0; i < 3; ++i) {
    run();
}
printf("made %d destroyed %d\n", made, destroyed);

# Each callee holds its caller's list and its own list across the call. The leaf
# raises an ordinary exception so that every catch and every frame must unwind.
sub unwindFrames(list<auto> parent, int remaining) {
    list<auto> child = (remaining, new Marker());
    try {
        if (!remaining) {
            throw "LEAF", "finished";
        }
        unwindFrames(child, remaining - 1);
    } catch (hash<ExceptionInfo> ex) {
        if (parent.size() != 2) {
            throw "SIZE", "lost caller's argument";
        }
        rethrow;
    }
}

foreach int depth in ((0, 1, 4, 12)) {
    made = destroyed = 0;
    try {
        unwindFrames((depth, new Marker()), depth);
    } catch (hash<ExceptionInfo> ex) {
        if (ex.err != "LEAF") {
            rethrow;
        }
    }
    printf("depth %d made %d destroyed %d\n", depth, made, destroyed);
}
