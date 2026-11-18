%modern
# Copyright (C) 2026 Qore Technologies, s.r.o.

hashdecl Point { int x; }
hashdecl Item<T> { T value; }
class Box<T> {
    hash<Item<T>> make(auto value) {
        hash<Item<T>> item({"value": value});
        return item;
    }
}

auto sub text() { return "bad"; }

int sub direct(int value) {
    hash<Point> p({"x": value});
    return p.x;
}

int sub invoked(int value) {
    try {
        hash<Point> p({"x": value});
        return p.x;
    } catch (hash<ExceptionInfo> ex) {
        return -1;
    }
}

string sub invalid() {
    try {
        hash<Point> p({"x": text()});
        return "accepted";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

Box<int> ints();
Box<string> strings();
for (int i = 0; i < 3; ++i) {
    printf("%d:%d:%s:%d:%s\n", direct(i), invoked(i + 1), invalid(), ints.make(i).value,
        strings.make("ok").value);
}
