%modern
%disable-warning unreferenced-variable

# a default argument expression that reads a top-level local variable (qore#5868): `K k();` at the top level of a
# %modern script declares a top-level local, not a global.  The value of the variable at the time of the call is
# read, in every execution mode.

class K {
    public {
        int v = 7;
    }

    string m2(string a, string b) {
        return sprintf("a=%y b=%y", a, b);
    }

    string m0() {
        return "m0";
    }

    int get() {
        return v;
    }

    static string s1(string a) {
        return "static " + a;
    }
}

K k();
int tl = 5;
string str = "text";
list<int> lst = (10, 20, 30);
hash<string, int> hsh = {"a": 1, "b": 2};
our string gs = "global";

string sub kk(string s) {
    return s;
}

string sub dm(string s = k.m2("A", "B")) {
    return s;
}

string sub dm0(string s = k.m0()) {
    return s;
}

string sub dc(string s = k.className()) {
    return s;
}

int sub dt(int i = tl + 1) {
    return i;
}

string sub ds(string s = str + "!") {
    return s;
}

int sub dl(int i = lst[1]) {
    return i;
}

int sub dh(int i = hsh.b) {
    return i;
}

string sub dg(string x = kk(gs)) {
    return x;
}

string sub dn(string x = kk(str)) {
    return x;
}

int sub dget(int i = k.get()) {
    return i;
}

string sub dstatic(string s = K::s1(str)) {
    return s;
}

# an overloaded function with a default that reads a top-level local
string sub over(int i, string s = str) {
    return sprintf("int %d %s", i, s);
}

string sub over(string a, string s = str + "?") {
    return sprintf("string %s %s", a, s);
}

# a default that uses two top-level locals and a global
string sub two(string s = str + k.m0() + gs) {
    return s;
}

# defaults that are larger expressions
list<int> sub dmap(list<int> l = (map $1 + tl, lst)) {
    return l;
}

string sub dtern(string s = tl > 3 ? str : "no") {
    return s;
}

int sub dsel(int n = (select lst, $1 > tl - 4).size()) {
    return n;
}

string sub dfmt(string s = sprintf("%d-%s", tl, str)) {
    return s;
}

auto sub dlist(auto l = (tl, str, lst)) {
    return l;
}

auto sub dhash(auto h = {"t": tl, "s": str}) {
    return h;
}

sub show() {
    printf("%s | %s | %s | %d | %s | %d | %d | %s | %s | %d | %s\n", dm(), dm0(), dc(), dt(), ds(), dl(), dh(), dg(),
        dn(), dget(), dstatic());
    printf("%s | %s | %s\n", over(1), over("x"), two());
    printf("%y %y %y %y %y %y\n", dmap(), dtern(), dsel(), dfmt(), dlist(), dhash());
}

show();
# the value of the variable at the time of the call is read, not the value when the default was loaded
tl = 10;
str = "changed";
lst[1] = 99;
hsh.b = 42;
k.v = 8;
show();
# an explicit argument is used as it is
printf("%d %s\n", dt(3), dm("X"));
