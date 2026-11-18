%modern
namespace NS {
    public list<U> sub mk<U>() { return new list<U>(); }
    public hash<string, U> sub mkh<T, U>() { return new hash<string, U>(); }
    namespace Inner {
        public list<U> sub deep<U>(int n) { return new list<U>(); }
    }
}
list<U> sub mk0<U>() { return new list<U>(); }
list<U> sub viaNs<U>() { return NS::mk<U>(); }
printf("%s\n", NS::mk<number>().fullType());
printf("%s\n", NS::mk<string>().fullType());
printf("%s\n", NS::mkh<int, float>().fullType());
printf("%s\n", NS::Inner::deep<string>(1).fullType());
printf("%s\n", mk0<int>().fullType());
printf("%s %s\n", viaNs<int>().fullType(), viaNs<string>().fullType());
