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
    - <tt>AncestryProbe::order(bool gap)</tt>: the order of the ancestors of \c B (non-virtual inheritance), and the
      order in which the destructors and copy methods of the ancestors run for an object of \c B; with \c gap false,
      \c B is created after \c A is complete (the reference)
    - <tt>AncestryProbe::twoOpenParents()</tt>: \c B inherits two classes that are both still getting base classes
    - <tt>AncestryProbe::rescan()</tt>: QoreClass::rescanParents() on \c B before and after \c A is complete
    - <tt>AncestryProbe::deletedSubclass()</tt>: \c B and \c C are deleted before \c A gets its later base classes
    - <tt>AncestryProbe::neverClosed()</tt>: \c A is never initialized and its reference is released; \c B keeps the
      ancestors that \c A had and stays registered with it, not final (debug builds:
      \c qore_dbg_class_ancestry_final()), and a subclass of \c B made then is not final either
    - <tt>AncestryProbe::rescanReaders(int n)</tt>: QoreClass::rescanParents() grows the list of a closed class while
      another thread reads it (with the test hook, as for concurrentReaders())
    - <tt>AncestryProbe::concurrentReaders(int n)</tt>: \c A gets \a n later base classes while another thread reads
      the ancestors of \c B; in a debug build, the reader takes a snapshot of the ancestors before \c A is complete and
      reads it, and the new list, while the completion holds the new list published (the libqore test hook
      \c qore_dbg_set_ancestry_publish_hook(), looked up with \c dlsym())
    - <tt>AncestryProbe::gapProgram(code cb)</tt>: the classes are created in a namespace of the calling Program;
      \c cb is called in the gap with the path of \c B (to import \c B into another Program and to parse a subclass
      of it there); <tt>AncestryProbe::checkObject(object o)</tt> checks an object of such a class afterwards

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

#include <dlfcn.h>

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
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

//! the log of the destructors and copy methods of the classes of order(), by the roles of the classes
std::mutex order_lock;
std::vector<std::string> order_log;
//! the roles of the classes of order() and twoOpenParents(), by class ID
std::map<qore_classid_t, std::string> roles;

void order_log_add(const std::string& entry) {
    std::lock_guard<std::mutex> al(order_lock);
    order_log.push_back(entry);
}

void order_constructor(const QoreMethod& method, const void*, QoreObject* self, const QoreListNode*, RuntimeConfig&,
        ExceptionSink*) {
    self->setPrivate(method.getClass()->getID(), new LateData);
}

void order_destructor(const QoreClass&, const void* ptr, QoreObject*, AbstractPrivateData* data, RuntimeConfig&,
        ExceptionSink* xsink) {
    order_log_add(std::string("destructor ") + static_cast<const char*>(ptr));
    data->deref(xsink);
}

void order_copy(const QoreClass& thisclass, const void* ptr, QoreObject* self, QoreObject*, AbstractPrivateData*,
        RuntimeConfig&, ExceptionSink*) {
    order_log_add(std::string("copy ") + static_cast<const char*>(ptr));
    self->setPrivate(thisclass.getID(), new LateData);
}

//! returns the roles of the class and of its ancestors in the order of the class's list of ancestors
QoreListNode* roles_of(const QoreClass& qc) {
    ReferenceHolder<QoreListNode> l(new QoreListNode(stringTypeInfo), nullptr);
    QoreClassHierarchyIterator i(qc);
    std::lock_guard<std::mutex> al(order_lock);
    while (i.next()) {
        auto ri = roles.find(i.get().getID());
        l->push(new QoreStringNode(ri == roles.end() ? std::string("?") : ri->second), nullptr);
    }
    return l.release();
}

//! the classes of a scenario, which are deleted with its namespace
/** The namespace has no root namespace: the classes registered in it are finalized as a module's classes are
    (QoreNamespace::addSystemClass()), and are reachable only from the scenario; with a namespace of the calling
    Program, the classes are registered in it and live with it
*/
class Scenario {
public:
    Scenario() : ns(new QoreNamespace("AncestryProbeScenario")), owned(true) {
    }

    explicit Scenario(QoreNamespace* pgm_ns) : ns(pgm_ns), owned(false) {
    }

    ~Scenario() {
        if (owned) {
            delete ns;
        }
    }

    Scenario(const Scenario&) = delete;
    Scenario& operator=(const Scenario&) = delete;

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

    //! returns a new class for order(), whose destructor and copy method log the role \a role; with \a parent as its
    //! non-virtual parent class if not nullptr; registered when returned unless \a open
    QoreClass* createOrdered(const char* role, QoreClass* parent, bool open = false) {
        QoreClass* qc = create("O");
        {
            std::lock_guard<std::mutex> al(order_lock);
            roles[qc->getID()] = role;
        }
        if (parent) {
            qc->addBaseClass(parent, false);
        }
        qc->addConstructor(nullptr, order_constructor);
        qc->setDestructor(role, order_destructor);
        qc->setCopy(role, order_copy);
        if (!open) {
            add(qc);
        }
        return qc;
    }

    //! registers the class, which finalizes it
    void add(QoreClass* qc) {
        ns->addSystemClass(qc);
    }

    //! returns the path of a class registered in the namespace
    std::string path(const QoreClass* qc) const {
        return std::string(ns->getName()) + "::" + qc->getName();
    }

private:
    QoreNamespace* ns;
    bool owned;
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

//! AncestryProbe::order(bool gap): the order of the ancestors and of their destructors and copy methods
/** Non-virtual inheritance: B inherits A; A inherits E1 (which inherits E0), then L1 (which inherits L0) and L2; with
    \a gap, B is created after A gets E1 and before it gets L1 and L2.  A build from scratch has the list
    E0, E1, L0, L1, L2, A: the destructors run in the reverse order, the copy methods in this order.
*/
QoreValue f_order(const QoreListNode* args, RuntimeConfig&, ExceptionSink* xsink) {
    bool gap = HARD_QORE_VALUE_BOOL(args, 0);
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    Scenario s;
    QoreClass* e0 = s.createOrdered("E0", nullptr);
    QoreClass* e1 = s.createOrdered("E1", e0);
    QoreClass* l0 = s.createOrdered("L0", nullptr);
    QoreClass* l1 = s.createOrdered("L1", l0);
    QoreClass* l2 = s.createOrdered("L2", nullptr);
    QoreClass* a = s.createOrdered("A", nullptr, true);
    a->addBaseClass(e1, false);
    QoreClass* b = nullptr;
    auto make_b = [&]() {
        b = s.create("B");
        {
            std::lock_guard<std::mutex> al(order_lock);
            roles[b->getID()] = "B";
        }
        b->addBaseClass(a, false);
        b->initializeBuiltin();
        s.add(b);
    };
    if (gap) {
        make_b();
    }
    a->addBaseClass(l1, false);
    a->addBaseClass(l2, false);
    a->initializeBuiltin();
    s.add(a);
    if (!gap) {
        make_b();
    }
    rv->setKeyValue("ancestors", roles_of(*b), nullptr);
    {
        std::lock_guard<std::mutex> al(order_lock);
        order_log.clear();
    }
    {
        ReferenceHolder<QoreObject> obj(b->execConstructor(nullptr, xsink), xsink);
        if (*xsink) {
            return QoreValue();
        }
        ValueHolder copy(obj->evalMethod("copy", nullptr, xsink), xsink);
        if (*xsink) {
            return QoreValue();
        }
        obj->doDelete(xsink);
        if (copy->getType() == NT_OBJECT) {
            copy->get<QoreObject>()->doDelete(xsink);
        }
    }
    ReferenceHolder<QoreListNode> dlog(new QoreListNode(stringTypeInfo), xsink);
    ReferenceHolder<QoreListNode> clog(new QoreListNode(stringTypeInfo), xsink);
    {
        std::lock_guard<std::mutex> al(order_lock);
        // the destructors of the original object, which is deleted first
        bool first_destructors = true;
        for (const std::string& e : order_log) {
            if (!e.compare(0, 5, "copy ")) {
                clog->push(new QoreStringNode(e.substr(5)), nullptr);
            } else if (first_destructors) {
                dlog->push(new QoreStringNode(e.substr(11)), nullptr);
                if (dlog->size() == 6) {
                    first_destructors = false;
                }
            }
        }
    }
    rv->setKeyValue("destructors", dlog.release(), nullptr);
    rv->setKeyValue("copies", clog.release(), nullptr);
    return rv.release();
}

//! AncestryProbe::twoOpenParents(): a class that inherits two classes that are still getting base classes
QoreValue f_two_open_parents(const QoreListNode*, RuntimeConfig&, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    Scenario s;
    QoreClass* l1 = s.createOrdered("L1", nullptr);
    QoreClass* l2 = s.createOrdered("L2", nullptr);
    QoreClass* a1 = s.createOrdered("A1", nullptr, true);
    QoreClass* a2 = s.createOrdered("A2", nullptr, true);
    QoreClass* b = s.create("B");
    {
        std::lock_guard<std::mutex> al(order_lock);
        roles[b->getID()] = "B";
    }
    b->addBaseClass(a1, true);
    b->addBaseClass(a2, true);
    b->initializeBuiltin();
    s.add(b);
    a1->addBaseClass(l1, true);
    a1->initializeBuiltin();
    s.add(a1);
    // B is still registered with A2: it has what A1 got, but not what A2 gets
    rv->setKeyValue("after_a1", roles_of(*b), nullptr);
    a2->addBaseClass(l2, true);
    a2->initializeBuiltin();
    s.add(a2);
    rv->setKeyValue("after_a2", roles_of(*b), nullptr);
    return rv.release();
}

//! AncestryProbe::rescan(): QoreClass::rescanParents() on a class whose parent class gets base classes, and after
QoreValue f_rescan(const QoreListNode*, RuntimeConfig&, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    Scenario s;
    QoreClass* early = s.create("Early");
    s.add(early);
    QoreClass* late = s.createLate(nullptr);
    QoreClass* a = s.create("A");
    a->addBaseClass(early, true);
    QoreClass* b = s.create("B");
    b->addBaseClass(a, true);
    b->initializeBuiltin();
    s.add(b);
    // B is registered with A: the rescan holds the lock of the registry of ancestries
    b->rescanParents();
    rv->setKeyValue("b_early", has_ancestor(b, early), nullptr);
    a->addBaseClass(late, true);
    a->initializeBuiltin();
    s.add(a);
    b->rescanParents();
    rv->setKeyValue("b_late", has_ancestor(b, late), nullptr);
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
        static_cast<QoreClass*>(d)->initializeBuiltin();
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

typedef bool (*final_query_t)(const QoreClass*);

//! returns the libqore function that tells if the ancestry of a class is final (debug builds only), or nullptr
final_query_t get_final_query() {
    return reinterpret_cast<final_query_t>(dlsym(RTLD_DEFAULT, "qore_dbg_class_ancestry_final"));
}

//! AncestryProbe::neverClosed(): a parent class that is never initialized
QoreValue f_never_closed(const QoreListNode*, RuntimeConfig&, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    Scenario s;
    QoreClass* early = s.create("Early");
    s.add(early);
    QoreClass* late = s.createLate(nullptr);

    QoreClass* b;
    QoreClass* c;
    {
        QoreClassHolder a(s.create("A"));
        static_cast<QoreClass*>(a)->addBaseClass(early, true);
        b = s.create("B");
        b->addBaseClass(a, true);
        b->initializeBuiltin();
        s.add(b);
        c = s.create("C");
        c->addBaseClass(b, true);
        c->initializeBuiltin();
        s.add(c);
        // A gets a later base class but is never initialized nor registered: its ancestry is never final, so B and C
        // keep the ancestors that A had
        static_cast<QoreClass*>(a)->addBaseClass(late, true);
        // the reference to A is released here; B holds a reference to its private data in its list of ancestors, so
        // A is not destroyed, and B stays registered with it
    }
    rv->setKeyValue("b_early", has_ancestor(b, early), nullptr);
    rv->setKeyValue("b_late", has_ancestor(b, late), nullptr);
    rv->setKeyValue("c_early", has_ancestor(c, early), nullptr);
    rv->setKeyValue("c_late", has_ancestor(c, late), nullptr);
    // a subclass of B made now is registered with B, which is not final; it walks the hierarchy of B through the
    // current base classes of A, as the list of B is not complete, so it gets the later base class of A
    QoreClass* d = s.create("D");
    d->addBaseClass(b, true);
    d->initializeBuiltin();
    s.add(d);
    rv->setKeyValue("d_early", has_ancestor(d, early), nullptr);
    rv->setKeyValue("d_late", has_ancestor(d, late), nullptr);
    if (final_query_t is_final = get_final_query()) {
        rv->setKeyValue("b_final", is_final(b), nullptr);
        rv->setKeyValue("c_final", is_final(c), nullptr);
        rv->setKeyValue("d_final", is_final(d), nullptr);
        rv->setKeyValue("early_final", is_final(early), nullptr);
    }
    // A is destroyed with its private data, after B, C and D are destroyed with the namespace of the scenario
    return rv.release();
}

//! the states of the handshake of concurrentReaders()
enum class ReaderState {
    Start,
    HaveView,
    Published,
    ReaderDone,
    Abort,
};

//! the handshake of concurrentReaders() with the test hook of libqore
struct ReaderHandshake {
    std::mutex m;
    std::condition_variable cv;
    ReaderState state = ReaderState::Start;
    const QoreClass* target = nullptr;

    void set(ReaderState s) {
        std::lock_guard<std::mutex> al(m);
        // a handshake that was aborted stays aborted
        if (state != ReaderState::Abort) {
            state = s;
        }
        cv.notify_all();
    }

    //! waits for the state \a s; returns false if the handshake was aborted
    bool wait(ReaderState s) {
        std::unique_lock<std::mutex> al(m);
        cv.wait(al, [&] { return state == s || state == ReaderState::Abort; });
        return state != ReaderState::Abort;
    }
};

ReaderHandshake* reader_handshake = nullptr;

//! the test hook: called with the list of ancestors of a subclass published and the registry locked
void ancestry_publish_hook(const QoreClass* qc) {
    ReaderHandshake* h = reader_handshake;
    if (!h || qc != h->target) {
        return;
    }
    h->set(ReaderState::Published);
    h->wait(ReaderState::ReaderDone);
}

typedef void (*hook_setter_t)(void (*)(const QoreClass*));

//! returns the libqore function that sets the test hook (debug builds only), or nullptr
hook_setter_t get_hook_setter() {
    return reinterpret_cast<hook_setter_t>(dlsym(RTLD_DEFAULT, "qore_dbg_set_ancestry_publish_hook"));
}

//! sets the test hook and clears it, and ends the reader thread, also if an exception is thrown
class ReaderGuard {
public:
    ReaderGuard(hook_setter_t setter, ReaderHandshake& h) : setter(setter), h(h) {
        reader_handshake = &h;
        if (setter) {
            setter(ancestry_publish_hook);
        }
    }

    ~ReaderGuard() {
        // the reader is not left waiting for a state that will not come
        h.set(ReaderState::Abort);
        if (reader.joinable()) {
            reader.join();
        }
        if (setter) {
            setter(nullptr);
        }
        reader_handshake = nullptr;
    }

    ReaderGuard(const ReaderGuard&) = delete;
    ReaderGuard& operator=(const ReaderGuard&) = delete;

    std::thread reader;

private:
    hook_setter_t setter;
    ReaderHandshake& h;
};

//! the results of a reader thread of read_while()
struct ReaderResult {
    //! the number of classes of the snapshot taken before the change, read after it
    int64 old_seen = -1;
    //! the number of classes of a snapshot taken after the change
    int64 new_seen = -1;
    //! the number of the classes of \a lates found by ID after the change
    int64 found = -1;
};

//! runs \a change while a reader thread holds a snapshot of the ancestors of \a target taken before it; when the
//! test hook of libqore is called for \a target (in a debug build), the reader reads that snapshot, a new one and the
//! classes of \a lates by ID while the hook waits; returns true if the hook was available
bool read_while(const QoreClass* target, const std::vector<QoreClass*>& lates, ReaderResult& r,
        const std::function<void()>& change) {
    hook_setter_t setter = get_hook_setter();
    ReaderHandshake h;
    h.target = target;
    ReaderGuard g(setter, h);
    g.reader = std::thread([&]() {
        // a snapshot of the ancestors of the target before the change
        QoreClassHierarchyIterator old_view(*target);
        if (!setter) {
            return;
        }
        h.set(ReaderState::HaveView);
        if (!h.wait(ReaderState::Published)) {
            return;
        }
        // the change is published and waits in the hook: the old snapshot is still readable, and a new one has the
        // classes added
        int64 seen = 0;
        while (old_view.next()) {
            ++seen;
        }
        r.old_seen = seen;
        seen = 0;
        QoreClassHierarchyIterator new_view(*target);
        while (new_view.next()) {
            ++seen;
        }
        r.new_seen = seen;
        int64 found = 0;
        for (QoreClass* qc : lates) {
            if (target->getClass(qc->getID())) {
                ++found;
            }
        }
        r.found = found;
        h.set(ReaderState::ReaderDone);
    });
    if (setter && !h.wait(ReaderState::HaveView)) {
        return false;
    }
    change();
    return setter != nullptr;
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

    ReaderResult r;
    // the writer: A gets its base classes after B inherited it, and is initialized, which completes B
    bool hook = read_while(b, lates, r, [&]() {
        for (QoreClass* qc : lates) {
            a->addBaseClass(qc, true);
        }
        a->initializeBuiltin();
        s.add(a);
    });

    int64 found = 0;
    for (QoreClass* qc : lates) {
        if (has_ancestor(b, qc)) {
            ++found;
        }
    }
    rv->setKeyValue("found", found, nullptr);
    rv->setKeyValue("hook", hook, nullptr);
    rv->setKeyValue("old_seen", r.old_seen, nullptr);
    rv->setKeyValue("new_seen", r.new_seen, nullptr);
    rv->setKeyValue("found_in_hook", r.found, nullptr);
    return rv.release();
}

//! AncestryProbe::rescanReaders(int n): QoreClass::rescanParents() appends to the list of a closed class, growing it,
//! while another thread reads it
/** B is closed and registered; A, its virtual parent class, is never closed and gets \a n base classes afterwards,
    which B does not get, as A never completes; rescanning the parent classes of B adds them to its list
*/
QoreValue f_rescan_readers(const QoreListNode* args, RuntimeConfig&, ExceptionSink* xsink) {
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
    QoreClass* early = s.create("Early");
    s.add(early);
    QoreClassHolder a(s.create("A"));
    static_cast<QoreClass*>(a)->addBaseClass(early, true);
    QoreClass* b = s.create("B");
    b->addBaseClass(a, true);
    b->initializeBuiltin();
    s.add(b);
    for (QoreClass* qc : lates) {
        static_cast<QoreClass*>(a)->addBaseClass(qc, true);
    }
    rv->setKeyValue("before", (int64)(has_ancestor(b, lates[0]) ? 1 : 0), nullptr);

    ReaderResult r;
    bool hook = read_while(b, lates, r, [&]() {
        b->rescanParents();
    });

    int64 found = 0;
    for (QoreClass* qc : lates) {
        if (has_ancestor(b, qc)) {
            ++found;
        }
    }
    rv->setKeyValue("found", found, nullptr);
    rv->setKeyValue("hook", hook, nullptr);
    rv->setKeyValue("old_seen", r.old_seen, nullptr);
    rv->setKeyValue("new_seen", r.new_seen, nullptr);
    rv->setKeyValue("found_in_hook", r.found, nullptr);
    return rv.release();
}

//! the classes of the last gapProgram() call, for checkObject()
const QoreClass* gap_late = nullptr;
const QoreClass* gap_late_parent = nullptr;

//! AncestryProbe::gapProgram(code cb): the classes are created in a namespace of the calling Program, and \a cb is
//! called in the gap with the path of B
QoreValue f_gap_program(const QoreListNode* args, RuntimeConfig&, ExceptionSink* xsink) {
    HARD_QORE_VALUE_PARAM(cb, const ResolvedCallReferenceNode, args, 0);
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    QoreNamespace* ns = getProgram()->getRootNS()->findCreateNamespacePath("AncestryProbeGap");
    Scenario s(ns);
    QoreClass* early = s.create("Early");
    s.add(early);
    QoreClass* late_parent = s.createLate(nullptr);
    QoreClass* late = s.createLate(late_parent);
    QoreClass* a = s.create("A");
    a->addBaseClass(early, true);
    QoreClass* b = s.create("B");
    b->addBaseClass(a, true);
    b->addConstructor(nullptr, late_constructor);
    b->setDestructor(late_destructor);
    b->initializeBuiltin();
    s.add(b);
    gap_late = late;
    gap_late_parent = late_parent;
    {
        ReferenceHolder<QoreListNode> cb_args(new QoreListNode(autoTypeInfo), xsink);
        cb_args->push(new QoreStringNode(s.path(b)), xsink);
        ValueHolder v(cb->invoke(*cb_args, xsink), xsink);
    }
    // A is completed also if the callback raised an exception
    a->addBaseClass(late, true);
    a->initializeBuiltin();
    s.add(a);
    if (*xsink) {
        return QoreValue();
    }
    rv->setKeyValue("b_late", has_ancestor(b, late), nullptr);
    return rv.release();
}

//! AncestryProbe::checkObject(object o): the ancestry of the class of an object of a class of gapProgram()
QoreValue f_check_object(const QoreListNode* args, RuntimeConfig&, ExceptionSink* xsink) {
    QoreObject* obj = HARD_QORE_VALUE_OBJECT(args, 0);
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    if (!gap_late) {
        xsink->raiseException("ANCESTRY-PROBE-ERROR", "gapProgram() has not been called");
        return QoreValue();
    }
    const QoreClass* cls = obj->getClass();
    rv->setKeyValue("class_late", cls->getClass(gap_late->getID()) != nullptr, nullptr);
    rv->setKeyValue("class_late_parent", cls->getClass(gap_late_parent->getID()) != nullptr, nullptr);
    rv->setKeyValue("instanceof_late", obj->validInstanceOf(gap_late->getID()), nullptr);
    {
        AbstractPrivateData* pd = obj->getReferencedPrivateDataIfExists(gap_late->getID());
        rv->setKeyValue("late_data", pd ? static_cast<LateData*>(pd)->value : -1, nullptr);
        if (pd) {
            pd->deref(xsink);
        }
    }
    {
        AbstractPrivateData* pd = obj->getReferencedPrivateDataIfExists(gap_late_parent->getID());
        rv->setKeyValue("late_parent_data", pd ? static_cast<LateData*>(pd)->value : -1, nullptr);
        if (pd) {
            pd->deref(xsink);
        }
    }
    if (cls->getClass(gap_late->getID())) {
        ValueHolder v(obj->evalMethod("lateValue", nullptr, xsink), xsink);
        if (*xsink) {
            return QoreValue();
        }
        rv->setKeyValue("late_value", v->getAsBigInt(), nullptr);
    }
    return rv.release();
}
}

static void ancestryprobe_module_init(QoreModuleInitContext& ctx, ExceptionSink& xsink) {
    module_ns = new QoreNamespace("AncestryProbe");
    module_ns->addBuiltinVariant("gap", f_gap, QCF_NO_FLAGS, QDOM_DEFAULT, autoHashTypeInfo, 1, boolTypeInfo,
        QORE_PARAM_NO_ARG, "virt");
    module_ns->addBuiltinVariant("order", f_order, QCF_NO_FLAGS, QDOM_DEFAULT, autoHashTypeInfo, 1, boolTypeInfo,
        QORE_PARAM_NO_ARG, "gap");
    module_ns->addBuiltinVariant("twoOpenParents", f_two_open_parents, QCF_NO_FLAGS, QDOM_DEFAULT, autoHashTypeInfo);
    module_ns->addBuiltinVariant("rescan", f_rescan, QCF_NO_FLAGS, QDOM_DEFAULT, autoHashTypeInfo);
    module_ns->addBuiltinVariant("deletedSubclass", f_deleted_subclass, QCF_NO_FLAGS, QDOM_DEFAULT, autoHashTypeInfo);
    module_ns->addBuiltinVariant("neverClosed", f_never_closed, QCF_NO_FLAGS, QDOM_DEFAULT, autoHashTypeInfo);
    module_ns->addBuiltinVariant("concurrentReaders", f_concurrent_readers, QCF_NO_FLAGS, QDOM_DEFAULT,
        autoHashTypeInfo, 1, bigIntTypeInfo, QORE_PARAM_NO_ARG, "n");
    module_ns->addBuiltinVariant("rescanReaders", f_rescan_readers, QCF_NO_FLAGS, QDOM_DEFAULT, autoHashTypeInfo, 1,
        bigIntTypeInfo, QORE_PARAM_NO_ARG, "n");
    module_ns->addBuiltinVariant("gapProgram", f_gap_program, QCF_NO_FLAGS, QDOM_DEFAULT, autoHashTypeInfo, 1,
        codeTypeInfo, QORE_PARAM_NO_ARG, "cb");
    module_ns->addBuiltinVariant("checkObject", f_check_object, QCF_NO_FLAGS, QDOM_DEFAULT, autoHashTypeInfo, 1,
        objectTypeInfo, QORE_PARAM_NO_ARG, "o");
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
