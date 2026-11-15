# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern

our list<string> events = ();
our int live = 0;
our *object escaped;

class Argument {
    private { string name; }
    constructor(string n = "argument") { name = n; }
    destructor() { push events, name; throw "ARGUMENT"; }
}
class Product {
    constructor(any value) { ++live; push events, "constructed"; }
    destructor() { --live; push events, "product"; }
}
class ListProduct {
    constructor(list<auto> value) { ++live; push events, "constructed"; }
    destructor() { --live; push events, "product"; }
}
class HashProduct {
    constructor(hash<auto> value) { ++live; push events, "constructed"; }
    destructor() { --live; push events, "product"; }
}
class VarargProduct {
    constructor(...) { ++live; push events, "constructed"; }
    destructor() { --live; push events, "product"; }
}
class Base {
    constructor() { push events, "base"; }
    destructor() { push events, "base-dtor"; }
}
class Derived inherits Base {
    constructor(any value, ...) : Base() { ++live; push events, "constructed"; }
    destructor() { --live; push events, "product"; }
}
class ThrowingProduct {
    constructor(any value) { ++live; push events, "constructed"; }
    destructor() { --live; push events, "product"; throw "PRODUCT"; }
}
class EscapingProduct {
    constructor(any value) { ++live; push events, "constructed"; escaped = self; }
    destructor() { --live; push events, "product"; }
}
class BodyFailure inherits Base {
    constructor(int value) : Base() { push events, "body"; throw "BODY"; }
    destructor() { push events, "unexpected-body-dtor"; }
}
int sub memberFailure() { push events, "member"; throw "MEMBER"; }
class MemberFailure {
    private { int value = memberFailure(); }
    constructor(int value) { push events, "unexpected-body"; }
    destructor() { push events, "unexpected-member-dtor"; }
}
class FailingBase {
    constructor() { push events, "base"; throw "BASE"; }
    destructor() { push events, "unexpected-base-dtor"; }
}
class BaseFailure inherits FailingBase {
    constructor(int value) : FailingBase() { push events, "unexpected-body"; }
    destructor() { push events, "unexpected-derived-dtor"; }
}

class DefaultProduct {
    constructor(any value = new Argument()) { ++live; push events, "constructed"; }
    destructor() { --live; push events, "product"; }
}
class EarlyProduct {
    constructor(any value) { ++live; push events, "constructed"; return; }
    destructor() { --live; push events, "product"; }
}
class LocalFailure {
    constructor(int value) { Argument local("local"); push events, "body"; }
    destructor() { push events, "unexpected-local-dtor"; }
}
class BaseArgument {
    constructor(any value) { push events, "base"; }
    destructor() { push events, "unexpected-base-dtor"; }
}
class BaseArgumentFailure inherits BaseArgument {
    constructor() : BaseArgument(new Argument()) { push events, "unexpected-body"; }
    destructor() { push events, "unexpected-derived-dtor"; }
}
class ReplacedArgv {
    constructor(...) { argv = (new Argument("replacement"),); ++live; push events, "constructed"; }
    destructor() { --live; push events, "product"; }
}
class RemovedArgv {
    constructor(...) { auto old = shift argv; push events, "body"; delete old; push events, "unexpected-body"; }
    destructor() { push events, "unexpected-argv-dtor"; }
}
sub defaultArgument() { auto product = new DefaultProduct(); }
sub earlyReturn() { auto product = new EarlyProduct(new Argument()); }
sub localFailure() { auto product = new LocalFailure(1); }
sub baseArgumentFailure() { auto product = new BaseArgumentFailure(); }
sub replacedArgv() { auto product = new ReplacedArgv(1); }
sub removedArgv() { auto product = new RemovedArgv(new Argument()); }

class GenericProduct<T> {
    private { T value; }
    constructor(T n, any arg) { value = n; ++live; push events, "constructed"; }
    destructor() { --live; push events, "product"; }
}
class BodyArgumentFailure {
    constructor(any value) { push events, "body"; throw "BODY"; }
    destructor() { push events, "unexpected-body-dtor"; }
}
sub genericArgument() { auto product = new GenericProduct<int>(1, new Argument()); }
sub bodyArgumentFailure() { auto product = new BodyArgumentFailure(new Argument()); }

class ReplacedParameter {
    constructor(any value) { value = new Argument(); ++live; push events, "constructed"; }
    destructor() { --live; push events, "product"; }
}
sub replacedParameter() { auto product = new ReplacedParameter(1); }

class ReplacedListParameter {
    constructor(list<auto> value) { value = (new Argument(),); ++live; push events, "constructed"; }
    destructor() { --live; push events, "product"; }
}
class ReplacedHashParameter {
    constructor(hash<auto> value) { value = {"value": new Argument()}; ++live; push events, "constructed"; }
    destructor() { --live; push events, "product"; }
}
sub replacedListParameter() { auto product = new ReplacedListParameter((1,)); }
sub replacedHashParameter() { auto product = new ReplacedHashParameter({"value": 1}); }

sub objectArgument() { auto product = new Product(new Argument()); push events, "unexpected-result"; }
sub listArgument() { auto product = new ListProduct((new Argument(),)); push events, "unexpected-result"; }
sub hashArgument() { auto product = new HashProduct({"value": new Argument()}); push events, "unexpected-result"; }
sub extraArgument() { auto product = new VarargProduct(new Argument()); push events, "unexpected-result"; }
sub derivedArgument() { auto product = new Derived(1, new Argument()); push events, "unexpected-result"; }
sub throwingDestructor() { auto product = new ThrowingProduct(new Argument()); push events, "unexpected-result"; }
sub escapingObject() { auto product = new EscapingProduct(new Argument()); push events, "unexpected-result"; }
sub bodyFailure() { auto product = new BodyFailure(1); push events, "unexpected-result"; }
sub initFailure() { auto product = new MemberFailure(1); push events, "unexpected-result"; }
sub baseFailure() { auto product = new BaseFailure(1); push events, "unexpected-result"; }
sub success() { auto product = new Product(1); push events, "result"; }

sub run(string name, code callback) {
    events = ();
    try { callback(); }
    catch (hash<ExceptionInfo> ex) {
        push events, "caught:" + ex.err;
        if (ex.next) push events, "next:" + ex.next.err;
    }
    if (escaped) {
        push events, "held:" + live;
        escaped = NOTHING;
    }
    printf("%s: %y live=%d\n", name, events, live);
}
for (int iteration = 0; iteration < 3; ++iteration) {
    run("object", \objectArgument());
    run("list", \listArgument());
    run("hash", \hashArgument());
    run("varargs", \extraArgument());
    run("derived", \derivedArgument());
    run("throwing", \throwingDestructor());
    run("escaped", \escapingObject());
    run("body-failure", \bodyFailure());
    run("member-failure", \initFailure());
    run("base-failure", \baseFailure());
    run("success", \success());
    run("default", \defaultArgument());
    run("early", \earlyReturn());
    run("local-failure", \localFailure());
    run("base-argument-failure", \baseArgumentFailure());
    run("replaced-argv", \replacedArgv());
    run("removed-argv", \removedArgv());
    run("generic", \genericArgument());
    run("body-argument-failure", \bodyArgumentFailure());
    run("replaced-parameter", \replacedParameter());
    run("replaced-list", \replacedListParameter());
    run("replaced-hash", \replacedHashParameter());
}
