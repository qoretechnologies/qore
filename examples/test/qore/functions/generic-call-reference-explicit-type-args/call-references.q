%modern
list<U> sub mk0<U>() { return new list<U>(); }
hash<string, U> sub mkh<T, U>() { return new hash<string, U>(); }
list<T> sub echo<T>(T v) { return new list<T>(v); }
namespace NS {
    public list<U> sub mk<U>() { return new list<U>(); }
}

code c = \mk0<string>();
printf("%s\n", c().fullType());
code d = \mkh<int, float>();
printf("%s\n", d().fullType());
code e = \NS::mk<number>();
printf("%s\n", e().fullType());
code f = \mk0<int>();
printf("%s %s\n", f().fullType(), c().fullType());
# a reference without explicit arguments infers them as before
code g = \echo();
printf("%s %s\n", g(1).fullType(), g("s").fullType());
# call references made inside generic code
list<auto> sub viaRef<U>() { code r = \mk0<U>(); return r(); }
printf("%s %s\n", viaRef<int>().fullType(), viaRef<string>().fullType());
# a reference made in generic code is bound to the instantiation that made it, also when it is called later
code sub getRef<U>() { return \mk0<U>(); }
printf("%s %s\n", getRef<int>()().fullType(), getRef<string>()().fullType());
class GG<T> {
    public code get() { return \mk0<T>(); }
}
GG<int> gi();
GG<float> gf();
printf("%s %s\n", gi.get()().fullType(), gf.get()().fullType());
code sub nested<U>() { return \mk0<list<U>>(); }
printf("%s\n", nested<int>()().fullType());
# call_function_args / map with the reference
printf("%s\n", (map $1(), (c, f)).size());
