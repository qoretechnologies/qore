%modern
# the worker of deleted-program-own-generic-cycle.qtest: a Program whose global variables hold generic values whose
# type arguments are its own classes (an object of its own generic class, a typed hash of its own generic hashdecl, a
# list of objects of its own generic class and an object of a generic class of the parent Program) is deleted in each
# generation; the values' destructors run, and in debug builds no declaration of the Program is kept after it is
# deleted: the ones that derived types name are retired, the derived types are repointed to their tombstones, and the
# declarations are freed once no thread can read them (issue #6419), here when this thread is made quiescent

# Copyright (C) 2026 Qore Technologies, s.r.o.

# a generic class of the parent Program, which the child Program inherits
public class ParentBox<T> {
    public {
        T v;
    }

    constructor(T v) {
        self.v = v;
    }

    destructor() {
        note_destroyed("ParentBox");
    }
}

# the destructors of the child Program's values count here, as the child Program is gone when they are read
our hash<string, int> destroyed;

sub note_destroyed(string what) {
    ++destroyed{what};
}

const Opts = PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS;

const Src = "
class Foo {
    destructor() {
        note_destroyed(\"Foo\");
    }
}

class Box<T> {
    public {
        T v;
    }

    constructor(T v) {
        self.v = v;
    }

    destructor() {
        note_destroyed(\"Box\");
    }
}

hashdecl HBox<T> {
    T v;
}

# the global variables hold values whose types name the Program's own declarations
our Box<Foo> g = new Box<Foo>(new Foo());
our hash<HBox<Foo>> h = new hash<HBox<Foo>>({\"v\": new Foo()});
our list<Box<Foo>> l = (new Box<Foo>(new Foo()),);
our ParentBox<Foo> pb = new ParentBox<Foo>(new Foo());

int sub run() {
    return g.v && h.v && l[0].v && pb.v ? 1 : 0;
}
";

const Generations = 10;

# the debug functions that count the declarations kept after their Program is deleted are in debug builds only
bool counted = Option::HAVE_LIBRARY_DEBUGGING;

int sub count_kept() {
    return counted
        ? call_function("dbg_retired_type_declarations") + call_function("dbg_type_declaration_groups")
        : 0;
}

list<int> kept;
for (int gen = 0; gen < Generations; ++gen) {
    int before = count_kept();
    {
        Program p(Opts);
        p.importFunction("note_destroyed");
        p.parse(Src, "own-generic-cycle");
        p.run();
        if (p.callFunction("run") != 1) {
            printf("generation %d: the values were not created\n", gen);
        }
    }
    if (counted && exists_function("dbg_type_retirement_free")) {
        # this thread is quiescent while the hook frees what no thread can read anymore
        call_function("dbg_type_retirement_free");
    }
    kept += count_kept() - before;
}
printf("destroyed: %y\n", destroyed);
if (!counted) {
    printf("kept per generation: n/a\n");
} else {
    printf("kept per generation: %s\n", (select kept, $1) ? sprintf("not zero: %y", kept) : "0");
}
