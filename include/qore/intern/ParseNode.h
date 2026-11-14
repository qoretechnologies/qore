/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ParseNode.h

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

#ifndef _QORE_PARSENODE_H

#define _QORE_PARSENODE_H

#include "qore/intern/QoreParseAnalysis.h"
#include "qore/intern/LexicalSemanticPolicy.h"
#include "qore/intern/WeakReferenceNode.h"
#include "qore/intern/WeakHashReferenceNode.h"
#include "qore/intern/WeakListReferenceNode.h"

#include <typeinfo>

struct QoreParseContext;

#ifndef NDEBUG
//! The node whose parse initialization parse_init_value() or parse_init_same_level() is making in this thread
/** Debug builds assert in ParseNode::parseInit() that every parse node is parse-initialized through one of these
    functions: parse_init_value() counts the levels of nesting of expressions, so that their evaluation checks the
    stack at regular intervals of nesting (see QORE_EVAL_STACK_CHECK_INTERVAL)
*/
DLLLOCAL extern thread_local const AbstractQoreNode* qore_parse_init_entry;
#endif

class ParseNode : public SimpleQoreNode {
public:
    const QoreProgramLocation* loc;

    DLLLOCAL ParseNode(const QoreProgramLocation* loc, qore_type_t t, bool n_needs_eval = true)
            : SimpleQoreNode(t, false, n_needs_eval), loc(loc), effect(n_needs_eval), ref_rv(true),
            parse_init(false) {
        effect_as_root = effect;
    }
    DLLLOCAL ParseNode(const QoreProgramLocation* loc, qore_type_t t, bool n_needs_eval, bool n_effect)
            : SimpleQoreNode(t, false, n_needs_eval), loc(loc), effect(n_effect), ref_rv(true), parse_init(false) {
        effect_as_root = effect;
    }
    DLLLOCAL ParseNode(const ParseNode& old) : SimpleQoreNode(old.type, false, old.needs_eval_flag), loc(old.loc),
            effect(old.effect), ref_rv(old.ref_rv), parse_init(false),
            semantic_policy_bound(old.semantic_policy_bound), semantic_policy(old.semantic_policy) {
        effect_as_root = effect;
    }
    // parse types should never be copied
    DLLLOCAL virtual AbstractQoreNode* realCopy() const {
        assert(false);
        return 0;
    }
    DLLLOCAL virtual bool is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        assert(false);
        return false;
    }
    DLLLOCAL virtual bool is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
        assert(false);
        return false;
    }
    DLLLOCAL void set_effect(bool n_effect) {
        effect = n_effect;
    }
    DLLLOCAL bool has_effect() const {
        return effect;
    }
    DLLLOCAL void set_effect_as_root(bool n_effect) {
        effect_as_root = n_effect;
    }
    DLLLOCAL bool has_effect_as_root() const {
        return effect_as_root;
    }
    DLLLOCAL void ignore_rv() {
        ref_rv = false;
    }
    DLLLOCAL bool need_rv() const {
        return ref_rv;
    }

    DLLLOCAL const QoreParseAnalysis& getParseAnalysis() const {
        return parse_analysis;
    }

    //! The operation's immutable compatibility semantics; zero never requests a Program lookup.
    DLLLOCAL LexicalSemanticPolicy getSemanticPolicy() const {
        return semantic_policy;
    }

    //! Binds a newly reduced source expression; pass-through grammar rules preserve an existing binding.
    DLLLOCAL static void bindSemanticPolicy(QoreValue value, LexicalSemanticPolicy policy);

    //! Copies policy to an unpublished execution clone, which does not undergo parse initialization again.
    DLLLOCAL void copySemanticPolicy(const ParseNode& source) {
        semantic_policy = source.semantic_policy;
        semantic_policy_bound = source.semantic_policy_bound;
    }

    //! Returns the type of the node's value as parse initialization gave it (nullptr if it is not known)
    DLLLOCAL const QoreTypeInfo* getParseTypeInfo() const {
        return getTypeInfo();
    }

    DLLLOCAL void setParseAnalysis(const QoreParseAnalysis& analysis) {
        captureSemanticPolicy();
        parse_analysis = analysis;
        parse_init = true;
    }

    DLLLOCAL virtual int parseInit(QoreValue& val, QoreParseContext& parse_context) {
        // every parse node is parse-initialized through parse_init_value() or parse_init_same_level()
        assert(qore_parse_init_entry == this);
        if (parse_init) {
            parse_context.typeInfo = getTypeInfo();
            parse_context.analysis = parse_analysis;
            return 0;
        }
        parse_init = true;
        captureSemanticPolicy();
        const LexicalSemanticPolicy policy = semantic_policy;
        LexicalParsePolicyHelper semantic_scope(policy);
        int rc = parseInitImpl(val, parse_context);
        if (val.hasNode() && val.getInternalNode() == this) {
            parse_analysis = parse_context.analysis;
        } else {
            // parseInitImpl may replace and destroy this node. Only unbound replacements inherit this
            // operation's policy; an existing operand returned by simplification keeps its own binding.
            bindSemanticPolicy(val, policy);
        }
        return rc;
    }

private:
    // not implemented
    ParseNode& operator=(const ParseNode&) = delete;

protected:
    //! if the node has an effect when evaluated (changes something)
    bool effect : 1;

    //! if the node has meaning as a top-level node
    bool effect_as_root : 1;

    //! if the return value is ignored
    bool ref_rv : 1;

    //! if the node has undergone "parse initialization"
    bool parse_init : 1;
    bool semantic_policy_bound : 1 = false;
    //! Written only during parse initialization or before publishing a background clone.
    LexicalSemanticPolicy semantic_policy;
    QoreParseAnalysis parse_analysis;

    DLLLOCAL void captureSemanticPolicy();

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context) = 0;

    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const = 0;
};

// these objects will never be copied or referenced therefore they can have
// public destructors - the deref() functions just call "delete this;"
class ParseNoEvalNode : public ParseNode {
private:
    // not implemented
    DLLLOCAL ParseNoEvalNode& operator=(const ParseNoEvalNode&);

    DLLLOCAL virtual int parseInitImpl(QoreValue& val, QoreParseContext& parse_context) = 0;
    DLLLOCAL virtual const QoreTypeInfo* getTypeInfo() const = 0;

protected:
    DLLLOCAL virtual QoreValue evalImpl(bool& needs_deref, ExceptionSink* xsink) const {
        assert(false);
        return QoreValue();
    }

public:
    DLLLOCAL ParseNoEvalNode(const QoreProgramLocation* loc, qore_type_t t) : ParseNode(loc, t, false) {
    }

    DLLLOCAL ParseNoEvalNode(const ParseNoEvalNode& old) : ParseNode(old) {
    }
};

//! Returns the node as a T if its class is exactly T, otherwise nullptr
/** The same result as dynamic_cast<const T*>() for a class T that has no subclasses, compared by the address of
    the type's type_info object: a dynamic_cast searches the class hierarchy with string comparisons of the type
    names, which is too slow for the interpreter's per-instruction paths.

    T must be a library-internal (DLLLOCAL) class without subclasses: objects of such a class are only created in
    this library, so their type_info object is the one that typeid(T) returns here.
*/
template <typename T>
DLLLOCAL inline const T* qore_exact_node_cast(const AbstractQoreNode* n) {
    const T* rv = (n && &typeid(*n) == &typeid(T)) ? static_cast<const T*>(n) : nullptr;
    assert(rv == dynamic_cast<const T*>(n));
    return rv;
}

//! Returns the node as a T if its class is exactly T, otherwise nullptr; see the const overload
template <typename T>
DLLLOCAL inline T* qore_exact_node_cast(AbstractQoreNode* n) {
    T* rv = (n && &typeid(*n) == &typeid(T)) ? static_cast<T*>(n) : nullptr;
    assert(rv == dynamic_cast<T*>(n));
    return rv;
}

#endif
