%modern
# the worker of ir-guard-profile-types.qtest (debug builds): a function of this Program whose IR guard profiles count
# the type of objects of a class of another Program must not keep that type once the other Program and its objects
# are gone, and counting it must not keep the class's declaration (issue #6421)

# Copyright (C) 2026 Qore Technologies, s.r.o.

public class Base {
    int get() {
        return 0;
    }
}

# the cast to a class type has a guard point, whose profile counts the full type of the value
int sub f(auto x) {
    Base y = cast<Base>(x);
    return y.get();
}

int sub kept() {
    return dbg_retired_type_declarations() + dbg_type_declaration_groups();
}

int kept_before = kept();
for (int gen = 0; gen < 3; ++gen) {
    int before;
    {
        Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES);
        p.parse("class Foo inherits Base { int get() { return 1; } } object sub mk() { return new Foo(); }", "p");
        for (int i = 0; i < 20; ++i) {
            f(p.callFunction("mk"));
        }
        before = dbg_ir_guard_profile_type_count(\f());
    }
    # the class of the deleted Program is destroyed, and its type is no longer counted by the profile
    printf("generation %d: profiled types: %d -> %d\n", gen, before, dbg_ir_guard_profile_type_count(\f()));
}
printf("declarations kept: %d\n", kept() - kept_before);
