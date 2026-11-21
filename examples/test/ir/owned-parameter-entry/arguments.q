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
