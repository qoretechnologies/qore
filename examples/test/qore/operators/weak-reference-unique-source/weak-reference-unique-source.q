%modern
%allow-weak-references

# a weak reference to a variable designates the variable's own container: a container that the variable shares
# with another owner (a literal of the program, another variable) is made unique first, so the writes of the owner
# are seen through the weak reference, in every execution mode (qore#5756)

# the issue: the literal is shared with the program
hash<auto> h = {"a": 1};
hash<auto> w := h;
hash<auto> r = remove h;
r.b = 2;
printf("remove: %y\n", w);

hash<auto> h2 = {"a": 1};
hash<auto> w2 := h2;
hash<string, int> n = new hash<string, int>(remove h2);
n.b = 2;
printf("new hash: %y\n", w2);

hash<auto> h1 = {"a": 1};
hash<auto> w1 := h1;
h1.b = 2;
printf("owner write: %y\n", w1);

# a computed container, owned only by the variable
hash<auto> u = map {$1: 1}, ("a",);
hash<auto> w3 := u;
u.b = 2;
printf("unshared: %y\n", w3);

# a container shared with another variable: the weak reference follows its owner, the other variable is not changed
hash<auto> c = map {$1: 1}, ("a",);
hash<auto> c2 = c;
hash<auto> cw := c2;
c2.b = 2;
printf("shared variable: %y %y %y\n", c, c2, cw);

# lists
list<auto> l = (1, 2);
list<auto> wl := l;
l += 3;
printf("list literal: %y\n", wl);

list<auto> k = map $1, (1, 2);
list<auto> k2 = k;
list<auto> wk := k2;
k2 += 3;
printf("list shared variable: %y %y %y\n", k, k2, wk);

# a variable of a function
hash<auto> sub in_function() {
    hash<auto> v = {"v": 1};
    hash<auto> vw := v;
    v.y = 2;
    return vw;
}
printf("function: %y\n", in_function());

list<auto> sub list_in_function() {
    list<auto> v = (1, 2);
    list<auto> vw := v;
    v += 3;
    v[0] = 9;
    return vw;
}
printf("list function: %y\n", list_in_function());

# a global variable
our hash<auto> og = {"og": 1};
hash<auto> sub from_global() {
    hash<auto> gw := og;
    og.x = 2;
    return gw;
}
printf("global: %y\n", from_global());

# a top-level variable that a function reads
hash<auto> top = map {$1: 1}, ("t",);
hash<auto> top_sharer = top;
hash<auto> sub from_top_level() {
    hash<auto> tw := top;
    top.x = 2;
    return tw;
}
printf("top-level variable: %y %y\n", from_top_level(), top_sharer);

# a local that a reference is taken of, and one that a background expression uses
sub noop(reference<hash<auto>> r) {
}
hash<auto> sub referenced_local() {
    hash<auto> a = map {$1: 1}, ("a",);
    hash<auto> b = a;
    noop(\b);
    hash<auto> bw := b;
    b.z = 2;
    return bw;
}
printf("referenced local: %y\n", referenced_local());

hash<auto> sub background_local() {
    hash<auto> a = map {$1: 1}, ("a",);
    hash<auto> b = a;
    background noop(\b);
    hash<auto> bw := b;
    b.z = 2;
    return bw;
}
printf("background local: %y\n", background_local());

# typed containers: the copy keeps the declared type
hash<string, int> ti = map {$1: 1}, ("a",);
hash<string, int> ti2 = ti;
hash<string, int> tw := ti2;
ti2.b = 2;
printf("hash<string, int>: %y %y %y %s\n", ti, ti2, tw, tw.fullType());

hashdecl WeakRefH {
    int a;
    int b = 0;
}
WeakRefH hd = <WeakRefH>{"a": 1};
WeakRefH hd2 = hd;
WeakRefH hw := hd2;
hd2.b = 2;
printf("hashdecl: %y %y %y\n", hd, hd2, hw);

list<int> tl = map $1, (1, 2);
list<int> tl2 = tl;
list<int> tlw := tl2;
tl2 += 3;
printf("list<int>: %y %y %y %s\n", tl, tl2, tlw, tlw.fullType());

# an optional type that holds a hash, and one that holds NOTHING
*hash<auto> op = map {$1: 1}, ("u",);
*hash<auto> op2 = op;
*hash<auto> opw := op2;
op2.v = 2;
printf("optional with a hash: %y %y %y\n", op, op2, opw);
*hash<auto> none;
*hash<auto> nonew := none;
printf("optional with NOTHING: %y\n", nonew);

# a source that is a reference to a variable
sub weak_of_reference(reference<hash<auto>> r) {
    hash<auto> w := r;
    r.added = 1;
    printf("reference source: %y\n", w);
}
hash<auto> rsrc = map {$1: 1}, ("r",);
hash<auto> rsrc2 = rsrc;
weak_of_reference(\rsrc2);
printf("reference source sharer: %y\n", rsrc);

# a weak reference of a weak reference
hash<auto> sub weak_of_weak() {
    hash<auto> a = {"a": 1};
    hash<auto> aw := a;
    hash<auto> aw2 := aw;
    a.q = 1;
    return aw2;
}
printf("weak of weak: %y\n", weak_of_weak());

# a value that is not a container is taken as it is
auto sub plain_int() {
    int i = 5;
    int j := i;
    i = 6;
    return (i, j);
}
printf("int: %y\n", plain_int());

# the same, repeated, so that the compiled tiers run their fast paths
int ok = 0;
for (int i = 0; i < 2000; ++i) {
    if (in_function() == {"v": 1, "y": 2}) {
        ++ok;
    }
}
printf("repeated: %d\n", ok);
