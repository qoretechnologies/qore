%modern

# a function, closure or method that reads the implicit arguments ($1, $$) of its own call outside any operator
# that sets them sees its own extra arguments, or none, never those of its caller (qore#5805)
auto sub own1() { return $1; }
auto sub ownv() { return $$; }
auto sub with_param(auto x) { return $1; }
auto sub extra(auto x, ...) { return $1; }

string sub from_function(...) {
    return sprintf("%y %y %y %y %y", own1(), own1("Z"), ownv(), with_param("Q"), extra("P", "R"));
}
printf("function: %s\n", from_function("A", "B"));
printf("top level: %y %y\n", own1(), own1("Z"));

string sub from_local_closure(...) {
    code f = auto sub() { return $1; };
    code g = auto sub() { return $$; };
    code h = auto sub(auto x) { return $1; };
    return sprintf("%y %y %y %y", f(), g(), h(5), f("own"));
}
printf("closure: %s\n", from_local_closure("C", "D"));

code global_closure = auto sub() { return $1; };
string sub from_global_closure(...) {
    return sprintf("%y %y", global_closure(), global_closure("own"));
}
printf("global closure: %s\n", from_global_closure("E"));

class K {
    auto m() { return $1; }
    static auto sm() { return $1; }
    auto mp(auto x) { return $1; }
}
string sub from_method(...) {
    K k();
    return sprintf("%y %y %y %y %y", k.m(), K::sm(), k.mp(1), k.m("own"), K::sm("own"));
}
printf("method: %s\n", from_method("G"));

# an operator that sets the implicit arguments still does: the call inside it has none of its own
string sub in_operator(...) {
    return sprintf("%y %y", (map $1 + 1, (1, 2)), (map {$1: own1()}, ("X", "Y")));
}
printf("operator: %s\n", in_operator("H"));

# a function that reads the implicit arguments only inside an operator keeps its calls cheap and correct
list<auto> sub mapper(list<int> l) { return map $1 * 2, l; }
string sub from_mapper(...) { return sprintf("%y", mapper((1, 2, 3))); }
printf("mapper: %s\n", from_mapper("I"));

# the same call repeated, so that the compiled tiers run their fast paths
int sum = 0;
for (int i = 0; i < 2000; ++i) {
    if (from_function("A", "B") == "null \"Z\" null null \"R\"") {
        ++sum;
    }
}
printf("repeated: %d\n", sum);
