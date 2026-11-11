%modern
%allow-weak-references
# containers changed in place through a variable that a weak reference is taken of
auto l1 = map $1, (1, 2);
auto l1w := l1;
l1 += (3, 4);
printf("list += list: %y\n", l1w);

auto l2 = map $1, (1, 2, 3);
auto l2w := l2;
l2 -= 2;
printf("list -= : %y\n", l2w);

auto l3 = map $1, (1, 2);
auto l3w := l3;
push l3, 3;
unshift l3, 0;
printf("push, unshift: %y\n", l3w);

auto l4 = map $1, (1, 2, 3);
auto l4w := l4;
shift l4;
pop l4;
printf("shift, pop: %y\n", l4w);

auto l5 = map $1, (1, 2, 3, 4);
auto l5w := l5;
splice l5, 1, 2;
printf("splice: %y\n", l5w);

auto h1 = map {$1: 1}, ("a", "b");
auto h1w := h1;
h1 += {"c": 3};
h1 -= "a";
printf("hash += -= : %y\n", h1w);

auto h2 = map {$1: 1}, ("a",);
auto h2w := h2;
h2.a += 5;
h2.b = 1;
h2{"c"} = 2;
printf("hash members: %y\n", h2w);

# a function local, a global and a closure-captured variable
auto sub in_function() {
    auto l = map $1, (1, 2);
    auto lw := l;
    l += 3;
    push l, 4;
    return lw;
}
printf("function: %y\n", in_function());

our auto og = map $1, (1, 2);
auto sub from_global() {
    auto gw := og;
    og += 3;
    return gw;
}
printf("global: %y\n", from_global());

auto sub captured() {
    auto l = map $1, (1, 2);
    code get = auto sub () { return l; };
    auto lw := l;
    l += 3;
    return (lw, get().size());
}
printf("closure: %y\n", captured());

# a source that is only read through a weak reference: scalars and objects are not changed
auto sub scalars() {
    auto i = 1;
    auto iw := i;
    i += 5;
    return (i, iw);
}
printf("scalar: %y\n", scalars());

# the weak holder is itself changed: the target is the container
auto sub holder() {
    auto src = map $1, (1, 2);
    auto w := src;
    w += 3;
    return (src, w);
}
printf("holder: %y\n", holder());
