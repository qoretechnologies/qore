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
