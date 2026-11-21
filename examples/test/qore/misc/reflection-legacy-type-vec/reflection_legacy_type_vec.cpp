/* Copyright (c) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
// The deprecated one-argument forms of qore_type_get_type_arguments() and qore_type_get_code_param_types(), which the
// earlier module API 2.3 headers declared, are still exported with their original symbols and give the same types as
// the forms that copy them to a vector of the caller (qore#6936).  The pointer that they return is stable, and when
// the declarations that the types name are freed with their deleted Program, the vector's elements are the tombstones
// that the derived type itself names, never a freed type.
//
// usage: reflection-legacy-type-vec <ast|ir|jit|tiered>
// prints "reflection-legacy-type-vec: ok" when every check passes, else each failure and exits with 1
#include <qore/Qore.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <dlfcn.h>

namespace {
//! the signature of the deprecated one-argument forms, as an old module calls them
typedef const type_vec_t* (*legacy_fn_t)(const QoreTypeInfo*);

//! the symbols of the deprecated forms, as a module built with the earlier module API 2.3 headers refers to them
//! (Itanium C++ ABI; dlsym() adds the leading underscore of Mach-O itself)
const char* const LEGACY_TYPE_ARGS_SYMBOL = "_Z28qore_type_get_type_argumentsPK12QoreTypeInfo";
const char* const LEGACY_PARAM_TYPES_SYMBOL = "_Z30qore_type_get_code_param_typesPK12QoreTypeInfo";

qore_exec_mode_t mode = QEM_AST;
unsigned failures = 0;

void check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("FAILED: %s\n", what.c_str());
        ++failures;
    }
}

std::string describe(ExceptionSink& xsink) {
    QoreValue err = xsink.getExceptionErr();
    QoreValue desc = xsink.getExceptionDesc();
    return std::string(err.getType() == NT_STRING ? err.get<const QoreStringNode>()->c_str() : "?") + ": "
        + (desc.getType() == NT_STRING ? desc.get<const QoreStringNode>()->c_str() : "?");
}

std::string type_name(const QoreTypeInfo* ti) {
    const char* name = qore_type_get_name(ti);
    return name ? name : "<null>";
}

//! parses the code in the program; returns false after printing the error if it cannot be parsed
bool parse(QoreProgram* pgm, const char* code, const char* label) {
    ExceptionSink xsink;
    pgm->setExecMode(mode, true);
    {
        ProgramRuntimeExternalParseContextHelper parse_context(pgm);
        if (!parse_context) {
            std::printf("FAILED: cannot enter the parse context of %s\n", label);
            ++failures;
            return false;
        }
        pgm->parse(code, label, &xsink);
    }
    if (xsink) {
        std::printf("FAILED: parse of %s: %s\n", label, describe(xsink).c_str());
        xsink.clear();
        ++failures;
        return false;
    }
    return true;
}

//! calls a function of the program that returns an int; returns -1 after printing the error on an exception
int64 call_int(QoreProgram* pgm, const char* function) {
    ExceptionSink xsink;
    ValueHolder rv(pgm->callFunction(function, nullptr, &xsink), &xsink);
    if (xsink) {
        std::printf("FAILED: %s(): %s\n", function, describe(xsink).c_str());
        xsink.clear();
        ++failures;
        return -1;
    }
    return rv->getAsBigInt();
}

//! compares the deprecated form's vector with the new form's copy and with the expected names
void compare(const char* label, const type_vec_t* legacy, bool found, const type_vec_t& copy,
        const std::vector<std::string>& names) {
    std::string l(label);
    check(legacy != nullptr, l + ": the deprecated form returns a vector");
    check(found, l + ": the new form returns true");
    if (!legacy) {
        return;
    }
    check(*legacy == copy, l + ": the deprecated form gives the types of the new form");
    check(legacy->size() == names.size(), l + ": " + std::to_string(names.size()) + " types, got "
        + std::to_string(legacy->size()));
    for (size_t i = 0; i < legacy->size() && i < names.size(); ++i) {
        check(type_name((*legacy)[i]) == names[i], l + ": type " + std::to_string(i) + " is " + names[i] + ", got "
            + type_name((*legacy)[i]));
    }
}

//! the program whose declarations the types name; deleted before the second part of the checks
const char* const DECL_CODE = "%modern\n"
    "class Foo {}\n"
    "class Box<T> { public { *T v; } }\n"
    "hashdecl Pair<K, V> { K k; V v; }\n"
    "sub f(Box<int> a, Box<Foo> b, code<string(int, Foo)> c, hash<Pair<string, Foo>> d, int e, code<int(Foo)> g,\n"
    "    code<bool()> h) {}\n"
    // values of the types in the program's execution mode
    "int sub run() {\n"
    "    Box<Foo> b(); b.v = new Foo();\n"
    "    code<string(int, Foo)> c = string sub (int i, Foo x) { return string(i); };\n"
    "    hash<Pair<string, Foo>> d = cast<hash<Pair<string, Foo>>>({\"k\": \"a\", \"v\": new Foo()});\n"
    "    return (c(1, b.v) == \"1\" && d.k == \"a\") ? 1 : 0;\n"
    "}\n";

//! the debug build's retirement bookkeeping (the dbg_* functions); each function returns -2 in a build without them
const char* const FREE_CODE = "%modern\n"
    // the number of retired declarations freed so far
    "int sub freed() {\n"
    "    if (!Option::HAVE_LIBRARY_DEBUGGING) {\n"
    "        return -2;\n"
    "    }\n"
    "    return call_function(\"dbg_type_retirement_info\").\"freed declarations\";\n"
    "}\n"
    // frees what can be freed; returns the number of retired declarations left
    "int sub free_retired() {\n"
    "    if (!Option::HAVE_LIBRARY_DEBUGGING) {\n"
    "        return -2;\n"
    "    }\n"
    "    call_function(\"dbg_wait_bg_compile_idle\");\n"
    "    call_function(\"dbg_type_retirement_free\");\n"
    "    return call_function(\"dbg_type_retirement_info\").\"pending declarations\";\n"
    "}\n";

int run_checks() {
    // the deprecated forms are exported under the symbols that old modules refer to
    legacy_fn_t legacy_type_args = reinterpret_cast<legacy_fn_t>(dlsym(RTLD_DEFAULT, LEGACY_TYPE_ARGS_SYMBOL));
    legacy_fn_t legacy_param_types = reinterpret_cast<legacy_fn_t>(dlsym(RTLD_DEFAULT, LEGACY_PARAM_TYPES_SYMBOL));
    check(legacy_type_args != nullptr, std::string("libqore exports ") + LEGACY_TYPE_ARGS_SYMBOL);
    check(legacy_param_types != nullptr, std::string("libqore exports ") + LEGACY_PARAM_TYPES_SYMBOL);
    if (!legacy_type_args || !legacy_param_types) {
        return 1;
    }

    // a type that is neither parameterized nor a typed code type
    {
        type_vec_t v;
        check(!legacy_type_args(bigIntTypeInfo), "int: the deprecated type arguments form returns nullptr");
        check(!qore_type_get_type_arguments(bigIntTypeInfo), "int: the header's deprecated form returns nullptr");
        check(!qore_type_get_type_arguments(bigIntTypeInfo, v), "int: the new type arguments form returns false");
        check(!legacy_param_types(bigIntTypeInfo), "int: the deprecated parameter types form returns nullptr");
        check(!qore_type_get_code_param_types(bigIntTypeInfo, v), "int: the new parameter types form returns false");
        check(!legacy_type_args(nullptr) && !legacy_param_types(nullptr), "no type: the deprecated forms return nullptr");
    }

    // the program that reads and frees the retired declarations; it outlives the program that declares the types
    ExceptionSink helper_xsink;
    QoreProgramHelper helper(PO_MODERN | PO_NO_CHILD_PO_RESTRICTIONS, helper_xsink);
    if (helper_xsink) {
        std::printf("FAILED: the helper program cannot be created: %s\n", describe(helper_xsink).c_str());
        helper_xsink.clear();
        return 1;
    }
    if (!parse(*helper, FREE_CODE, "free")) {
        return 1;
    }
    int64 freed_before = call_int(*helper, "freed");

    const QoreTypeInfo* box_int;
    const QoreTypeInfo* box_foo;
    const QoreTypeInfo* code_type;
    const QoreTypeInfo* code_after;
    const QoreTypeInfo* code_none;
    const type_vec_t* box_foo_args;
    const type_vec_t* code_params;
    const QoreTypeInfo* foo_before;
    std::string foo_name;
    {
        ExceptionSink xsink;
        QoreProgramHelper pgm(PO_MODERN | PO_NO_CHILD_PO_RESTRICTIONS, xsink);
        if (xsink) {
            std::printf("FAILED: the program cannot be created: %s\n", describe(xsink).c_str());
            xsink.clear();
            return 1;
        }
        if (!parse(*pgm, DECL_CODE, "decls")) {
            return 1;
        }
        check(call_int(*pgm, "run") == 1, "run() of the program returns 1");
        const QoreExternalFunction* f = pgm->findFunction("f");
        check(f != nullptr, "the function f() is found");
        if (!f) {
            return 1;
        }
        const type_vec_t& params = f->getFirstVariant()->getParamTypeList();
        check(params.size() == 7, "f() has 7 parameters");
        if (params.size() != 7) {
            return 1;
        }
        box_int = params[0];
        box_foo = params[1];
        code_type = params[2];
        const QoreTypeInfo* pair_type = params[3];
        const QoreTypeInfo* int_type = params[4];
        code_after = params[5];
        code_none = params[6];

        type_vec_t copy;
        bool found;

        // parameterized class types
        found = qore_type_get_type_arguments(box_int, copy);
        compare("Box<int>", legacy_type_args(box_int), found, copy, {"int"});
        found = qore_type_get_type_arguments(box_foo, copy);
        box_foo_args = legacy_type_args(box_foo);
        compare("Box<Foo>", box_foo_args, found, copy, {"object<Foo>"});
        // the header's declaration of the deprecated form is the exported symbol, and the vector is the type's own
        check(qore_type_get_type_arguments(box_foo) == box_foo_args, "Box<Foo>: the header's deprecated form gives the "
            "same vector");
        check(legacy_type_args(box_foo) == box_foo_args, "Box<Foo>: the deprecated form gives the same vector again");

        // a parameterized hashdecl type: the concrete hashdecl's own vector
        found = qore_type_get_type_arguments(pair_type, copy);
        const type_vec_t* pair_args = legacy_type_args(pair_type);
        compare("hash<Pair<string, Foo>>", pair_args, found, copy, {"string", "object<Foo>"});
        check(legacy_type_args(pair_type) == pair_args, "hash<Pair<string, Foo>>: the deprecated form gives the same "
            "vector again");
        check(!legacy_type_args(int_type) && !legacy_type_args(code_type), "the deprecated type arguments form returns "
            "nullptr for types that are not parameterized");

        // typed code types
        found = qore_type_get_code_param_types(code_type, copy);
        code_params = legacy_param_types(code_type);
        compare("code<string(int, Foo)>", code_params, found, copy, {"int", "object<Foo>"});
        check(qore_type_get_code_param_types(code_type) == code_params, "code<string(int, Foo)>: the header's "
            "deprecated form gives the same vector");
        found = qore_type_get_code_param_types(code_none, copy);
        compare("code<bool()>", legacy_param_types(code_none), found, copy, {});
        check(!legacy_param_types(box_foo) && !legacy_param_types(int_type), "the deprecated parameter types form "
            "returns nullptr for types that are not typed code types");

        foo_before = box_foo_args->empty() ? nullptr : (*box_foo_args)[0];
        foo_name = type_name(foo_before);
        if (failures) {
            return 1;
        }
        // the program is deleted here: its declarations are retired, and the derived types are repointed to their
        // tombstones
    }

    // the derived types live until library cleanup; their declarations' types are tombstones now
    {
        type_vec_t copy;
        bool found = qore_type_get_type_arguments(box_foo, copy);
        check(found && copy.size() == 1 && copy[0] != foo_before, "Box<Foo>: the type argument is repointed to the "
            "tombstone of the deleted program's class");
        compare("Box<Foo> after the deletion", legacy_type_args(box_foo), found, copy, {foo_name});
        check(legacy_type_args(box_foo) == box_foo_args, "Box<Foo>: the deprecated form gives the same vector after "
            "the deletion");
        found = qore_type_get_code_param_types(code_type, copy);
        compare("code<string(int, Foo)> after the deletion", code_params, found, copy, {"int", foo_name});
        // a vector made for the first time after the deletion has the tombstones too
        found = qore_type_get_code_param_types(code_after, copy);
        check(found && copy.size() == 1 && copy[0] != foo_before, "code<int(Foo)>: the parameter type is repointed "
            "to the tombstone of the deleted program's class");
        compare("code<int(Foo)> after the deletion", legacy_param_types(code_after), found, copy, {foo_name});
        found = qore_type_get_type_arguments(box_int, copy);
        compare("Box<int> after the deletion", legacy_type_args(box_int), found, copy, {"int"});
    }

    // the retired declarations are freed (debug builds); the vectors then still name live types (valgrind checks the
    // reads of their names)
    {
        int64 pending = call_int(*helper, "free_retired");
        int64 freed_after = call_int(*helper, "freed");
        if (pending != -2) {
            check(!pending, "no retired declaration is left in a debug build: " + std::to_string(pending) + " left");
            // Foo, whose type the vectors named before it was retired (checked above), is among them, as nothing
            // retired is left
            check(freed_after - freed_before >= 1, "the retired declarations are freed in a debug build: "
                + std::to_string(freed_after - freed_before) + " freed");
        }
    }
    check(type_name((*box_foo_args)[0]) == foo_name, "Box<Foo>: the type argument is " + foo_name
        + " after the free");
    check(type_name((*code_params)[1]) == foo_name, "code<string(int, Foo)>: the parameter type is " + foo_name
        + " after the free");
    return failures ? 1 : 0;
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fputs("usage: reflection-legacy-type-vec <ast|ir|jit|tiered>\n", stderr);
        return 2;
    }
    if (!std::strcmp(argv[1], "ir")) {
        mode = QEM_IR;
    } else if (!std::strcmp(argv[1], "jit")) {
        mode = QEM_JIT;
    } else if (!std::strcmp(argv[1], "tiered")) {
        mode = QEM_TIERED;
    } else if (std::strcmp(argv[1], "ast")) {
        std::fprintf(stderr, "unknown execution mode %s\n", argv[1]);
        return 2;
    }

    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    int rc = run_checks();
    qore_cleanup();
    if (!rc) {
        std::puts("reflection-legacy-type-vec: ok");
    }
    return rc;
}
