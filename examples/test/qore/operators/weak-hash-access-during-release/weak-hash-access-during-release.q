%modern
%allow-weak-references
# the program of weak-hash-access-during-release.qtest: a destructor that runs during the release of a hash reads and
# changes the hash through a weak reference (qore#7569); every execution mode must print the output of the AST
# interpreter
#
# The destructor runs while the members of the hash are freed: by qore_hash_private::freeMembers() when the hash is
# released directly ("shallow"), and by qore_hash_private::freeNextMember() when the hash is nested deeply enough for
# the iterative release of nested containers ("deep").  A read through the weak reference gets an empty stand-in
# that cannot be changed, so no access reaches the members while they are freed.

#! nested more deeply than the recursive release of containers goes (QORE_CONTAINER_FREE_RECURSION_DEPTH is 16)
const DeepNesting = 24;

#! holds a weak reference to the hash that holds it, which its destructor reads and changes
class Probe {
    public {
        string label;
        hash<auto> wh;
    }

    constructor(string label) {
        self.label = label;
    }

    destructor() {
        # a read takes the stand-in: empty, whatever members the hash has left
        hash<auto> h = wh;
        printf("%s: read: size %d, keys %y, member a: %y\n", label, h.size(), keys h, h.a);
        printf("%s: weak read: size %d, member b: %y, exists c: %y\n", label, wh.size(), wh.b, exists wh.c);
        int n = 0;
        foreach hash<auto> i in (wh.pairIterator()) {
            printf("%s: iterated member %y\n", label, i.key);
            ++n;
        }
        printf("%s: iterated members: %d\n", label, n);
        # a change raises an exception and changes nothing
        try {
            wh.x = 1;
            printf("%s: write: no exception\n", label);
        } catch (hash<ExceptionInfo> ex) {
            printf("%s: write: %s: %s\n", label, ex.err, ex.desc);
        }
        try {
            wh += {"y": 2};
            printf("%s: merge: no exception\n", label);
        } catch (hash<ExceptionInfo> ex) {
            printf("%s: merge: %s: %s\n", label, ex.err, ex.desc);
        }
        try {
            remove wh.a;
            printf("%s: remove: no exception\n", label);
        } catch (hash<ExceptionInfo> ex) {
            printf("%s: remove: %s: %s\n", label, ex.err, ex.desc);
        }
        printf("%s: after the changes: size %d, keys %y\n", label, wh.size(), keys wh);
    }
}

#! makes a hash with members before and after a Probe that holds a weak reference to it
hash<auto> sub make_hash(string label) {
    hash<auto> h = {};
    h.a = 1;
    h.b = "two";
    h.p = new Probe(label);
    h.c = 3.0;
    h.d = {"nested": True};
    h.p.wh := h;
    return h;
}

#! releases the hash directly: its members are freed by freeMembers()
sub shallow() {
    hash<auto> h = make_hash("shallow");
    h = {};
    printf("shallow: released\n");
}

#! releases the hash nested in lists: its members are freed by freeNextMember()
sub deep() {
    auto v = make_hash("deep");
    for (int i = 0; i < DeepNesting; ++i) {
        v = (v,);
    }
    v = NOTHING;
    printf("deep: released\n");
}

#! the probe is the first member: no member of the hash is freed when the destructor runs
sub shallow_first() {
    hash<auto> h = {};
    h.p = new Probe("shallow first");
    h.a = 1;
    h.p.wh := h;
    h = {};
    printf("shallow first: released\n");
}

#! the probe is the last member of a deeply nested hash: every other member is freed when the destructor runs
sub deep_last() {
    hash<auto> h = {};
    h.a = 1;
    h.b = "two";
    h.c = 3.0;
    h.p = new Probe("deep last");
    h.p.wh := h;
    auto v = h;
    h = {};
    for (int i = 0; i < DeepNesting; ++i) {
        v = (v,);
    }
    v = NOTHING;
    printf("deep last: released\n");
}

shallow();
deep();
shallow_first();
deep_last();
