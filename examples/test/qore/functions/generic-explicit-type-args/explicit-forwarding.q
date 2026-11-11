%modern
hash<string, T> sub inner<T>() { return new hash<string, T>(); }
hash<string, T> sub outerf<T>(T v) { return inner<T>(); }
list<T> sub linner<T>() { return new list<T>(); }
list<T> sub louter<T>(T v) { return linner<T>(); }
class K<T> {
    hash<string, T> m() { return new hash<string, T>(); }
    hash<string, T> n() { return inner<T>(); }
    static list<T> s() { return linner<T>(); }
}
printf("%s\n", outerf(1).fullType());
printf("%s\n", louter("s").fullType());
K<float> k();
printf("%s\n", k.m().fullType());
printf("%s\n", k.n().fullType());
printf("%s\n", K<int>::s().fullType());
printf("%s\n", inner<int>().fullType());
