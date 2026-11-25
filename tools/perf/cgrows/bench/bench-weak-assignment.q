%modern
%allow-weak-references
# Weak assignment from a member, hash key, list element, or local variable (qore#7301).
class Holder {
    public { list<auto> items = (1, 2); }
}
int sub f(int a) { return a + 1; }
int sub run_member(int n) {
    Holder c();
    list<auto> w;
    int t = 0;
    for (int i = 0; i < n; ++i) {
        w := c.items;
        t += w.size();
    }
    return t;
}
int sub run_hash(int n) {
    hash<auto> h = {"k": (1, 2)};
    list<auto> w;
    int t = 0;
    for (int i = 0; i < n; ++i) {
        w := h.k;
        t += w.size();
    }
    return t;
}
int sub run_list(int n) {
    list<auto> l = ((1, 2),);
    list<auto> w;
    int t = 0;
    for (int i = 0; i < n; ++i) {
        w := l[0];
        t += w.size();
    }
    return t;
}
int sub run_variable(int n) {
    list<auto> a = (1, 2);
    list<auto> w;
    int t = 0;
    for (int i = 0; i < n; ++i) {
        w := a;
        t += w.size();
    }
    return t;
}
int n = ENV.N.toInt();
switch (ENV.L) {
    case "member": run_member(n); break;
    case "hash": run_hash(n); break;
    case "list": run_list(n); break;
    case "variable": run_variable(n); break;
}
