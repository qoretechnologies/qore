%modern
%allow-weak-references

# a weak reference to a variable read in a function: the writes of the function to the variable's container
# change the container that the weak reference designates, the container is not copied when the variable is its only
# owner; the executable of a compiled program copied it, which left the weak reference with a container that the
# variable no longer held: it read as an empty hash, and returned from the function it was freed (qore#5832)

# a top-level variable read in functions
hash<auto> g = {"g": 1};
sub constant_key() {
    g.y = 0;
    hash<auto> w := g;
    g.x = 2;
    printf("constant key: %y\n", w);
}
constant_key();

hash<auto> sub returned() {
    g.z = 0;
    hash<auto> w := g;
    g.u = 3;
    return w;
}
printf("returned: %y\n", returned());

hash<auto> h = {"h": 1};
sub dynamic_key(string k) {
    h{k} = 0;
    hash<auto> w := h;
    h{k + "2"} = 2;
    printf("dynamic key: %y\n", w);
}
dynamic_key("a");

hash<auto> c = {"n": 1};
sub compound() {
    c.m = 0;
    hash<auto> w := c;
    c.n += 5;
    c{"m"} += 7;
    printf("compound: %y\n", w);
}
compound();

list<auto> l = (1, 2);
sub list_index() {
    l[0] = 9;
    list<auto> w := l;
    l[1] = 8;
    l[2] = 7;
    printf("list index: %y\n", w);
}
list_index();

# a container shared with another variable is made unique by the first write, as it was
hash<auto> sh1 = map {$1: 1}, ("a",);
hash<auto> sh2 = sh1;
sub shared() {
    sh2.b = 2;
    hash<auto> w := sh2;
    sh2.c = 3;
    printf("shared: %y %y %y\n", sh1, sh2, w);
}
shared();

# a variable captured by a closure
hash<auto> cap = {"cap": 1};
code cl = sub () {
    cap.a = 0;
    hash<auto> w := cap;
    cap.b = 2;
    printf("closure: %y\n", w);
};
cl();

# a variable that a function reads through a reference
sub through_reference(reference<hash<auto>> r) {
    r.a = 0;
    hash<auto> w := r;
    r.b = 2;
    printf("reference: %y\n", w);
}
hash<auto> rv = {"rv": 1};
through_reference(\rv);

# the same without a weak reference: the writes and the copies as before
sub plain() {
    g.p = 1;
    hash<auto> copy = g;
    g.q = 2;
    printf("plain: %y %y\n", copy, g);
}
plain();

# repeated, so that the compiled code runs its fast paths
int ok = 0;
for (int i = 0; i < 1000; ++i) {
    if (returned().size() > 4) {
        ++ok;
    }
}
printf("repeated: %d\n", ok);
