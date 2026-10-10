/* Copyright (c) 2026 Qore Technologies, s.r.o.
 * SPDX-License-Identifier: MIT
 */
#include <qore/Qore.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <initializer_list>

static void checkClassHandles() {
    QoreClassHolder base_owner(new QoreClass("IdentityBase", "::IdentityBase"));
    QoreClass* base = base_owner;
    QoreClassHolder copy_owner(base->copy());
    QoreClass* copy = copy_owner;
    QoreClassHolder second_owner(base->copy());
    QoreClass* second = second_owner;
    assert(base != copy && copy != second);

    // Lookup returns the definition's canonical handle, not the queried copy.
    for (QoreClass* source : {base, copy, second}) {
        for (QoreClass* target : {base, copy, second}) {
            bool private_access = true;
            assert(source->getClass(*target, private_access) == base);
            assert(!private_access);
        }
    }

    QoreClassHolder child_owner(new QoreClass("IdentityChild", "::IdentityChild"));
    QoreClass* child = child_owner;
    // A base edge may itself hold a copied handle; its match stays canonical.
    child->addBuiltinBaseClass(copy);
    for (QoreClass* target : {base, copy, second}) {
        bool private_access = true;
        assert(child->getClass(*target, private_access) == base);
        assert(!private_access);
    }
    QoreClassHolder grandchild_owner(new QoreClass("IdentityGrandchild", "::IdentityGrandchild"));
    QoreClass* grandchild = grandchild_owner;
    grandchild->addBuiltinBaseClass(child);
    bool private_access = true;
    assert(grandchild->getClass(*second, private_access) == base);
    assert(!private_access);
}

int main() {
    qore_init(QL_MIT, "UTF-8", true, QLO_DISABLE_SIGNAL_HANDLING);
    checkClassHandles();
    qore_cleanup();
    std::puts("native-class-identity: ok");
}
