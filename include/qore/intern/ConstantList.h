/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ConstantList.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    constants can only be defined when parsing
    constants values will be substituted during the 2nd parse phase

    reads and writes are (must be) wrapped under the program-level parse lock

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

#ifndef _QORE_CONSTANTLIST_H

#define _QORE_CONSTANTLIST_H

#include <qore/common.h>
#include "qore/intern/ParseNode.h"
#include "qore/intern/QoreTypeInfo.h"
#include "qore/intern/qore_aot_deps.h"

#include <string>

class qore_ns_private;
class qore_class_private;

//! parse-time thread-local depth of constant value initialization currently in progress
/** Incremented for the duration of each ConstantEntry::parseInit() (via ConstantEntryInitHelper).  While this
    is non-zero the parser is folding a constant value, and callee function/method BODIES must not be eagerly
    parse-initialized at call sites (only their signatures) - see FunctionCallBase::parseArgsVariant().  A
    constant's value only ever needs the callee's signature (return type) at parse time; its value is computed
    later at parseCommitRuntimeInit.  Eagerly parse-initializing a callee body during constant folding forces
    value-initialization of every constant that body references, which manufactures spurious, commit-order
    (file-glob) dependent "recursive constant reference" cycles. */
extern thread_local unsigned qore_constant_init_depth;

//! returns true if the parser is currently folding a constant value (qore_constant_init_depth != 0)
static inline bool qore_parse_in_constant_init() {
    return qore_constant_init_depth != 0;
}

// tricky structure that holds 2 types of pointers and a flag in the space of 1 pointer
// the flag is in the low bit since memory has to be aligned anyway we have at a few bits of space for flags
struct ClassNs {
    // if the low bit is set, then ptr is a qore_ns_priv*, otherwise it's a qore_class_private
    size_t ptr;

    DLLLOCAL ClassNs(qore_class_private* qc) : ptr((size_t)qc) {
    }

    DLLLOCAL ClassNs(qore_ns_private* ns) {
        ptr = ((size_t)ns) | (size_t)1;
    }

    DLLLOCAL ClassNs(const ClassNs& old) : ptr(old.ptr) {
    }

    DLLLOCAL qore_class_private* getClass() const {
        return (!(ptr & (size_t)1)) ? (qore_class_private*)ptr : nullptr;
    }

    DLLLOCAL qore_ns_private* getNs() const {
        return (ptr & (size_t)1) ? (qore_ns_private*)(ptr & ~(size_t)1) : nullptr;
    }

    DLLLOCAL bool isNs() const {
        return (bool)(ptr & (size_t)1);
    }

#ifdef DEBUG
    DLLLOCAL const char* getType() const {
        return isNs() ? "namespace" : "class";
    }

    DLLLOCAL const char* getName() const;
#endif
};

class RuntimeConstantRefNode;
class ConstantEntry;

DLLLOCAL bool qore_is_deferred_runtime_init_exception(ExceptionSink* xsink);

//! An AOT constant initializer that module load could not execute; owned by the AOT runtime
struct AOTPendingConstantInit;

//! Runs the deferred AOT initializer of an unpopulated constant, if there is one
/** Module load runs AOT constant initializers in serialization order with a fix-point retry.  When that order
    cannot be satisfied — typically because an initializer depends on state another module is still building —
    the constant stays an unpopulated shell.  Running the initializer from the first read makes load order
    irrelevant: any dependency read from inside it is initialized on demand as well.

    @param ce the constant entry being read
    @param xsink Qore-language exception sink; the initializer's own exception is raised here

    @return 1 if the constant now has a value, 0 if there is nothing to run (the caller reports the constant as
        unavailable), -1 if the initializer failed and \a xsink holds the reason
*/
DLLLOCAL int qore_aot_run_pending_constant_init(ConstantEntry* ce, ExceptionSink* xsink);

//! Returns "err: desc" for the module-load failure recorded for an unpopulated AOT constant, or an empty string
DLLLOCAL std::string qore_aot_get_pending_constant_error(ConstantEntry* ce);

//! Returns true if this thread is already materializing the given constant's stored value
/** ConstantEntry::getReferencedValue() walks the value it returns, resolving the AOT constant references
    serialized inside it.  A reference that leads back to a constant already being walked further up the same
    call stack is a cycle in the serialized reference graph, and following it recurses until the stack
    overflows.  The state is per-thread because a cycle is always contained in one thread's call stack, which
    also keeps concurrent reads of the same constant race-free.

    @param ce the constant entry a reference is about to be resolved from
*/
DLLLOCAL bool qore_constant_deep_resolve_in_flight(const ConstantEntry* ce);

//! Returns the next value of the process-wide constant initialization counter
/** Stamped on a ConstantEntry when its value is finished; see ConstantEntry::getInitSeq().
*/
DLLLOCAL uint64_t qore_next_constant_init_seq();

class ConstantEntry : public QoreReferenceCounter {
    friend class ConstantEntryInitHelper;
    friend class RuntimeConstantRefNode;
    friend class ConstantList;

public:
    const QoreProgramLocation* loc;
    ParseWarnOptions pwo;
    std::string name;
    const QoreTypeInfo* typeInfo;
    // unresolved declared type for an explicitly-typed constant; resolution is deferred to parseInit() so that
    // forward references to hashdecls/classes declared later in the same module resolve correctly (the same
    // deferral that type inference already gets); nullptr once resolved or for built-in/already-resolved types
    QoreParseTypeInfo* parseTypeInfo = nullptr;
    QoreValue val{};
    bool in_init : 1,     // being initialized
        pub : 1,          // public constant (modules only)
        init : 1,         // already initialized
        builtin : 1,      // builtin vs user
        delayed_eval : 1, // delayed evaluation
        explicit_type : 1, // constant has an explicit declared type
        has_init_expr : 1, // had a delayed-eval init expression (for AOT)
        saved_val_set : 1, // saved_val contains a runtime value or preserved init expression
        aot_shell_pending : 1, // AOT-deserialized shell whose init-func has not run
        external_stub : 1, // qcc --stub declaration; value is supplied by the runtime host
        external_stub_dependent : 1, // initializer references an external stub constant
        rt_in_init : 1, // runtime value evaluation in progress (parseCommitRuntimeInit); detects genuine cycles
        aot_parse_shell_value_set : 1 // a preloaded `.qo` shell supplied a compile-time value (see below)
        ;

    //! deferred AOT initializer for an unpopulated shell, or nullptr; owned by the AOT runtime
    /** Copied with the entry so that every Program importing the same AOT module can run the initializer from
        its own copy; cleared once the value has been stored.
    */
    AOTPendingConstantInit* aot_pending_init = nullptr;

    //! where this constant's value falls in the order constant values were finished
    /** Copied with the entry, so importing a module preserves the relative order its constants were
        initialized in.  See getInitSeq() for what depends on it.
    */
    uint64_t init_seq = 0;

    DLLLOCAL ConstantEntry(const QoreProgramLocation* loc, const char* n, QoreValue v,
        const QoreTypeInfo* ti = nullptr, bool n_pub = false, bool n_init = false, bool n_builtin = false,
        ClassAccess n_access = Public, QoreParseTypeInfo* pti = nullptr);

    DLLLOCAL ConstantEntry(const ConstantEntry& old);

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            del(xsink);
            delete this;
        }
    }

    DLLLOCAL void deref(QoreListNode& l) {
        if (ROdereference()) {
            del(l);
            delete this;
        }
    }

    DLLLOCAL void ref() const {
        ROreference();
    }

    DLLLOCAL ConstantEntry* refSelf() const {
        ref();
        return const_cast<ConstantEntry*>(this);
    }

    //! Where this constant's value falls in the order constant values were finished
    /** Initializing a constant resolves its initializer first, which initializes every constant that
        initializer reads.  Completion order is therefore a topological order of the "holds another constant's
        value" relation: whatever a constant folded in was finished before it was.

        The AOT writer records a shared value node under the lowest sequence that reaches it, which makes the
        constant that *defines* a shared value its owner and every constant aliasing or containing it a
        reference to it.  A reference then always names a constant finished earlier, which any unit able to
        see the holder has already loaded.

        Declaration order cannot be used for this: constant initializers are resolved lazily, so an entry may
        be created long before the constant its initializer reads.  Qorus declares
        `MapperFieldCodeTypeHelper::JavaTypeMap = OMQ::MapperProgram::JavaTypeMap` in a source parsed 115
        files ahead of the one declaring `MapperProgram::JavaTypeMap`, so by creation order the alias comes
        first.  Ranking the candidate paths cannot express it either: the traversal walks namespaces and
        classes in tree order, which has no relation to which source unit is upstream, and an alias whose
        class name sorts first then owned a value its own definer's object had to reference -- unresolvable
        for any consumer, since the alias is not one of the definer's predecessors.
    */
    DLLLOCAL uint64_t getInitSeq() const {
        return init_seq;
    }

    DLLLOCAL QoreValue getReferencedValue() const;

    DLLLOCAL const QoreValue getValue() const;

    //! Returns true if the constant's value has been initialized
    DLLLOCAL bool hasValue() const {
        return !aot_shell_pending && !external_stub
            && (saved_val_set || (init && val.getType() != NT_RTCONSTREF));
    }

    //! Supplies the value a preloaded `.qo` shell recorded for this pending constant
    /** A `.qo` carries declarations, not executable bodies, so a `qcc -c -L` parse resolving a constant
        against a preloaded shell cannot run the provider's `__const_init` function.  A whole-group parse can
        evaluate the provider's retained initializer during the consumer's parse, and evaluating a constant
        narrows its declared type to its value's type (see parseCommitRuntimeInit()), so the same source
        compiled the two ways published different declared types for every constant folded from a sibling --
        which invalidated every consumer on a mode change.  The provider records the value it computed itself
        (AOT format v15), and a parse reads it here.

        Only a qcc source parse reads this; at runtime the `__const_init` function remains the sole source of
        the value, so nothing about module load order changes.

        @param v the value the shell recorded; ownership is taken
    */
    DLLLOCAL void setAOTParseShellValue(QoreValue v) {
        if (aot_parse_shell_value_set) {
            aot_parse_shell_value.discard(nullptr);
        }
        aot_parse_shell_value = v;
        aot_parse_shell_value_set = true;
    }

    //! Returns true if a preloaded shell supplied a compile-time value for this constant
    /** A value is attached only where `.qo` declaration shells are preloaded for a compile
        (QoreAOTBinaryDeserializer::preload_parse_constant_values), so a runtime module load never has one and
        the `__const_init` function stays the only source of a constant's value there.
    */
    DLLLOCAL bool hasAOTParseShellValue() const {
        return aot_parse_shell_value_set;
    }

    //! Deep-resolves the AOT constant references inside a shell-supplied compile-time value
    /** The value is deserialized while sibling shells are still being registered, so references into other
        constants are left as deferred reference nodes; they have to be replaced with the values they name
        before any parse can fold the constant, or a reference node reaches a typed container and the parse
        fails with a RUNTIME-TYPE-ERROR naming an 'AOT constant path reference'.

        A reference that cannot be resolved drops the whole value rather than failing: without it the
        consuming parse defers the initializer exactly as it did before shells carried values at all.

        @param xsink exception sink used for the walk; any exception it takes is consumed here
    */
    DLLLOCAL void materializeAOTParseShellValue(ExceptionSink* xsink);

    DLLLOCAL const QoreTypeInfo* getParseTypeInfo() const {
        if (qore_aot_source_parse_active() && typeInfo == nothingTypeInfo
                && (aot_shell_pending || external_stub || external_stub_dependent || val.hasNode())) {
            return autoTypeInfo;
        }
        return QoreTypeInfo::hasType(typeInfo) ? typeInfo : autoTypeInfo;
    }

    //! Sets the runtime value (val + saved_val) for AOT init functions
    DLLLOCAL void setRuntimeValue(QoreValue result, ExceptionSink* xsink);
    DLLLOCAL void materializeRuntimeRefs(ExceptionSink* xsink);

    // Follows a chain of RuntimeConstantRefNode indirections (const A = B;
    // const B = ...) to the terminal stored value while preserving unresolved
    // AOT shells / external stubs as runtime references.
    DLLLOCAL static const QoreValue& resolveRtConstRef(const QoreValue& start);

    DLLLOCAL int parseInit(ClassNs ptr);

    DLLLOCAL int parseCommitRuntimeInit();

    DLLLOCAL QoreValue get(const QoreProgramLocation* loc, const QoreTypeInfo*& constantTypeInfo, ClassNs ptr) {
        if (in_init) {
            parse_error(*loc, "recursive constant reference found to constant '%s'", name.c_str());
            constantTypeInfo = nothingTypeInfo;
            return QoreValue();
        }

        if (!init && parseInit(ptr)) {
            constantTypeInfo = nothingTypeInfo;
            return QoreValue();
        }

        // AOT incremental dependency: record this constant's source file so a
        // `qcc -c -L` compile that folds the value still rebuilds when the
        // defining file changes (the folded literal leaves no trace in the
        // emitted `.qo`).  No-op unless an AOT dependency sink is active.
        qore_aot_note_referenced_decl(this->loc, loc);

        constantTypeInfo = getParseTypeInfo();
        return val;
    }

    DLLLOCAL const char* getName() const {
        return name.c_str();
    }

    DLLLOCAL const std::string& getNameStr() const {
        return name;
    }

    DLLLOCAL bool isPublic() const {
        return pub;
    }

    DLLLOCAL bool isUserPublic() const {
        return pub && !builtin;
    }

    DLLLOCAL bool isSystem() const {
        return builtin;
    }

    DLLLOCAL bool isUser() const {
        return !builtin;
    }

    DLLLOCAL ClassAccess getAccess() const {
        return access;
    }

    DLLLOCAL const char* getModuleName() const {
        return from_module.empty() ? nullptr : from_module.c_str();
    }

    //! Returns true if this constant had a delayed-eval init expression
    DLLLOCAL bool hasInitExpr() const {
        return has_init_expr;
    }

    //! Returns true for qcc --stub constants that are declarations only.
    DLLLOCAL bool isExternalStub() const {
        return external_stub;
    }

    //! Returns true when this constant's preserved init expression depends on a qcc --stub constant.
    DLLLOCAL bool isExternalStubDependent() const {
        return external_stub_dependent;
    }

    //! Converts this parse-time stub placeholder into a runtime-resolved declaration.
    DLLLOCAL void makeExternalStubDeclaration();

    //! Returns the preserved init expression (for AOT lowering); NOTHING if not preserved
    DLLLOCAL const QoreValue getInitExpr() const {
        return aot_init_expr;
    }

    //! Discard the preserved init expression (call after AOT lowering is complete)
    DLLLOCAL void discardInitExpr(ExceptionSink* xsink) {
        aot_init_expr.discard(xsink);
    }

protected:
    QoreValue saved_val{};
    QoreValue aot_init_expr{};  //!< preserved init expression for AOT lowering
    //! compile-time value recorded by a preloaded `.qo` shell; see setAOTParseShellValue()
    QoreValue aot_parse_shell_value{};
    ClassAccess access;
    std::string from_module;

    DLLLOCAL ~ConstantEntry() {
        assert(saved_val.isNothing());
        assert(aot_init_expr.isNothing());
        assert(aot_parse_shell_value.isNothing());
        assert(val.isNothing());
        // free the deferred declared type if parseInit() never ran (e.g. a parse error aborted the commit)
        delete parseTypeInfo;
    }

    DLLLOCAL void del(ExceptionSink* xsink);
    DLLLOCAL void del(QoreListNode& l);
};

class ConstantEntryInitHelper {
protected:
    ConstantEntry &ce;

public:
    DLLLOCAL ConstantEntryInitHelper(ConstantEntry& n_ce) : ce(n_ce) {
        assert(!ce.in_init);
        assert(!ce.init);
        ce.in_init = true;
        // mark that we are folding a constant value so callee bodies are not eagerly parse-initialized at
        // call sites reached from this constant's initializer (see qore_constant_init_depth)
        ++qore_constant_init_depth;
        //printd(5, "ConstantEntryInitHelper::ConstantEntryInitHelper() '%s'\n", ce.getName());
    }

    DLLLOCAL ~ConstantEntryInitHelper() {
        assert(qore_constant_init_depth > 0);
        --qore_constant_init_depth;
        ce.in_init = false;
        ce.init = true;
        // the value is finished here, and so is every constant this one's initializer read: that is what
        // makes the sequence a topological order of the folding relation rather than of the source text
        ce.init_seq = qore_next_constant_init_seq();
        //printd(5, "ConstantEntryInitHelper::~ConstantEntryInitHelper() '%s'\n", ce.getName());
    }
};

// we use a vector map as the number of constants is generally relatively small
// and lookups are only performed during parsing
#include <qore/vector_map>
typedef vector_map_t<const char*, ConstantEntry*> cnemap_t;
/*
#ifdef HAVE_QORE_HASH_MAP
//#warning compiling with hash_map
#include <qore/hash_map_include.h>
#include "qore/intern/xxhash.h"

typedef HASH_MAP<const char*, ConstantEntry*, qore_hash_str, eqstr> cnemap_t;
#else
#include <map>
typedef std::map<const char*, ConstantEntry*, ltstr> cnemap_t;
#endif
*/

class ConstantList {
    friend class ConstantListIterator;
    friend class ConstConstantListIterator;

private:
    // not implemented
    DLLLOCAL ConstantList& operator=(const ConstantList&);

    DLLLOCAL void clearIntern(ExceptionSink* xsink);

    size_t runtime_init_hwm = 0;  // high water mark for initialized constants

protected:
    // the object that owns the list (either a class or a namespace)
    ClassNs ptr;

public:
    vector_map_t<std::string, ConstantEntry*> new_cnemap;
    cnemap_t cnemap;

    DLLLOCAL ~ConstantList();

    DLLLOCAL ConstantList(ClassNs p) : ptr(p) {
        //printd(5, "ConstantList::ConstantList() this: %p cls: %p ns: %p\n", this, ptr.getClass(), ptr.getNs());
    }

    DLLLOCAL ConstantList(const ConstantList& old, const QoreParseOptions& po, ClassNs p);

    // do not delete the object returned by this function
    DLLLOCAL cnemap_t::iterator add(const char* name, QoreValue val, const QoreTypeInfo* typeInfo = nullptr,
            ClassAccess access = Public);

    DLLLOCAL cnemap_t::iterator parseAdd(const QoreProgramLocation* loc, const char* name, QoreValue val,
            const QoreTypeInfo* typeInfo = nullptr, bool pub = false, ClassAccess access = Public,
            QoreParseTypeInfo* parseTypeInfo = nullptr);

    //! Add a pre-created ConstantEntry (takes ownership)
    /** Uses ce->getName() as the key — which returns a pointer into the
        ConstantEntry's own std::string storage — so the key remains stable
        for the lifetime of the entry. The deserializer-supplied `name`
        pointer must NOT be used here: it points into the AOT binary
        reader's string pool, which is freed when the deserializer is
        destroyed (end of qore_aot_module_init_v3), leaving dangling keys
        that cause find() to fail while iteration still works.
    */
    DLLLOCAL void addEntry(const char* /*name*/, ConstantEntry* ce) {
        cnemap.insert(cnemap_t::value_type(ce->getName(), ce));
    }

    DLLLOCAL ConstantEntry* findEntry(const char* name);

    DLLLOCAL const ConstantEntry* findEntry(const char* name) const;

    DLLLOCAL QoreValue find(const char* name, const QoreTypeInfo*& constantTypeInfo, ClassAccess& access,
            bool& found, const QoreProgramLocation* consumer_loc = nullptr);

    DLLLOCAL QoreValue find(const char* name, const QoreTypeInfo*& constantTypeInfo, bool& found,
            const QoreProgramLocation* consumer_loc = nullptr) {
        ClassAccess access;
        return find(name, constantTypeInfo, access, found, consumer_loc);
    }

    DLLLOCAL bool inList(const char* name) const;
    DLLLOCAL bool inList(const std::string& name) const;
    //DLLLOCAL ConstantList *copy();

    // assimilate the list without any duplicate checking
    DLLLOCAL void assimilate(ConstantList& n);

    // assimilate a constant list in a namespace with duplicate checking (also in pending list)
    DLLLOCAL void assimilate(ConstantList& n, const char* type, const char* name,
        std::vector<std::string>* pending_names = nullptr);

    // copy all user/public elements of the source list to the target, assuming no duplicates
    DLLLOCAL void mergeUserPublic(const ConstantList& src);

    DLLLOCAL int importSystemConstants(const ConstantList& src, ExceptionSink* xsink);

    // add a constant to a list with duplicate checking (pub & priv + pending)
    DLLLOCAL void parseAdd(const QoreProgramLocation* loc, const std::string& name, QoreValue val, ClassAccess access,
            const char* cname, const QoreTypeInfo* typeInfo = nullptr, QoreParseTypeInfo* parseTypeInfo = nullptr);

    DLLLOCAL int parseInit();
    DLLLOCAL int parseCommitRuntimeInit();

    DLLLOCAL QoreHashNode* getInfo();
    DLLLOCAL void parseDeleteAll();
    DLLLOCAL void clear(QoreListNode& l);
    DLLLOCAL void deleteAll(ExceptionSink* xsink);
    DLLLOCAL void reset();
    //! Removes a specific constant by name (for selective rollback support)
    DLLLOCAL void parseRemove(const char* name, ExceptionSink* xsink);

    DLLLOCAL bool empty() const {
        return cnemap.empty();
    }

    DLLLOCAL cnemap_t::iterator end() {
        return cnemap.end();
    }

    DLLLOCAL cnemap_t::const_iterator end() const {
        return cnemap.end();
    }

    DLLLOCAL void setAccess(ClassAccess access) {
        for (auto& i : cnemap)
            i.second->access = access;
    }
};

class ConstantListIterator {
protected:
    cnemap_t& cl;
    cnemap_t::iterator i;

public:
    DLLLOCAL ConstantListIterator(ConstantList& n_cl) : cl(n_cl.cnemap), i(cl.end()) {
    }

    DLLLOCAL bool next() {
        if (i == cl.end()) {
            i = cl.begin();
        } else {
            ++i;
        }
        return i != cl.end();
    }

    DLLLOCAL const std::string& getName() const {
        return i->second->getNameStr();
    }

    DLLLOCAL QoreValue getValue() const {
        return i->second->val;
    }

    DLLLOCAL ConstantEntry* getEntry() const {
        return i->second;
    }

    DLLLOCAL ClassAccess getAccess() const {
        return i->second->getAccess();
    }

    DLLLOCAL bool isPublic() const {
        return i->second->isPublic();
    }

    DLLLOCAL bool isUserPublic() const {
        return i->second->isUserPublic();
    }
};

class ConstConstantListIterator {
protected:
    const cnemap_t& cl;
    cnemap_t::const_iterator i;

public:
    DLLLOCAL ConstConstantListIterator(const ConstantList& n_cl) : cl(n_cl.cnemap), i(cl.end()) {
    }

    DLLLOCAL bool next() {
        if (i == cl.end())
            i = cl.begin();
        else
            ++i;
        return i != cl.end();
    }

    DLLLOCAL const std::string& getName() const {
        return i->second->getNameStr();
    }

    DLLLOCAL const QoreValue getValue() const {
        return i->second->val;
    }

    DLLLOCAL const ConstantEntry* getEntry() const {
        return i->second;
    }

    DLLLOCAL bool isPublic() const {
        return i->second->isPublic();
    }

    DLLLOCAL bool isUserPublic() const {
        return i->second->isUserPublic();
    }
};

class RuntimeConstantRefNode : public ParseNode {
protected:
    ConstantEntry* ce;

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context) {
        parse_context.typeInfo = ce->getParseTypeInfo();
        if (ce->external_stub) {
            parse_context.external_stub_constant_ref = true;
        }
        return 0;
    }

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const {
        return ce->getParseTypeInfo();
    }

    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
        // For constants still undergoing delayed init (parseCommitRuntimeInit
        // path), ce->saved_val holds the committed value. For normal/builtin
        // constants (e.g. Reflection::IntType), only ce->val is populated and
        // saved_val remains empty — fall back to evaluating ce->val in that
        // case so AOT init functions referencing builtin constants get the
        // correct value instead of NOTHING.
        if (ce->saved_val_set) {
            // Genuine runtime constant-value cycle: this constant's value is currently being evaluated (in
            // ConstantEntry::parseCommitRuntimeInit) and its computation references itself, typically via a
            // function/method body that reads the constant.  The parse-time detector cannot see such indirect
            // back-edges, so detect them here and report deterministically instead of recursing until the stack
            // overflows.  (When the referenced constant is deferred but not yet committed, ce->saved_val still
            // holds its initializer expression, which is evaluated inline here - correct and order-independent
            // for the pure expressions constants must be.)
            if (ce->rt_in_init) {
                xsink->raiseException("RECURSIVE-CONSTANT-REFERENCE", "recursive reference detected while "
                    "initializing the runtime value of constant '%s'", ce->getName());
                return QoreValue();
            }
            return ce->saved_val.eval(needs_deref, xsink);
        }
        // A `.qo` preloaded as a declaration shell records the value its provider computed for a pending
        // constant, so a parse resolving against shells evaluates an initializer that reads it exactly as a
        // whole-group parse does.  hasAOTParseShellValue() is false at runtime, where the init function is
        // still the only source of the value.
        if (ce->hasAOTParseShellValue()) {
            return ce->aot_parse_shell_value.eval(needs_deref, xsink);
        }
        // AOT pending shell: ce->val is a self-referential RuntimeConstantRefNode
        // set up by QoreAOTBinaryDeserializer::deserializeConstants so sibling
        // `.qo` references defer to runtime.  Evaluating ce->val here would
        // re-enter evalImpl → infinite recursion → stack overflow.  The
        // legitimate consumer is post-init-func dispatch (saved_val populated,
        // handled above); any other eval path is a programmer error (typically
        // parse-time fold of an unpopulated pending constant) and must raise
        // rather than loop.
        if (ce->external_stub) {
            xsink->raiseException("EXTERNAL-STUB-CONSTANT",
                "cannot evaluate external stub constant '%s'; the runtime host "
                "must inject this constant before loading AOT code that references it",
                ce->getName());
            return QoreValue();
        }
        if (ce->aot_shell_pending || !ce->hasValue()) {
            // The initializer that module load could not run is run here, from the first read; this is what
            // keeps AOT constant initialization independent of module load order.
            int rc = runPendingInit(xsink);
            if (rc < 0) {
                return QoreValue();
            }
            if (rc > 0) {
                return evalImpl(needs_deref, xsink);
            }
            raisePendingError(xsink, "evaluate");
            return QoreValue();
        }
        return ce->val.eval(needs_deref, xsink);
    }

    //! Runs the deferred AOT initializer for an unpopulated constant
    /** @return 1 if the constant now has a value, 0 if there was nothing to run, -1 on failure (\a xsink set)
    */
    DLLLOCAL int runPendingInit(ExceptionSink* xsink) const {
        if (!ce->aot_pending_init) {
            return 0;
        }
        return qore_aot_run_pending_constant_init(ce, xsink);
    }

    //! Reports an unpopulated AOT constant, including the module-load failure that left it unpopulated
    DLLLOCAL void raisePendingError(ExceptionSink* xsink, const char* action) const {
        std::string cause = qore_aot_get_pending_constant_error(ce);
        if (cause.empty()) {
            xsink->raiseException("AOT-PENDING-CONSTANT",
                "cannot %s AOT-deserialized constant '%s' before its "
                "__const_init function has populated the value",
                action, ce->getName());
            return;
        }
        xsink->raiseException("AOT-PENDING-CONSTANT",
            "cannot %s AOT-deserialized constant '%s'; its __const_init function failed: %s",
            action, ce->getName(), cause.c_str());
    }

    DLLLOCAL ~RuntimeConstantRefNode() {
    }

public:
    DLLLOCAL RuntimeConstantRefNode(const QoreProgramLocation* loc, ConstantEntry* n_ce) : ParseNode(loc,
            NT_RTCONSTREF, true, false), ce(n_ce) {
        assert(ce->hasValue());
        assert(!ce->aot_shell_pending);
    }

    //! Constructor for AOT deferred evaluation — saved_val may not be set yet
    /** The init function will populate saved_val via setRuntimeValue() before
        evalImpl() is called at runtime.
    */
    DLLLOCAL RuntimeConstantRefNode(const QoreProgramLocation* loc, ConstantEntry* n_ce, bool aot_deferred)
            : ParseNode(loc, NT_RTCONSTREF, true, false), ce(n_ce) {
    }

    DLLLOCAL ConstantEntry* getConstantEntry() const {
        return ce;
    }

    DLLLOCAL virtual int getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
        if (ce->saved_val_set) {
            return ce->saved_val.getAsString(str, foff, xsink);
        }
        if (ce->hasAOTParseShellValue()) {
            return ce->aot_parse_shell_value.getAsString(str, foff, xsink);
        }
        if (ce->external_stub) {
            xsink->raiseException("EXTERNAL-STUB-CONSTANT",
                "cannot convert external stub constant '%s' to a string; the runtime host "
                "must inject this constant before loading AOT code that references it",
                ce->getName());
            return -1;
        }
        if (ce->aot_shell_pending || !ce->hasValue()) {
            int rc = runPendingInit(xsink);
            if (rc < 0) {
                return -1;
            }
            if (rc > 0) {
                return getAsString(str, foff, xsink);
            }
            raisePendingError(xsink, "convert");
            return -1;
        }
        return ce->val.getAsString(str, foff, xsink);
    }

    DLLLOCAL virtual QoreString* getAsString(bool& del, int foff, ExceptionSink* xsink) const {
        if (ce->saved_val_set) {
            return ce->saved_val.getAsString(del, foff, xsink);
        }
        if (ce->hasAOTParseShellValue()) {
            return ce->aot_parse_shell_value.getAsString(del, foff, xsink);
        }
        if (ce->external_stub) {
            xsink->raiseException("EXTERNAL-STUB-CONSTANT",
                "cannot convert external stub constant '%s' to a string; the runtime host "
                "must inject this constant before loading AOT code that references it",
                ce->getName());
            del = false;
            return nullptr;
        }
        if (ce->aot_shell_pending || !ce->hasValue()) {
            int rc = runPendingInit(xsink);
            if (rc < 0) {
                del = false;
                return nullptr;
            }
            if (rc > 0) {
                return getAsString(del, foff, xsink);
            }
            raisePendingError(xsink, "convert");
            del = false;
            return nullptr;
        }
        return ce->val.getAsString(del, foff, xsink);
    }

    DLLLOCAL virtual const char* getTypeName() const {
        if (ce->saved_val_set) {
            return ce->saved_val.getTypeName();
        }
        if (ce->external_stub || ce->aot_shell_pending || !ce->hasValue()) {
            return QoreTypeInfo::getName(ce->getParseTypeInfo());
        }
        return ce->val.getTypeName();
    }
};

#endif // _QORE_CONSTANTLIST_H
