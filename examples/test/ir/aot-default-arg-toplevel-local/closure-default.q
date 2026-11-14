%modern

# a closure that is the default argument of a function and reads a top-level local (qore#5998): the closure reads the
# variable of the running Program, with its value at the time of the call

int tl = 5;
string str = "s";
list<int> lst = (1, 2, 3);
hash<auto> hsh = {"a": 1, "b": 2};

# the reproducer of the issue
auto sub dcl(code c = auto sub () { return tl; }) {
    return c();
}
printf("%y\n", dcl());
tl = 10;
printf("%y\n", dcl());

# a closure that reads other kinds of top-level locals, and its own parameter
auto sub dstr(code c = auto sub (string s) { return str + s; }) {
    return c("!");
}
printf("%y\n", dstr());
str = "t";
printf("%y\n", dstr());

auto sub dcont(code c = auto sub () { return (lst[1], hsh.b, lst.size()); }) {
    return c();
}
printf("%y\n", dcont());
lst += 4;
printf("%y\n", dcont());

# the closure after another parameter with a default, and called with an argument instead
auto sub dtwo(int a = 1, code c = auto sub () { return tl + 1; }) {
    return (a, c());
}
printf("%y\n", dtwo());
printf("%y\n", dtwo(2));
printf("%y\n", dtwo(3, auto sub () { return tl + 100; }));

# a closure in the default of a closure's parameter, and a closure inside the closure
auto sub dnest(code c = auto sub () { code inner = auto sub () { return tl * 2; }; return inner(); }) {
    return c();
}
printf("%y\n", dnest());
tl = 7;
printf("%y\n", dnest());

# a closure that assigns the top-level local
auto sub dset(code c = auto sub (int i) { tl = i; return tl; }) {
    return c(42);
}
printf("%y %y\n", dset(), tl);

# a top-level local that a closure of a local variable in a function also reads, in the same program
auto sub dmix(int a = tl, code c = auto sub () { return tl + 1; }) {
    return (a, c());
}
printf("%y\n", dmix());
tl = 1;
printf("%y\n", dmix());
