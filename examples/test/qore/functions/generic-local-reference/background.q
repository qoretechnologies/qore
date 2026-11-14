# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern
sub writeInOtherContext<U>(Queue start, Queue done, reference r, auto y) {
    try {
        start.get(10s);
        r = y;
        done.push(sprintf("%y", r));
    } catch (hash<ExceptionInfo> ex) { done.push(ex.err + ": " + ex.desc); }
}
int sub startReference<T>(Queue start, Queue done, auto y) {
    *T value;
    return background writeInOtherContext<int>(start, done, \value, y);
}
sub runClosure(Queue start, Queue done, code setter) {
    try {
        start.get(10s);
        done.push(sprintf("%y", setter()));
    } catch (hash<ExceptionInfo> ex) { done.push(ex.err + ": " + ex.desc); }
}
int sub startClosure<T>(Queue start, Queue done, auto y) {
    *T value;
    code setter = auto sub () { value = y; return value; };
    return background runClosure(start, done, setter);
}
sub probe(string name, code start_worker) {
    Queue start();
    Queue done();
    int tid = start_worker(start, done);
    on_exit { thread_join(tid); }
    start.push(True);
    printf("%s: %s\n", name, done.get(10s));
}
probe("ref-string", int sub (Queue s, Queue d) { return startReference<string>(s, d, "s"); });
probe("ref-int", int sub (Queue s, Queue d) { return startReference<int>(s, d, 7); });
probe("ref-invalid", int sub (Queue s, Queue d) { return startReference<string>(s, d, 7); });
probe("closure-string", int sub (Queue s, Queue d) { return startClosure<string>(s, d, "s"); });
probe("closure-int", int sub (Queue s, Queue d) { return startClosure<int>(s, d, 7); });
probe("closure-invalid", int sub (Queue s, Queue d) { return startClosure<string>(s, d, 7); });
