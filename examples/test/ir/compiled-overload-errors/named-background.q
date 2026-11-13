%modern
# named calls whose arguments are bound to their parameters at parse time, in the copy of a background expression:
# the copied arguments, evaluated or not when the thread starts, keep the positions of their parameters (in the AST
# interpreter, the copy of the call takes its arguments as temporary values); see compiled-overload-errors.qtest

sub fsw(Queue q, int count, string label) { q.push(sprintf("fsw count=%y label=%y", count, label)); }
string sub str(int count, string label) { return sprintf("str count=%y label=%y", count, label); }
auto sub value_of(auto v) { return v; }

final class F {
    string sw(int count, string label) { return sprintf("F::sw count=%y label=%y", count, label); }
    static string ss(int count, string label) { return sprintf("F::ss count=%y label=%y", count, label); }
    # a call of a method of the object in a background expression
    string selfBg(Queue q) { background q.push(self.sw(label: "self", count: 1)); return q.get(); }
    string unqualifiedBg(Queue q) { background q.push(sw(label: "unqualified", count: 2)); return q.get(); }
}

class N {
    public { string s; }
    constructor(int count, string label) { s = sprintf("N count=%y label=%y", count, label); }
}

Queue q();
F f();
int c = 3;
string l = "local";

# literal arguments, evaluated when the expression is copied
background fsw(q, label: "lit", count: 4);
printf("%s\n", q.get());
background q.push(str(label: "lit", count: 5));
printf("%s\n", q.get());
background q.push(f.sw(label: "lit", count: 6));
printf("%s\n", q.get());
background q.push(F::ss(label: "lit", count: 7));
printf("%s\n", q.get());
background q.push((new N(label: "lit", count: 8)).s);
printf("%s\n", q.get());

# local variables, whose values are taken when the expression is copied
background q.push(str(label: l, count: c));
printf("%s\n", q.get());

# expressions evaluated in the new thread, in source order
background q.push(str(label: value_of("expr"), count: value_of(9)));
printf("%s\n", q.get());

# calls of methods of the object
printf("%s\n", f.selfBg(q));
printf("%s\n", f.unqualifiedBg(q));

# a call evaluated for each element (the copy of the call is reused)
background q.push(map str(label: "map" + $1, count: $1), (1, 2));
printf("%y\n", q.get());
background q.push(map f.sw(label: "mmap" + $1, count: $1), (1, 2));
printf("%y\n", q.get());

# literal arguments force the reusable copied-list path; keep their named parameter positions on every call
thread_join(background q.push(map F::ss(label: "smap", count: 10), (1, 2)));
printf("%y\n", q.get());
thread_join(background q.push(map F::ss(11, "positional"), (1, 2)));
printf("%y\n", q.get());
