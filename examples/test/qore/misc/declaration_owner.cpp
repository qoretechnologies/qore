/* Copyright (C) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <qore/Qore.h>

#include <cstdio>
#include <stdexcept>
#include <vector>

static unsigned checks = 0;

static void require(bool valid, const char* message) {
    ++checks;
    if (!valid) {
        throw std::runtime_error(message);
    }
}

// A module must invalidate every handle that escaped into the other language, not just erase
// its lookup map. This test keeps the handles after Program destruction to check invalidation.
struct Handle {
    QoreProgram* owner;
    const TypedHashDecl* hashdecl;
    const TypedHashDecl* concrete;
    const QoreEnumDecl* enumeration;
    unsigned cleanups = 0;
    bool valid_at_cleanup = false;
};

// Test Programs have no background threads; callbacks run synchronously on this thread.
static thread_local std::vector<Handle>* handles = nullptr;

static void cleanup(QoreProgram* pgm) {
    if (!handles) {
        return;
    }
    for (auto& h : *handles) {
        if (h.owner != pgm) {
            continue;
        }
        ++h.cleanups;
        h.valid_at_cleanup = h.hashdecl && h.hashdecl->getProgram() == pgm
            && h.concrete && h.concrete->getProgram() == pgm
            && h.enumeration && h.enumeration->getProgram() == pgm
            && h.enumeration->findMember("Ready");
        h.hashdecl = h.concrete = nullptr;
        h.enumeration = nullptr;
        h.owner = nullptr;
    }
}

class CleanupRegistration {
public:
    explicit CleanupRegistration(std::vector<Handle>& h) {
        handles = &h;
        qore_register_program_cleanup_callback(cleanup);
    }
    ~CleanupRegistration() {
        qore_deregister_program_cleanup_callback(cleanup);
        handles = nullptr;
    }
};

static const char* source = R"QORE(
    public namespace Records {
        public hashdecl Record { int a = 0; }
        public hashdecl Box<T> { T value; }
        public enum State : int { Ready = 1, Done = 2 }
    }
    hash<Records::Record> sub record() { return <Records::Record>{"a": 17}; }
    hash<Records::Box<int>> sub box() {
        hash<Records::Box<int>> value({"value": 23});
        return value;
    }
    Program sub child() { return new Program(PO_MODERN); }
    Program sub imported() {
        Program target(PO_MODERN | PO_NO_INHERIT_USER_HASHDECLS);
        target.importHashDecl("Records::Record");
        target.importHashDecl("Records::Box");
        return target;
    }
)QORE";

static Handle inspect(QoreProgram* pgm) {
    const QoreNamespace* ns = nullptr;
    const TypedHashDecl* hd = pgm->findHashDecl("Records::Record", ns);
    require(hd && ns && hd->getProgram() == pgm && ns->getProgram() == pgm,
        "nested hashdecl has the wrong owner");
    const QoreEnumDecl* ed = pgm->findEnum("Records::State", ns);
    require(ed && ns && ed->getProgram() == pgm && ns->getProgram() == pgm,
        "nested enum has the wrong owner");
    const TypedHashDecl* generic = pgm->findHashDecl("Records::Box", ns);
    require(generic && generic->getProgram() == pgm, "generic hashdecl has the wrong owner");
    const TypedHashDecl* concrete = generic->getParameterizedHashDecl(type_vec_t{bigIntTypeInfo});
    require(concrete && concrete->getProgram() == pgm, "concrete hashdecl has the wrong owner");
    require(concrete == generic->getParameterizedHashDecl(type_vec_t{bigIntTypeInfo}),
        "generic declaration cache changed identity");

    const TypedHashDecl* builtin = pgm->findHashDecl("Qore::StatInfo", ns);
    require(builtin && builtin->isSystem() && builtin != hashdeclStatInfo && builtin->getProgram() == pgm,
        "Program copy of a builtin hashdecl was treated as process-global");
    const QoreEnumDecl* builtin_enum = pgm->findEnum("Qore::ExecMode", ns);
    require(builtin_enum && builtin_enum->isSystem() && builtin_enum != enumExecMode
        && builtin_enum->getProgram() == pgm, "Program copy of a builtin enum was treated as process-global");

    TypedHashDeclHolder copied_hash(new TypedHashDecl(*hd));
    QoreEnumDeclHolder copied_enum(new QoreEnumDecl(*ed));
    require(!copied_hash->getProgram() && !copied_enum->getProgram(),
        "unattached copies must not inherit the source owner");
    return {pgm, hd, concrete, ed};
}

static void programOwners(qore_exec_mode_t mode) {
    ExceptionSink xsink;
    std::vector<Handle> saved;
    CleanupRegistration registration(saved);
    ValueHolder record(&xsink);
    ValueHolder box(&xsink);
    const TypedHashDecl* retained = nullptr;
    const TypedHashDecl* concrete = nullptr;
    {
        QoreProgramHelper pgm(PO_MODERN, xsink);
        pgm->setExecMode(mode, true);
        pgm->parse(source, "declaration-owner", &xsink);
        require(!xsink, "cannot parse declaration fixture");
        saved.push_back(inspect(*pgm));
        retained = saved[0].hashdecl;
        concrete = saved[0].concrete;
        record = pgm->callFunction("record", nullptr, &xsink);
        require(!xsink && record->getType() == NT_HASH, "cannot retain a hash value");
        box = pgm->callFunction("box", nullptr, &xsink);
        require(!xsink && box->getType() == NT_HASH, "cannot retain a generic hash value");
        require(record->get<const QoreHashNode>()->getHashDecl() == retained
            && box->get<const QoreHashNode>()->getHashDecl() == concrete,
            "returned values must retain the declarations being watched");

        for (const char* factory : {"child", "imported"}) {
            {
                // Both inheritance and explicit import create copies owned by the receiving Program.
                ValueHolder child_value(pgm->callFunction(factory, nullptr, &xsink), &xsink);
                require(!xsink && child_value->getType() == NT_OBJECT, "cannot create child Program");
                ReferenceHolder<QoreProgram> child(
                    child_value->get<const QoreObject>()->getReferencedPrivateData<QoreProgram>(CID_PROGRAM, &xsink),
                    &xsink);
                require(!xsink && *child, "cannot access child Program");
                saved.push_back(inspect(*child));
                require(saved.back().hashdecl != retained && saved.back().enumeration != saved[0].enumeration,
                    "child must own distinct declaration copies");
            }
            require(!xsink, "child Program cleanup raised an exception");
            const Handle& child = saved.back();
            require(child.cleanups == 1 && child.valid_at_cleanup && !child.hashdecl
                && !child.concrete && !child.enumeration, "child handles were not invalidated before deletion");
            require(!saved[0].cleanups && retained->getProgram() == *pgm,
                "child cleanup invalidated its parent's declaration");
        }

        // A failed parse must not change the owner of previously committed declarations.
        pgm->parse("hashdecl FailedRecord { MissingType value; }", "failed-declaration", &xsink);
        require(static_cast<bool>(xsink), "invalid declaration unexpectedly parsed");
        xsink.clear();
        require(retained->getProgram() == *pgm && concrete->getProgram() == *pgm,
            "parse rollback changed a committed declaration's owner");
    }
    require(!xsink, "Program cleanup raised an exception");
    require(saved[0].cleanups == 1 && saved[0].valid_at_cleanup && !saved[0].hashdecl
        && !saved[0].concrete && !saved[0].enumeration, "parent handles were not invalidated before deletion");
    // The hash values retain their declarations, but do not retain the namespace. This must
    // also work for concrete generic declarations, which live in the generic's cache.
    require(!retained->getProgram() && !retained->getNamespace(), "hashdecl kept a deleted namespace");
    require(!concrete->getProgram() && !concrete->getNamespace(), "generic hashdecl kept a deleted namespace");
    require(record->get<const QoreHashNode>()->getKeyValue("a").getAsBigInt() == 17,
        "retained hash value changed during cleanup");
    require(box->get<const QoreHashNode>()->getKeyValue("value").getAsBigInt() == 23,
        "retained generic hash value changed during cleanup");
}

static void cachedBeforeNamespace() {
    ExceptionSink xsink;
    ReferenceHolder<QoreHashNode> retained(&xsink);
    const TypedHashDecl* concrete = nullptr;
    {
        QoreProgramHelper pgm(PO_MODERN, xsink);
        QoreProgramContextHelper context(*pgm);
        TypedHashDeclHolder generic(new TypedHashDecl("LateBox", "LateBox"));
        generic->addTypeParameter("T");
        concrete = generic->getParameterizedHashDecl(type_vec_t{bigIntTypeInfo});
        require(concrete && !concrete->getProgram(), "unattached generic instance has an owner");
        const TypedHashDecl* base = *generic;
        pgm->getRootNS()->addSystemHashDecl(generic.release());
        require(base->getProgram() == *pgm && concrete->getProgram() == *pgm,
            "namespace assignment did not bind a previously cached generic instance");
        require(base->getParameterizedHashDecl(type_vec_t{bigIntTypeInfo}) == concrete,
            "namespace assignment replaced a cached instance");
        retained = new QoreHashNode(concrete, &xsink);
        require(!xsink, "cannot retain a previously unattached generic instance");
    }
    require(!xsink, "late-bound declaration cleanup raised an exception");
    require(!concrete->getProgram() && !concrete->getNamespace(),
        "late-bound generic instance kept a deleted namespace");
}

int main() {
    qore_init(QL_MIT, "UTF-8", false, QLO_DISABLE_SIGNAL_HANDLING);
    int status = 0;
    try {
        require(hashdeclStatInfo && !hashdeclStatInfo->getProgram(), "static hashdecl has a Program owner");
        require(enumExecMode && !enumExecMode->getProgram(), "static enum has a Program owner");
        TypedHashDeclHolder unattached_hash(new TypedHashDecl("Unattached", "Unattached"));
        QoreEnumDeclHolder unattached_enum(new QoreEnumDecl("Unattached", "Unattached", bigIntTypeInfo));
        require(!unattached_hash->getProgram() && !unattached_enum->getProgram(),
            "unattached declarations have a Program owner");
        cachedBeforeNamespace();
        for (qore_exec_mode_t mode : {QEM_AST, QEM_IR, QEM_JIT, QEM_TIERED}) {
            programOwners(mode);
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL after %u checks: %s\n", checks, error.what());
        status = 1;
    }
    qore_cleanup();
    if (!status) {
        std::printf("PASS: %u declaration owner checks (AST, IR, JIT, tiered)\n", checks);
    }
    return status;
}
