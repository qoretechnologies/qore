/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file ancestryprobe-module.cpp test binary module that creates builtin classes at run time in the order of qore#7614

    Test scaffolding for the completion of the ancestry of builtin subclasses (qore#7614): the module
    \c ancestryprobe is built into the build tree but never installed.  Like the jni module, which creates the Qore
    classes of Java classes lazily, its functions create builtin classes at run time with the C++ API in an order where
    a subclass is created while its parent class is still getting base classes: class \c A is created; while its base
    classes are added, \c B is created with \c A as a parent class, initialized and registered; then \c A gets more
    base classes and is initialized and registered.  Each function returns a hash of the results of its checks, which
    the test compares with the expected values:
    - <tt>AncestryProbe::gap(bool virt)</tt>: the ancestry of \c B and of \c C (created with \c B as a parent class in
      the gap), and an object of \c B: \c instanceof, the private data of a later ancestor and of its own parent class
      (virtual inheritance: set by \c B and shared with its virtual ancestors), a method of a later ancestor and the
      destructor of a later ancestor (non-virtual inheritance: set by the later ancestor's constructor)
    - <tt>AncestryProbe::deletedSubclass()</tt>: \c B and \c C are deleted before \c A gets its later base classes
    - <tt>AncestryProbe::neverClosed()</tt>: \c A is never initialized; \c B keeps the ancestors that \c A had
    - <tt>AncestryProbe::concurrentReaders(int n)</tt>: \c A gets \a n later base classes while another thread reads
      the ancestors of \c B

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include <qore/Qore.h>

#include <atomic>
#include <cassert>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {
//! the namespace of the module
QoreNamespace* module_ns = nullptr;
//! the sequence of the names of the classes created
std::atomic<int64> class_seq{0};
//! the number of times the destructor of a later ancestor has run
std::atomic<int64> late_destructor_count{0};

//! the value that the private data of a later ancestor holds
constexpr int64 LateValue = 42;

//! the private data of the classes created as later ancestors
class LateData : public AbstractPrivateData {
public:
    int64 value = LateValue;
};

void late_constructor(const QoreMethod& method, const void*, QoreObject* self, const QoreListNode*, RuntimeConfig&,
        ExceptionSink*) {
    self->setPrivate(method.getClass()->getID(), new LateData);
}

void late_destructor(QoreObject*, AbstractPrivateData* data, RuntimeConfig&, ExceptionSink* xsink) {
    ++late_destructor_count;
    data->deref(xsink);
}

QoreValue late_value(QoreObject*, AbstractPrivateData* data, const QoreListNode*, RuntimeConfig&, ExceptionSink*) {
    return static_cast<LateData*>(data)->value;
}

//! the classes of a scenario, which are deleted with its namespace
/** The namespace has no root namespace: the classes registered in it are finalized as a module's classes are
    (QoreNamespace::addSystemClass()), and are reachable only from the scenario
*/
class Scenario {
public:
    Scenario() : ns(new QoreNamespace("AncestryProbeScenario")) {
    }

    ~Scenario() {
        delete ns;
    }

    //! returns a new builtin class whose base classes are still to be added
    QoreClass* create(const char* prefix) {
        std::string name = prefix + std::to_string(++class_seq);
        std::string path = "AncestryProbeScenario::" + name;
        return new QoreBuiltinClass(name.c_str(), path.c_str());
    }

    //! returns a new class that is a later ancestor: it sets private data in its constructor, frees it in its
    //! destructor and has a method that returns a value of the private data; registered when returned
    QoreClass* createLate(QoreClass* parent) {
        QoreClass* qc = create("Late");
        if (parent) {
            qc->addBaseClass(parent, true);
        }
        qc->addConstructor(nullptr, late_constructor);
        qc->setDestructor(late_destructor);
        qc->addMethod("lateValue", late_value, Public, QCF_CONSTANT, QDOM_DEFAULT, bigIntTypeInfo);
        add(qc);
        return qc;
    }

    //! registers the class, which finalizes it
    void add(QoreClass* qc) {
        ns->addSystemClass(qc);
    }

private:
    QoreNamespace* ns;
};

//! returns true if \a qc has \a ancestor among its ancestors
bool has_ancestor(const QoreClass* qc, const QoreClass* ancestor) {
    return qc->getClass(ancestor->getID()) != nullptr;
}

//! AncestryProbe::gap(bool virt): the ancestry of a subclass created while its parent class gets base classes
QoreValue f_gap(const QoreListNode* args, RuntimeConfig&, ExceptionSink* xsink) {
    bool virt = HARD_QORE_VALUE_BOOL(args, 0);
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    Scenario s;

    // the ancestors that A has when B inherits it, and those that A gets afterwards
    QoreClass* early = s.create("Early");
    s.add(early);
    QoreClass* late_parent = s.createLate(nullptr);
    QoreClass* late = s.createLate(late_parent);
    QoreClass* plain = s.create("Plain");
    s.add(plain);

    // A is created and gets its first base class
    QoreClass* a = s.create("A");
    a->addBaseClass(early, virt);

    // while A's base classes are added, B is created with A as a parent class, initialized and registered
    QoreClass* b = s.create("B");
    b->addBaseClass(a, virt);
    if (virt) {
        // the constructors of virtual base classes are not run: B sets the private data, which its virtual ancestors
        // share (qore_object_private::addVirtualPrivateData() with the ancestors in B's list)
        b->addConstructor(nullptr, late_constructor);
        b->setDestructor(late_destructor);
    }
    b->initializeBuiltin();
    s.add(b);
    // C is created with B as a parent class in the gap
    QoreClass* c = s.create("C");
    c->addBaseClass(b, virt);
    c->initializeBuiltin();
    s.add(c);
    rv->setKeyValue("b_late_before", has_ancestor(b, late), nullptr);

    // A gets its later base classes and is initialized and registered
    a->addBaseClass(late, virt);
    a->addBaseClass(plain, false);
    a->initializeBuiltin();
    s.add(a);

    rv->setKeyValue("b_early", has_ancestor(b, early), nullptr);
    rv->setKeyValue("b_late", has_ancestor(b, late), nullptr);
    rv->setKeyValue("b_late_parent", has_ancestor(b, late_parent), nullptr);
    rv->setKeyValue("b_plain", has_ancestor(b, plain), nullptr);
    rv->setKeyValue("c_late", has_ancestor(c, late), nullptr);
    rv->setKeyValue("c_late_parent", has_ancestor(c, late_parent), nullptr);
    rv->setKeyValue("c_plain", has_ancestor(c, plain), nullptr);
    rv->setKeyValue("c_a", has_ancestor(c, a), nullptr);

    // the hierarchy iterator visits each later ancestor
    {
        int64 late_seen = 0;
        QoreClassHierarchyIterator i(*b);
        while (i.next()) {
            const QoreClass& qc = i.get();
            if (&qc == late || &qc == late_parent || &qc == plain) {
                ++late_seen;
            }
        }
        rv->setKeyValue("b_iterated_late", late_seen, nullptr);
    }

    // an object of B: with non-virtual inheritance, its constructor runs the constructor of the later ancestor, which
    // sets private data that needs the later ancestor in the ancestry of B (qore_object_private::
    // addVirtualPrivateData()); without it, a debug build fails an assertion, so the object is only created if the
    // ancestry is complete; with virtual inheritance, B's constructor sets the private data, which the later ancestors
    // share if they are in the ancestry of B
    if (has_ancestor(b, late) && (!virt || has_ancestor(b, late_parent))) {
        int64 dcount = late_destructor_count.load();
        {
            ReferenceHolder<QoreObject> obj(b->execConstructor(nullptr, xsink), xsink);
            if (*xsink) {
                return QoreValue();
            }
            rv->setKeyValue("obj_instanceof_late", obj->validInstanceOf(*late), nullptr);
            rv->setKeyValue("obj_instanceof_plain", obj->validInstanceOf(*plain), nullptr);
            {
                AbstractPrivateData* pd = obj->getReferencedPrivateData(late->getID(), xsink);
                rv->setKeyValue("obj_late_data", pd ? static_cast<LateData*>(pd)->value : -1, nullptr);
                if (pd) {
                    pd->deref(xsink);
                }
            }
            if (virt) {
                AbstractPrivateData* pd = obj->getReferencedPrivateDataIfExists(late_parent->getID());
                rv->setKeyValue("obj_late_parent_data", pd ? static_cast<LateData*>(pd)->value : -1, nullptr);
                if (pd) {
                    pd->deref(xsink);
                }
            }
            ValueHolder v(obj->evalMethod("lateValue", nullptr, xsink), xsink);
            if (*xsink) {
                return QoreValue();
            }
            rv->setKeyValue("obj_late_value", v->getAsBigInt(), nullptr);
            obj->doDelete(xsink);
        }
        rv->setKeyValue("late_destructors", late_destructor_count.load() - dcount, nullptr);
    }
    return rv.release();
}

//! AncestryProbe::deletedSubclass(): subclasses deleted before their parent class gets its later base classes
QoreValue f_deleted_subclass(const QoreListNode*, RuntimeConfig&, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    Scenario s;
    QoreClass* early = s.create("Early");
    s.add(early);
    QoreClass* late = s.createLate(nullptr);

    QoreClass* a = s.create("A");
    a->addBaseClass(early, true);
    {
        // B and C are created as the jni module creates a class that it then fails to complete: they are deleted
        // before A gets its later base classes
        QoreClassHolder b(s.create("B"));
        static_cast<QoreClass*>(b)->addBaseClass(a, true);
        static_cast<QoreClass*>(b)->initializeBuiltin();
        QoreClassHolder c(s.create("C"));
        static_cast<QoreClass*>(c)->addBaseClass(b, true);
        static_cast<QoreClass*>(c)->initializeBuiltin();
        // C is deleted first, then B
    }
    {
        // D is deleted with A open, after B's sibling E is registered with A
        QoreClassHolder d(s.create("D"));
        static_cast<QoreClass*>(d)->addBaseClass(a, true);
    }
    QoreClass* e = s.create("E");
    e->addBaseClass(a, true);
    e->initializeBuiltin();
    s.add(e);

    a->addBaseClass(late, true);
    a->initializeBuiltin();
    s.add(a);
    rv->setKeyValue("a_late", has_ancestor(a, late), nullptr);
    rv->setKeyValue("e_late", has_ancestor(e, late), nullptr);
    return rv.release();
}

//! AncestryProbe::neverClosed(): a parent class that is never initialized
QoreValue f_never_closed(const QoreListNode*, RuntimeConfig&, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    Scenario s;
    QoreClass* early = s.create("Early");
    s.add(early);
    QoreClass* late = s.createLate(nullptr);

    QoreClassHolder a(s.create("A"));
    static_cast<QoreClass*>(a)->addBaseClass(early, true);
    QoreClass* b = s.create("B");
    b->addBaseClass(a, true);
    b->initializeBuiltin();
    s.add(b);
    QoreClass* c = s.create("C");
    c->addBaseClass(b, true);
    c->initializeBuiltin();
    s.add(c);
    // A gets a later base class but is never initialized nor registered: its ancestry is never final, so B and C
    // keep the ancestors that A had
    static_cast<QoreClass*>(a)->addBaseClass(late, true);
    rv->setKeyValue("b_early", has_ancestor(b, early), nullptr);
    rv->setKeyValue("b_late", has_ancestor(b, late), nullptr);
    rv->setKeyValue("c_early", has_ancestor(c, early), nullptr);
    rv->setKeyValue("c_late", has_ancestor(c, late), nullptr);
    // A is deleted before B and C, which keep references to its private data
    return rv.release();
}

//! AncestryProbe::concurrentReaders(int n): later ancestors are added while another thread reads the ancestors
QoreValue f_concurrent_readers(const QoreListNode* args, RuntimeConfig&, ExceptionSink* xsink) {
    int64 n = HARD_QORE_VALUE_INT(args, 0);
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    Scenario s;
    std::vector<QoreClass*> lates;
    lates.reserve(n);
    for (int64 i = 0; i < n; ++i) {
        QoreClass* qc = s.create("L");
        s.add(qc);
        lates.push_back(qc);
    }
    QoreClass* a = s.create("A");
    QoreClass* b = s.create("B");
    b->addBaseClass(a, true);
    b->initializeBuiltin();
    s.add(b);

    // the reader iterates the ancestors of B until the writer is done, and once more after it
    std::atomic<bool> done{false};
    std::atomic<int64> reads{0};
    int64 last_seen = 0;
    std::thread reader([&]() {
        bool last = false;
        while (true) {
            last = done.load(std::memory_order_acquire);
            int64 seen = 0;
            QoreClassHierarchyIterator i(*b);
            while (i.next()) {
                ++seen;
            }
            // the ancestors are found by ID while they are appended
            for (QoreClass* qc : lates) {
                if (b->getClass(qc->getID())) {
                    ++seen;
                }
            }
            ++reads;
            last_seen = seen;
            if (last) {
                break;
            }
        }
    });
    // the writer: A gets its base classes after B inherited it, and is initialized, which adds them to B
    for (QoreClass* qc : lates) {
        a->addBaseClass(qc, true);
    }
    a->initializeBuiltin();
    s.add(a);
    done.store(true, std::memory_order_release);
    reader.join();

    int64 found = 0;
    for (QoreClass* qc : lates) {
        if (has_ancestor(b, qc)) {
            ++found;
        }
    }
    rv->setKeyValue("found", found, nullptr);
    // the last read after the writer was done: the hierarchy iterator visits B and its ancestors (A and the later
    // ones), then each later one is found by its ID
    rv->setKeyValue("last_seen", last_seen, nullptr);
    rv->setKeyValue("reads_positive", reads.load() > 0, nullptr);
    return rv.release();
}
}

static void ancestryprobe_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    module_ns = new QoreNamespace("AncestryProbe");
    module_ns->addBuiltinVariant("gap", f_gap, QCF_NO_FLAGS, QDOM_DEFAULT, autoHashTypeInfo, 1, boolTypeInfo,
        QORE_PARAM_NO_ARG, "virt");
    module_ns->addBuiltinVariant("deletedSubclass", f_deleted_subclass, QCF_NO_FLAGS, QDOM_DEFAULT, autoHashTypeInfo);
    module_ns->addBuiltinVariant("neverClosed", f_never_closed, QCF_NO_FLAGS, QDOM_DEFAULT, autoHashTypeInfo);
    module_ns->addBuiltinVariant("concurrentReaders", f_concurrent_readers, QCF_NO_FLAGS, QDOM_DEFAULT,
        autoHashTypeInfo, 1, bigIntTypeInfo, QORE_PARAM_NO_ARG, "n");
}

static void ancestryprobe_module_ns_init(QoreNamespace* rns, QoreNamespace* qns, ExceptionSink& xsink) {
    qns->addNamespace(module_ns->copy());
}

static void ancestryprobe_module_delete() {
    delete module_ns;
    module_ns = nullptr;
}

extern "C" DLLEXPORT void ancestryprobe_qore_module_desc(QoreModuleInfo& mod_info) {
    mod_info.name = "ancestryprobe";
    mod_info.version = PACKAGE_VERSION;
    mod_info.desc = "test module that creates builtin classes at run time while their parent classes get base classes";
    mod_info.author = "Qore Technologies, s.r.o.";
    mod_info.url = "https://qore.org";
    mod_info.api_major = QORE_MODULE_API_MAJOR;
    mod_info.api_minor = QORE_MODULE_API_MINOR;
    mod_info.init = ancestryprobe_module_init;
    mod_info.ns_init = ancestryprobe_module_ns_init;
    mod_info.del = ancestryprobe_module_delete;
    mod_info.license = QL_MIT;
}
