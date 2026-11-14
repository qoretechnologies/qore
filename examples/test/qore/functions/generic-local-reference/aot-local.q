# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern
int sub localAliases() {
    int value = 10;
    reference<int> first = \value;
    reference<int> second = \first;
    second += 5;
    return value;
}
int sub unassignedReference() {
    reference<int> first;
    reference<int> second = \first;
    second = 42;
    return first;
}
reference<int> sub escapedLocal() {
    int value = 7;
    return \value;
}
auto sub escaped() {
    reference<int> value = escapedLocal();
    value += 3;
    return value;
}
auto sub concreteNoNarrow() {
    list<auto!> value = (1,);
    reference r = \value;
    r += ("s",);
    return value;
}
class SelfSource {
    private { int value = 17; }
    reference<SelfSource> ref() { return \self; }
    int read() { return value; }
}
int sub selfReference() {
    SelfSource source();
    reference<SelfSource> r = source.ref();
    return r.read();
}
auto sub rejectedValue() {
    int value = 1;
    reference r = \value;
    try { r = "bad"; } catch (hash<ExceptionInfo> ex) { return (ex.err, value); }
    return "missed";
}
printf("aliases: %y\n", localAliases());
printf("unassigned: %y\n", unassignedReference());
printf("escaped: %y\n", escaped());
printf("no-narrow: %y\n", concreteNoNarrow());
printf("self: %y\n", selfReference());
printf("rejected: %y\n", rejectedValue());
