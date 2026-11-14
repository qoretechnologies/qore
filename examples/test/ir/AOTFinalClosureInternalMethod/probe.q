#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# the probe of AOTFinalClosureInternalMethod.qtest: a closure in a method of a final class calls a private:internal
# method of the class through self (qore#5975); its neighbours are the cases that already worked

%modern

# the issue's case and the call in larger expressions in the closure
final class G {
    private:internal int fi(int x) { printf("fi %d\n", x); return x; }
    private:internal static int sfi(int x) { printf("sfi %d\n", x); return x; }
    run() {
        (sub () { self.fi(1); })();
        printf("r2=%y\n", (auto sub () { return (self.fi(2), self.fi(3)); })());
        printf("r3=%y\n", (auto sub () { return self.fi(4) + 1; })());
        printf("r4=%y\n", (auto sub () { return {"a": self.fi(5)}; })());
        printf("r5=%y\n", (auto sub () { return sprintf("%d", self.fi(6)); })());
        printf("r6=%y\n", (auto sub () { return True ? self.fi(7) : 0; })());
        printf("r7=%y\n", (auto sub () { return sfi(8); })());
        printf("r8=%y\n", (auto sub () { G g = self; return g.fi(9); })());
        printf("r9=%y\n", (auto sub () { return (auto sub () { return self.fi(10); })(); })());
        code c = \self.fi();
        printf("r10=%y\n", (auto sub () { return c(11); })());
        printf("r11=%y\n", (auto sub (G o) { return o.fi(12); })(self));
        # outside a closure
        printf("r12=%y\n", self.fi(13));
        printf("r13=%y\n", (auto sub () { return fi(14); })());
    }
}

# a private method of a final class, and the methods of classes that are not final
final class GP {
    private int fp(int x) { printf("GP::fp %d\n", x); return x; }
    run() { (sub () { self.fp(1); })(); }
}

class NI {
    private:internal int fi(int x) { printf("NI::fi %d\n", x); return x; }
    run() { (sub () { self.fi(1); })(); }
}

class NP {
    private int fp(int x) { printf("NP::fp %d\n", x); return x; }
    run() { (sub () { self.fp(1); })(); }
}

final class GU {
    int fu(int x) { printf("GU::fu %d\n", x); return x; }
    run() { (sub () { self.fu(1); })(); }
}


# a private:internal static method called through self, and a normal and a static method of one name
final class GS {
    private:internal int f(int x) { printf("GS::f normal %d\n", x); return x; }
    static int f(string x) { printf("GS::f static %s\n", x); return 0; }
    private:internal static int sf(int x) { printf("GS::sf static %d\n", x); return x; }
    run() {
        (sub () { self.f(1); })();
        (sub () { self.sf(2); })();
    }
}

# overloaded private:internal methods
final class GO {
    private:internal int ov(int x) { printf("GO::ov int %d\n", x); return x; }
    private:internal int ov(string x) { printf("GO::ov string %s\n", x); return 0; }
    run() { (sub () { self.ov(1); self.ov("two"); })(); }
}

# private:internal inheritance and a method that shadows one of the parent class; a private:internal method of a class
# that is not final whose subclasses have public methods of the name
class IP {
    pm(int x) { printf("IP::pm %d\n", x); }
    private:internal pi(int x) { printf("IP::pi %d\n", x); }
    private:internal sh(int x) { printf("IP::sh %d\n", x); }
    prun() { (sub () { self.pi(1); self.sh(2); })(); }
}
final class IC inherits private:internal IP {
    private:internal sh(int x) { printf("IC::sh %d\n", x); }
    run() { (sub () { self.pm(3); self.sh(4); })(); prun(); }
}
class IQ {
    private:internal v(int x) { printf("IQ::v %d\n", x); }
    qrun() { (sub () { self.v(5); })(); }
}
class IR inherits IQ {
    v(int x) { printf("IR::v %d\n", x); }
}
final class IS inherits IQ {
    v(int x) { printf("IS::v %d\n", x); }
    srun() { (sub () { self.v(6); })(); qrun(); }
}

# a static method of the parent class and a private:internal normal method of the final class with the same name
class KP {
    static x(string v) { printf("KP::x static %s\n", v); }
}
final class KC inherits KP {
    private:internal x(int v) { printf("KC::x normal %d\n", v); }
    run() { (sub () { self.x(1); })(); self.x(2); }
}

G g();
g.run();
GP gp();
gp.run();
NI ni();
ni.run();
NP np();
np.run();
GU gu();
gu.run();
GS gs();
gs.run();
GO go();
go.run();
IC ic();
ic.run();
IR ir();
ir.qrun();
IS is();
is.srun();
KC kc();
kc.run();
