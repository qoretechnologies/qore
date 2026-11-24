%modern

hashdecl HD {
    int a = 1;
}

sub check(string key) {
    *hash<HD> h;
    try {
        h.a = 2;
    } catch (hash<ExceptionInfo> ex) {
        printf("constant: %s\n", ex.err);
    }
    printf("after constant: %y\n", h);
    try {
        h{key} = 3;
    } catch (hash<ExceptionInfo> ex) {
        printf("dynamic %s: %s\n", key, ex.err);
    }
    printf("after dynamic: %y\n", h);
    h = new hash<HD>();
    try {
        h{key} = 4;
    } catch (hash<ExceptionInfo> ex) {
        printf("initialized %s: %s\n", key, ex.err);
    }
    printf("initialized: %y\n", h);
}

sub generic<T>(*T witness) {
    *T h;
    try {
        h.a = 2;
    } catch (hash<ExceptionInfo> ex) {
        printf("generic: %s\n", ex.err);
    }
    printf("generic value: %y\n", h);
}

class Holder {
    private {
        *hash<HD> slot;
    }
    checkMember() {
        try {
            slot.a = 2;
        } catch (hash<ExceptionInfo> ex) {
            printf("member: %s\n", ex.err);
        }
        printf("member value: %y\n", slot);
    }
}
sub ordinary() {
    *hash<string, int> h;
    h.a = 2;
    printf("ordinary: %y\n", h);
    *hash<string, hash<HD>> table;
    table.entry = new hash<HD>();
    printf("ordinary outer hash: %y\n", table.entry.a);
}
check("a");
check("z");
generic<hash<HD>>();
new Holder().checkMember();
ordinary();
