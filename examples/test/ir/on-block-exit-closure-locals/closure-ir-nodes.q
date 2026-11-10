%modern
# closures that create values from parse nodes (typed hashes and lists, hashdecl hashes, objects declared with their
# constructor arguments, references and closures); see on-block-exit-closure-locals.qtest, which runs them in AOT
# executables from their serialized IR and compiled with the JIT, where these instructions had no parse node

hashdecl ClosureIrNodesHash {
    int a = 1;
    string b = "x";
}

class ClosureIrNodesObject {
    public {
        int n;
    }

    constructor(int n) {
        self.n = n;
    }
}

#! adds 30 to the variable it is passed a reference to
sub add30(reference<int> r) {
    r += 30;
}

sub main() {
    code typed_hash = string sub (int n) {
        hash<string, int> h = new hash<string, int>();
        h.x = n;
        return sprintf("%y", h);
    };
    code typed_list = string sub (int n) {
        list<int> l = new list<int>();
        l += n;
        return sprintf("%y", l);
    };
    code hashdecl_hash = string sub (int n) {
        hash<ClosureIrNodesHash> h = new hash<ClosureIrNodesHash>();
        h.a += n;
        return sprintf("%d %s", h.a, h.b);
    };
    code object = string sub (int n) {
        ClosureIrNodesObject o(n);
        return sprintf("%d", o.n);
    };
    code reference = string sub (int n) {
        int e = n;
        add30(\e);
        return sprintf("%d", e);
    };
    code closure = string sub (int n) {
        int e = n;
        code inc = sub () { e += 30; };
        inc();
        return sprintf("%d", e);
    };
    # every closure runs more than once, so that it is compiled with tiered execution
    for (int i = 0; i < 3; ++i) {
        printf("typed hash: %s\n", typed_hash(7));
        printf("typed list: %s\n", typed_list(7));
        printf("hashdecl hash: %s\n", hashdecl_hash(7));
        printf("object: %s\n", object(7));
        printf("reference: %s\n", reference(7));
        printf("closure: %s\n", closure(7));
    }
}

main();
