/*
    ConstantList.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#include <qore/Qore.h>
#include "qore/intern/ConstantList.h"
#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/QoreHashNodeIntern.h"

#include <cstdlib>
#include <cstring>

#ifdef DEBUG
const char* ClassNs::getName() const {
   return isNs() ? getNs()->name.c_str() : getClass()->name.c_str();
}
#endif

ConstantEntry::ConstantEntry(const QoreProgramLocation* loc, const char* n, QoreValue val, const QoreTypeInfo* ti,
        bool n_pub, bool n_init, bool n_builtin, ClassAccess n_access)
        : loc(loc), name(n), typeInfo(ti), val(val), in_init(false), pub(n_pub),
        init(n_init), builtin(n_builtin), delayed_eval(false), access(n_access) {
    QoreProgram* pgm = getProgram();
    if (pgm)
        pwo = qore_program_private::getParseWarnOptions(pgm);

    const char* mod_name = get_module_context_name();
    if (mod_name) {
        from_module = mod_name;
    }

    //printd(5, "ConstantEntry::ConstantEntry() this: %p '%s' ti: '%s' nti: '%s'\n", this, n,
    //  QoreTypeInfo::getName(typeInfo), QoreTypeInfo::getName(val.getTypeInfo()));
}

ConstantEntry::ConstantEntry(const ConstantEntry& old)
        : loc(old.loc), pwo(old.pwo), name(old.name),
        typeInfo(old.typeInfo), val(old.val.refSelf()),
        in_init(false), pub(old.builtin), init(true), builtin(old.builtin), delayed_eval(old.delayed_eval),
        saved_val(old.saved_val.refSelf()),
        access(old.access), from_module(old.from_module) {
    assert(!old.in_init);
    assert(old.init);
    //printd(5, "ConstantEntry::ConstantEntry() this: %p copy '%s' ti: '%s' nti: '%s'\n", this, name.c_str(),
    //  QoreTypeInfo::getName(typeInfo), QoreTypeInfo::getName(val.getTypeInfo()));
}

void ConstantEntry::del(QoreListNode& l) {
    //printd(5, "ConstantEntry::del(l) this: %p '%s' node: %p (%d) %s %d (saved_val: %s)\n", this, name.c_str(),
    //  node, get_node_type(node), get_type_name(node), node->reference_count(), saved_val.getTypeName());
    if (saved_val) {
        val.discard(nullptr);
        l.push(saved_val, nullptr);
#ifdef DEBUG
        val.clear();
        saved_val.clear();
#endif
    } else {
        if (val.hasNode()) {
            l.push(val.takeNode(), nullptr);
        }
#ifdef DEBUG
        val.clear();
#endif
    }
}

void ConstantEntry::del(ExceptionSink* xsink) {
    if (saved_val) {
        val.discard(xsink);
        saved_val.discard(xsink);
#ifdef DEBUG
        val.clear();
        saved_val.clear();
#endif
    } else {
        // note that objects may be present here when discarding with xsink == nullptr if there is a builtin object in
        // a class constant; in this case the destructor cannot throw an exception
        val.discard(xsink);
#ifdef DEBUG
        val.clear();
#endif
    }
}

int ConstantEntry::parseInit(ClassNs ptr) {
    //printd(5, "ConstantEntry::parseInit() this: %p '%s' pub: %d init: %d in_init: %d node: %p '%s' "
    //  "class context: %p '%s' ns: %p ('%s') pub: %d\n", this, name.c_str(), pub, init, in_init, node,
    //  get_type_name(node), ptr.getClass(), ptr.getClass() ? ptr.getClass()->name.c_str() : "<none>", ptr.getNs(),
    //  ptr.getNs() ? ptr.getNs()->name.c_str() : "<none>", ptr.getNs() ? ptr.getNs()->pub : 0);
    if (init) {
        return 0;
    }

    if (in_init) {
        parse_error(*loc, "recursive constant reference found to constant '%s'", name.c_str());
        return -1;
    }

    ConstantEntryInitHelper ceih(*this);

    if (!val.hasNode()) {
        return 0;
    }

    int err = 0;

    QoreProgram* pgm;
    if (!builtin) {
        QoreParseContext parse_context;
        parse_context.setFlags(PF_CONST_EXPRESSION);

        // push parse class context
        qore_class_private* p = ptr.getClass();
        QoreParseClassHelper qpch(p ? p->cls : nullptr, ptr.getNs());

        // ensure that there is no accessible local variable state
        VariableBlockHelper vbh;

        // set parse options and warning mask for this statement
        ParseWarnHelper pwh(pwo);

        //printd(5, "ConstantEntry::parseInit() this: %p '%s' about to init val: '%s' class: %p '%s'\n", this,
        //    name.c_str(), val.getFullTypeName(), p, p ? p->name.c_str() : "n/a");
        err = parse_init_value(val, parse_context);
        typeInfo = parse_context.typeInfo;
        assert(!parse_context.lvids);
        pgm = parse_context.pgm;
        assert(pgm == getProgram());
    } else {
        pgm = getProgram();
    }

    //printd(5, "ConstantEntry::parseInit() this: %p %s initialized to node: %p (%s) value: %d type: '%s'\n", this,
    //  name.c_str(), node, get_type_name(node), node->is_value(), QoreTypeInfo::getName(typeInfo));

    // do not evaluate expression if any parse exceptions have been thrown
    if (!val.hasNode() || !val.getInternalNode()->needs_eval() || pgm->parseExceptionRaised()) {
        if (!QoreTypeInfo::hasType(typeInfo)) {
            typeInfo = val.getTypeInfo();
        }
        return err;
    }

    delayed_eval = true;
    saved_val = val.takeIfNode();
    val = new RuntimeConstantRefNode(loc, this);
    return err;
}

int ConstantEntry::parseCommitRuntimeInit() {
    if (!delayed_eval) {
        return 0;
    }
    delayed_eval = false;
    assert(saved_val);
    assert(saved_val.needsEval());

    int err = 0;

    // evaluate expression
    ExceptionSink xsink;
    {
        RuntimeConfig parse_rc = rc_get_parse_time();
        ValueEvalOptimizedRefHolder v(parse_rc, saved_val, &xsink);

        //printd(5, "ConstantEntry::parseInit() this: %p %s evaluated to node: %p (%s)\n", this, name.c_str(), *v,
        //  get_type_name(*v));

        if (!xsink) {
            QoreValue nv = v.takeReferencedValue();
            saved_val.discard(&xsink);
            saved_val = nv;
            typeInfo = saved_val.getTypeInfo();
            assert(!saved_val.getInternalNode() || !saved_val.needsEval());
        } else {
            typeInfo = nothingTypeInfo;
        }
    }

    if (xsink.isEvent()) {
        qore_program_private::addParseException(getProgram(), xsink, loc);
        if (!err) {
            err = -1;
        }
    }

    return err;
}

QoreValue ConstantEntry::getReferencedValue() const {
    if (val.getType() == NT_RTCONSTREF) {
        return val.get<const RuntimeConstantRefNode>()->getConstantEntry()->saved_val.refSelf();
    } else {
        return val.refSelf();
    }
}

const QoreValue ConstantEntry::getValue() const {
    if (val.getType() == NT_RTCONSTREF) {
        return val.get<const RuntimeConstantRefNode>()->getConstantEntry()->saved_val;
    } else {
        return val;
    }
}

ConstantList::ConstantList(const ConstantList& old, int64 po, ClassNs p) : ptr(p) {
    //printd(5, "ConstantList::ConstantList(old: %p, p: %s %s) this: %p cls: %p ns: %p\n", &old, p.getType(),
    //  p.getName(), this, ptr.getClass(), ptr.getNs());
    cnemap_t::iterator last = cnemap.begin();
    for (cnemap_t::const_iterator i = old.cnemap.begin(), e = old.cnemap.end(); i != e; ++i) {
        assert(i->second->init);
        // only check copying criteria when copying a constant list in a namespace
        if (p.isNs()) {
            // check the public flag
            if (!i->second->pub)
                continue;
            if (po & PO_NO_INHERIT_USER_CONSTANTS && i->second->isUser())
                continue;
            if (po & PO_NO_INHERIT_SYSTEM_CONSTANTS && i->second->isSystem())
                continue;
        }

        ConstantEntry* ce = i->second;

        if (ce->getModuleName() || !get_module_context_name()) {
            ce->ref();
        } else {
            ce = new ConstantEntry(*ce);
        }

        last = cnemap.insert(last, cnemap_t::value_type(ce->getName(), ce));
        //printd(5, "ConstantList::ConstantList(old=%p) this=%p copying %s (%p)\n", &old, this, i->first,
        //  i->second->node);
    }
}

ConstantList::~ConstantList() {
    //QORE_TRACE("ConstantList::~ConstantList()");
    // for non-debug mode with old modules: clear constants here
    //fprintf(stderr, "XXX ConstantList::~ConstantList() this=%p size=%d\n", this, cnemap.size());

    reset();
}

void ConstantList::reset() {
   if (!cnemap.empty())
      clearIntern(0);
}

void ConstantList::clearIntern(ExceptionSink* xsink) {
    for (auto& i : cnemap) {
        if (i.second) {
            i.second->deref(xsink);
        }
    }

    cnemap.clear();
}

// called at runtime
void ConstantList::clear(QoreListNode& l) {
    for (auto& i : cnemap) {
        if (i.second) {
            i.second->deref(l);
        }
    }

    cnemap.clear();
}

// called at runtime
void ConstantList::deleteAll(ExceptionSink* xsink) {
    clearIntern(xsink);
}

void ConstantList::parseDeleteAll() {
    ExceptionSink xsink;
    clearIntern(&xsink);

    if (xsink.isEvent())
        qore_program_private::addParseException(getProgram(), xsink);
}

cnemap_t::iterator ConstantList::parseAdd(const QoreProgramLocation* loc, const char* name, QoreValue value,
        const QoreTypeInfo* typeInfo, bool pub, ClassAccess access) {
    // first check if the constant has already been defined
    if (cnemap.find(name) != cnemap.end()) {
        parse_error(*loc, "constant \"%s\" has already been defined", name);
        value.discard(nullptr);
        return cnemap.end();
    }

    ConstantEntry* ce = new ConstantEntry(loc, name, value,
        typeInfo || (value.hasNode() && value.getInternalNode()->needs_eval()) ? typeInfo : value.getTypeInfo(),
        pub, false, false, access);
    return cnemap.insert(cnemap_t::value_type(ce->getName(), ce)).first;
}

cnemap_t::iterator ConstantList::add(const char* name, QoreValue value, const QoreTypeInfo* typeInfo,
        ClassAccess access) {
#ifdef DEBUG
    if (cnemap.find(name) != cnemap.end()) {
        printd(0, "ConstantList::add() %s added twice!", name);
        assert(false);
    }
#endif
    ConstantEntry* ce = new ConstantEntry(&loc_builtin, name, value,
        typeInfo || (value.hasNode() && value.getInternalNode()->needs_eval()) ? typeInfo : value.getTypeInfo(),
        true, true, true, access);
    return cnemap.insert(cnemap_t::value_type(ce->getName(), ce)).first;
}

ConstantEntry* ConstantList::findEntry(const char* name) {
    cnemap_t::iterator i = cnemap.find(name);
    return i == cnemap.end() ? 0 : i->second;
}

const ConstantEntry* ConstantList::findEntry(const char* name) const {
    cnemap_t::const_iterator i = cnemap.find(name);
    return i == cnemap.end() ? 0 : i->second;
}

QoreValue ConstantList::find(const char* name, const QoreTypeInfo*& constantTypeInfo, ClassAccess& access,
        bool& found) {
    cnemap_t::iterator i = cnemap.find(name);
    if (i != cnemap.end()) {
        if (!i->second->parseInit(ptr)) {
            constantTypeInfo = i->second->typeInfo;
            access = i->second->getAccess();
            found = true;
            return i->second->val;
        }
        constantTypeInfo = nothingTypeInfo;
        found = true;
        return QoreValue();
    }

    constantTypeInfo = nullptr;
    found = false;
    return QoreValue();
}

bool ConstantList::inList(const char* name) const {
    cnemap_t::const_iterator i = cnemap.find(name);
    return i != cnemap.end() ? true : false;
}

bool ConstantList::inList(const std::string& name) const {
    cnemap_t::const_iterator i = cnemap.find(name.c_str());
    return i != cnemap.end() ? true : false;
}

void ConstantList::mergeUserPublic(const ConstantList& src) {
    for (cnemap_t::const_iterator i = src.cnemap.begin(), e = src.cnemap.end(); i != e; ++i) {
        if (!i->second->isUserPublic()) {
            continue;
        }

        assert(!inList(i->first));

        ConstantEntry* n = new ConstantEntry(*i->second);
        cnemap[n->getName()] = n;
    }
}

int ConstantList::importSystemConstants(const ConstantList& src, ExceptionSink* xsink) {
    for (cnemap_t::const_iterator i = src.cnemap.begin(), e = src.cnemap.end(); i != e; ++i) {
        if (!i->second->isSystem())
            continue;

        if (inList(i->first)) {
            xsink->raiseException("IMPORT-SYSTEM-API-ERROR", "cannot import system constant %s due to an existing " \
                "constant with the same name in the target namespace", i->first);
            return -1;
        }

        ConstantEntry* n = new ConstantEntry(*i->second);
        cnemap[n->getName()] = n;
    }
    return 0;
}

// no duplicate checking is done here
void ConstantList::assimilate(ConstantList& n) {
    for (cnemap_t::iterator i = n.cnemap.begin(), e = n.cnemap.end(); i != e; ++i) {
        assert(!inList(i->first));
        // "move" data to new list
        cnemap[i->first] = i->second;
        i->second = nullptr;
    }

    n.parseDeleteAll();
}

// duplicate checking is done here
void ConstantList::assimilate(ConstantList& n, const char* type, const char* name,
        std::vector<std::string>* pending_names) {
    // assimilate target list
    for (cnemap_t::iterator i = n.cnemap.begin(), e = n.cnemap.end(); i != e; ++i) {
        if (inList(i->first)) {
            parse_error(*i->second->loc, "constant \"%s\" has already been defined in %s \"%s\"", i->first, type,
                name);
            continue;
        }

        cnemap[i->first] = i->second;
        // track the new constant name for rollback support
        if (pending_names) {
            pending_names->push_back(i->first);
        }
        i->second = nullptr;
    }

    n.parseDeleteAll();
}

void ConstantList::parseAdd(const QoreProgramLocation* loc, const std::string& name, QoreValue val,
        ClassAccess access, const char* cname) {
    if (inList(name)) {
        parse_error(*loc, "constant \"%s\" has already been defined in class \"%s\"", name.c_str(), cname);
        val.discard(nullptr);
        return;
    }

    ConstantEntry* ce = new ConstantEntry(loc, name.c_str(), val, val.getTypeInfo(), false, false, false, access);
    cnemap[ce->getName()] = ce;
}

int ConstantList::parseInit() {
    int err = 0;
    for (cnemap_t::iterator i = cnemap.begin(), e = cnemap.end(); i != e; ++i) {
        //printd(5, "ConstantList::parseInit() this: %p '%s' %p (class: %p '%s' ns: %p '%s')\n", this, i->first,
        //  i->second->node, ptr.getClass(), ptr.getClass() ? ptr.getClass()->name.c_str() : "n/a", ptr.getNs(),
        //  ptr.getNs() ? ptr.getNs()->name.c_str() : "n/a");
        if (i->second->parseInit(ptr) && !err) {
            err = -1;
        }
    }
    return err;
}

int ConstantList::parseCommitRuntimeInit() {
    int err = 0;
    for (auto& i : cnemap) {
        //printd(5, "ConstantList::parseInit() this: %p '%s' %p (class: %p '%s' ns: %p '%s')\n", this, i->first,
        //  i->second->node, ptr.getClass(), ptr.getClass() ? ptr.getClass()->name.c_str() : "n/a", ptr.getNs(),
        //  ptr.getNs() ? ptr.getNs()->name.c_str() : "n/a");
        if (i.second->parseCommitRuntimeInit() && !err) {
            err = -1;
        }
    }
    return err;
}

QoreHashNode* ConstantList::getInfo() {
    QoreHashNode* h = new QoreHashNode(autoTypeInfo);

    qore_hash_private* hp = qore_hash_private::get(*h);
    for (cnemap_t::iterator i = cnemap.begin(), e = cnemap.end(); i != e; ++i)
        hp->setKeyValueIntern(i->first, i->second->val.refSelf());

    return h;
}

void ConstantList::parseRemove(const char* name, ExceptionSink* xsink) {
    cnemap_t::iterator i = cnemap.find(name);
    if (i != cnemap.end()) {
        i->second->deref(xsink);
        cnemap.erase(i);
    }
}
