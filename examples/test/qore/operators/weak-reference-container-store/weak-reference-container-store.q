%modern
%allow-weak-references
# writes through a local that holds a weak reference to a list or a hash change the container that the weak
# reference designates, in every mode (qore#7299); every mode must print the output of the AST interpreter

sub lists() {
    list<int> src = (1, 2);
    list<int> l;
    l := src;
    int i = 1;
    try {
        l[2] = 3;
        printf("list index store: %y\n", src);
    } catch (hash<ExceptionInfo> ex) {
        printf("list index store: %s\n", ex.err);
    }
    try {
        l[0] += 10;
        printf("list element +=: %y\n", src);
    } catch (hash<ExceptionInfo> ex) {
        printf("list element +=: %s\n", ex.err);
    }
    try {
        push l, 4;
        printf("list push: %y\n", src);
    } catch (hash<ExceptionInfo> ex) {
        printf("list push: %s\n", ex.err);
    }
    try {
        unshift l, 0;
        printf("list unshift: %y\n", src);
    } catch (hash<ExceptionInfo> ex) {
        printf("list unshift: %s\n", ex.err);
    }
    try {
        l += 5;
        printf("list +=: %y\n", src);
    } catch (hash<ExceptionInfo> ex) {
        printf("list +=: %s\n", ex.err);
    }
    try {
        splice l, 0, 1;
        printf("list splice: %y\n", src);
    } catch (hash<ExceptionInfo> ex) {
        printf("list splice: %s\n", ex.err);
    }
    try {
        extract l, 0, 1;
        printf("list extract: %y\n", src);
    } catch (hash<ExceptionInfo> ex) {
        printf("list extract: %s\n", ex.err);
    }
    try {
        pop l;
        printf("list pop: %y\n", src);
    } catch (hash<ExceptionInfo> ex) {
        printf("list pop: %s\n", ex.err);
    }
    try {
        shift l;
        printf("list shift: %y\n", src);
    } catch (hash<ExceptionInfo> ex) {
        printf("list shift: %s\n", ex.err);
    }
    try {
        l[i] = 7;
        printf("list dynamic index store: %y\n", src);
    } catch (hash<ExceptionInfo> ex) {
        printf("list dynamic index store: %s\n", ex.err);
    }
    printf("l: %y\n", l);
}

sub hashes() {
    hash<string, int> src = {"a": 1};
    hash<string, int> h;
    h := src;
    string k = "c";
    try {
        h.b = 2;
        printf("hash key store: %y\n", src);
    } catch (hash<ExceptionInfo> ex) {
        printf("hash key store: %s\n", ex.err);
    }
    try {
        h{k} = 3;
        printf("hash dynamic key store: %y\n", src);
    } catch (hash<ExceptionInfo> ex) {
        printf("hash dynamic key store: %s\n", ex.err);
    }
    try {
        h.a += 10;
        printf("hash key +=: %y\n", src);
    } catch (hash<ExceptionInfo> ex) {
        printf("hash key +=: %s\n", ex.err);
    }
    try {
        h += {"d": 4};
        printf("hash +=: %y\n", src);
    } catch (hash<ExceptionInfo> ex) {
        printf("hash +=: %s\n", ex.err);
    }
    try {
        remove h.b;
        printf("hash remove: %y\n", src);
    } catch (hash<ExceptionInfo> ex) {
        printf("hash remove: %s\n", ex.err);
    }
    try {
        delete h.c;
        printf("hash delete: %y\n", src);
    } catch (hash<ExceptionInfo> ex) {
        printf("hash delete: %s\n", ex.err);
    }
    try {
        h -= "d";
        printf("hash -=: %y\n", src);
    } catch (hash<ExceptionInfo> ex) {
        printf("hash -=: %s\n", ex.err);
    }
    printf("h: %y\n", h);
}

sub untyped() {
    list<auto> src = (1, 2);
    auto l;
    l := src;
    l[2] = 3;
    printf("auto list: %y\n", src);
    hash<auto> hs = {"a": 1};
    auto h;
    h := hs;
    h.b = 2;
    printf("auto hash: %y\n", hs);
}

sub loop() {
    list<int> src = ();
    list<int> l;
    l := src;
    for (int i = 0; i < 5; ++i) {
        l[i] = i;
    }
    printf("loop: %y\n", src);
}

sub shared() {
    # a list held by two variables, written through a weak reference to it: both see the change, as in the AST
    list<int> a = (1,);
    list<int> b = a;
    list<int> l;
    l := a;
    l[1] = 2;
    printf("shared: a %y b %y\n", a, b);
}

sub freed() {
    list<int> l;
    {
        list<int> src = (1,);
        l := src;
    }
    try {
        l[0] = 5;
        printf("freed: %y\n", l);
    } catch (hash<ExceptionInfo> ex) {
        printf("freed: %s\n", ex.err);
    }
}

lists();
hashes();
untyped();
loop();
shared();
freed();
