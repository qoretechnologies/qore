# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern
auto sub typed<T>(auto input) { *T value; sub(reference<*T> r) { r = input; }(\value); return value; }
auto sub untyped<T>(auto input) { *T value; sub(reference r) { r = input; }(\value); return value; }
auto sub direct<T>(auto input) { *T value; value = input; return value; }
code sub setter<T>() { *T value; return auto sub(auto input) { value = input; return value; }; }
auto sub noNarrow() { list<auto!> value = (1,); reference r = \value; r += ("s",); return value; }
sub show(string name, code f) { try { printf("%s: %y\n",name,f()); } catch(hash<ExceptionInfo> ex){printf("%s: %s %s\n",name,ex.err,ex.desc);} }
show("typed",auto sub(){ return typed<softint>("7"); });
show("untyped",auto sub(){ return untyped<softint>("7"); });
show("direct",auto sub(){ return direct<softint>("7"); });
show("closure",auto sub(){ code c=setter<softint>();return c("7"); });
show("no-narrow",\noNarrow());

auto sub plainDefault<T>() { T value; return value; }
auto sub refDefault<T>() { T value; auto r = \value; return value; }
auto sub closureDefault<T>() { T value; code c=auto sub(){return value;};return c(); }
show("default-int",auto sub(){return (plainDefault<int>(),refDefault<int>(),closureDefault<int>());});
show("default-string",auto sub(){return (plainDefault<string>(),refDefault<string>(),closureDefault<string>());});

reference<*T> sub makeRef<T>() { *T value; return \value; }
auto sub copyRef() { reference first=makeRef<string>(); reference second=\first; second="s"; return first; }
show("copied-reference", \copyRef());

auto sub copyRefReject() {
    reference first = makeRef<string>();
    reference second = \first;
    second = "s";
    try { second = 42; }
    catch (hash<ExceptionInfo> ex) { return (ex.err, first); }
    return "missed";
}
show("copied-reference-reject", \copyRefReject());
