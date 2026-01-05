/*
    QoreHashNode.cpp

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
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/QoreParseHashNode.h"
#include "qore/intern/QoreParseListNode.h"
#include "qore/intern/QoreNamespaceIntern.h"
#include "qore/intern/ParserSupport.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/typed_hash_decl_private.h"
#include "qore/intern/qore_list_private.h"
#include "qore/intern/qore_type_safe_ref_helper_priv.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <strings.h>

static const char* qore_hash_type_name = "hash";

// hashes should always be empty by the time they are deleted
// because object destructors need to be run...
qore_hash_private::~qore_hash_private() {
    assert(member_list.empty());
    if (hashdecl) {
        typed_hash_decl_private::get(*const_cast<TypedHashDecl*>(hashdecl))->deref();
    }
}

void qore_hash_private::setHashDecl(const TypedHashDecl* hd) {
    if (complexTypeInfo) {
        complexTypeInfo = nullptr;
    }
    if (hashdecl) {
        typed_hash_decl_private::get(*const_cast<TypedHashDecl*>(hashdecl))->deref();
    }
    if (hd) {
        typed_hash_decl_private::get(*hd)->ref();
    }
    hashdecl = hd;
}

QoreValue qore_hash_private::takeKeyValueIntern(const char* key, qore_object_private* obj) {
    assert(key);

    hm_hm_t::iterator i = hm.find(key);

    if (i == hm.end()) {
        return QoreValue();
    }

    qhlist_t::iterator li = i->second;
    hm.erase(i);

    QoreValue rv = (*li)->val;
    internDeleteKey(li);

    if (needs_scan(rv)) {
        if (obj) {
            assert(is_obj);
            obj->incScanCount(-1);
        } else {
            assert(!is_obj);
            incScanCount(-1);
        }
    }

    return rv;
}

QoreListNode* qore_hash_private::getKeys() const {
    QoreListNode* list = new QoreListNode(stringTypeInfo);
    qore_list_private::get(*list)->reserve(member_list.size());

    for (auto& i : member_list) {
        list->push(new QoreStringNode(i->key), nullptr);
    }
    return list;
}

QoreListNode* qore_hash_private::getValues() const {
    ReferenceHolder<QoreListNode> list(new QoreListNode(getValueTypeInfo()), nullptr);
    qore_list_private::get(**list)->reserve(member_list.size());

    for (auto& i : member_list) {
        list->push(i->val.refSelf(), nullptr);
    }
    return list.release();
}

void qore_hash_private::merge(const qore_hash_private& h, ExceptionSink* xsink) {
    for (auto& i : h.member_list) {
        setKeyValue(i->key, i->val.refSelf(), xsink);
    }
}

void qore_hash_private::merge(const qore_hash_private& h, SafeDerefHelper& sdh, ExceptionSink* xsink) {
    for (auto& i : h.member_list) {
        setKeyValue(i->key, i->val.refSelf(), sdh, xsink);
    }
}

int qore_hash_private::getLValue(const char* key, LValueHelper& lvh, bool for_remove, ExceptionSink* xsink) {
    if (checkValid(xsink)) {
        return -1;
    }

    const QoreTypeInfo* memTypeInfo = nullptr;

    if (hashdecl) {
        const HashDeclMemberInfo* m = typed_hash_decl_private::get(*hashdecl)->findMember(key);
        if (!m) {
            xsink->raiseException("INVALID-MEMBER", "'%s' is not a registered member of hashdecl '%s'", key,
                hashdecl->getName());
            lvh.clearPtr();
            return -1;
        }

        memTypeInfo = m->getTypeInfo();
    } else if (complexTypeInfo) {
        memTypeInfo = QoreTypeInfo::getUniqueReturnComplexHash(complexTypeInfo);
    }

    hm_hm_t::const_iterator i = hm.find(key);
    HashMember* m;
    if (i == hm.end()) {
        if (for_remove) {
            return -1;
        }
        m = findCreateMember(key);
    } else {
        m = (*(i->second));
    }

    //printd(5, "qore_hash_private::getLValue() this: %p hd: %p ct: %p key: '%s' type: '%s'\n", this, hashdecl,
    //    complexTypeInfo, key, QoreTypeInfo::getName(memTypeInfo));

    lvh.resetValue(m->val, memTypeInfo);
    //lvh.resetPtr(&m->node, memTypeInfo);
    return 0;
}

int qore_hash_private::parseInitHashInitialization(const QoreProgramLocation* loc, QoreParseContext& parse_context,
        QoreParseListNode* args, QoreValue& arg, int& err) {
    assert(!parse_context.typeInfo);

    if (!args || args->empty()) {
        return -1;
    }

    if (args->size() > 1) {
        parse_error(*loc, "illegal arguments to typed hash initialization; a single hash argument is expected; %d " \
            "arguments supplied instead", (int)args->size());
        err = -1;
        return -1;
    }

    // initialize argument
    QoreParseContextFlagHelper fh(parse_context);
    fh.unsetFlags(PF_RETURN_VALUE_IGNORED);
    if (parse_init_value(args->getReference(0), parse_context) && !err) {
        err = -1;
        return -1;
    }
    arg = args->get(0);

    if (!err && !QoreTypeInfo::parseReturns(parse_context.typeInfo, NT_HASH)) {
        parse_error(*loc, "illegal argument to typed hash initialization; a single hash argument is expected; got " \
            "type '%s' instead", QoreTypeInfo::getName(parse_context.typeInfo));
        if (!err) {
            err = -1;
        }
        return -1;
    }

    return 0;
}

int qore_hash_private::parseInitComplexHashInitialization(const QoreProgramLocation* loc,
        QoreParseContext& parse_context, QoreParseListNode* args) {
    //int lvids = 0;
    //const QoreTypeInfo* argTypeInfo = nullptr;
    QoreValue arg{};
    const QoreTypeInfo* ti = parse_context.typeInfo;
    parse_context.typeInfo = nullptr;
    int err = 0;
    if (!parseInitHashInitialization(loc, parse_context, args, arg, err)) {
        if (parseCheckComplexHashInitialization(loc, ti, parse_context.typeInfo, arg, "initialize", true) && !err) {
            err = -1;
        }
    }
    return err;
}

int qore_hash_private::parseCheckComplexHashInitialization(const QoreProgramLocation* loc,
        const QoreTypeInfo* valueTypeInfo, const QoreTypeInfo* argTypeInfo, QoreValue exp, const char* context_action,
        bool strict_check) {
    const TypedHashDecl* hd = QoreTypeInfo::getUniqueReturnHashDecl(argTypeInfo);
    if (hd) {
        return typed_hash_decl_private::get(*hd)->parseCheckComplexHashAssignment(loc, valueTypeInfo);
    }

    const QoreTypeInfo* vti2 = QoreTypeInfo::getUniqueReturnComplexHash(argTypeInfo);
    if (vti2) {
        if (!QoreTypeInfo::hasType(vti2)) {
            switch (exp.getType()) {
                case NT_HASH: {
                    ConstHashIterator i(exp.get<const QoreHashNode>());
                    while (i.next()) {
                        const QoreTypeInfo* eti = i.get().getFullTypeInfo();
                        if (!QoreTypeInfo::parseAccepts(valueTypeInfo, eti)) {
                            parse_error(*loc, "cannot %s 'hash<string, %s>' from a hash typed with incompatible "
                                "value type '%s' assigned to key '%s'",
                                context_action, QoreTypeInfo::getName(valueTypeInfo), QoreTypeInfo::getName(eti),
                                i.getKey());
                            return -1;
                        }
                    }
                }
            }
        } else if (!QoreTypeInfo::parseAccepts(valueTypeInfo, vti2)) {
            parse_error(*loc, "cannot %s 'hash<string, %s>' from a hash typed with incompatible value type '%s'",
                context_action, QoreTypeInfo::getName(valueTypeInfo),
            QoreTypeInfo::getName(vti2));
            return -1;
        }
    } else {
        return parseCheckTypedAssignment(loc, exp, valueTypeInfo, context_action, strict_check);
    }

    return 0;
}

int qore_hash_private::parseCheckTypedAssignment(const QoreProgramLocation* loc, QoreValue arg,
        const QoreTypeInfo* vti, const char* context_action, bool strict_check) {
    int err = 0;
    switch (arg.getType()) {
        case NT_HASH: {
            ConstHashIterator i(arg.get<const QoreHashNode>());
            while (i.next()) {
                const QoreTypeInfo* kti = i.get().getTypeInfo();
                bool may_not_match = false;
                qore_type_result_e res = QoreTypeInfo::parseAccepts(vti, kti, may_not_match);
                if (res && (res == QTI_IDENT || (!strict_check || !may_not_match)))
                    continue;
                parse_error(*loc, "cannot %s 'hash<string, %s>' from key '%s' of a hash with incompatible value " \
                    "type '%s'", context_action, QoreTypeInfo::getName(vti), i.getKey(), QoreTypeInfo::getName(kti));
                if (!err) {
                    err = -1;
                }
            }
            break;
        }
        case NT_PARSE_HASH: {
            const QoreParseHashNode* phn = arg.get<const QoreParseHashNode>();
            const QoreParseHashNode::nvec_t& keys = phn->getKeys();
            const QoreParseHashNode::tvec_t& vtypes = phn->getValueTypes();
            assert(keys.size() == vtypes.size());

            for (unsigned i = 0; i < vtypes.size(); ++i) {
                const QoreTypeInfo* vti2 = vtypes[i];
                bool may_not_match = false;
                qore_type_result_e res = QoreTypeInfo::parseAccepts(vti, vti2, may_not_match);
                if (res && (res == QTI_IDENT || (!strict_check || !may_not_match)))
                    continue;
                QoreValue kn = keys[i];
                const QoreStringNode* key = kn.getType() == NT_STRING ? kn.get<const QoreStringNode>() : nullptr;
                if (key) {
                    parse_error(*loc, "cannot %s 'hash<string, %s>' from key '%s' of a hash with incompatible " \
                        "value type '%s'", context_action, QoreTypeInfo::getName(vti), key->c_str(),
                        QoreTypeInfo::getName(vti2));
                } else {
                    parse_error(*loc, "cannot %s 'hash<string, %s>' from element value %d/%d of a hash with " \
                        "incompatible value type '%s'", context_action, QoreTypeInfo::getName(vti), (int)(i + 1),
                        (int)vtypes.size(), QoreTypeInfo::getName(vti2));
                }
                if (!err) {
                    err = -1;
                }
            }
            break;
        }
        default:
            break;
    }
    return err;
}

QoreHashNode* qore_hash_private::newComplexHash(const QoreTypeInfo* typeInfo, const QoreParseListNode* args,
        ExceptionSink* xsink) {
    assert(!args || args->empty() || args->size() == 1);

    QoreHashNode* init = nullptr;

    if (args && !args->empty()) {
        RuntimeConfig rc = rc_get_current();
        ValueEvalOptimizedRefHolder a(rc, args->get(0), xsink);
        if (*xsink)
            return nullptr;

        if (a->getType() != NT_HASH) {
            xsink->raiseException("HASH-INIT-ERROR", "typed hash initializer value must be a hash; got type '%s' "
                "instead", a->getTypeName());
            return nullptr;
        }

        init = a.takeReferencedNode<QoreHashNode>();
    }

    return newComplexHashFromHash(typeInfo, init, xsink);
}

// RuntimeConfig-aware version - avoids TLS lookup
QoreHashNode* qore_hash_private::newComplexHash(RuntimeConfig& rc, const QoreTypeInfo* typeInfo,
        const QoreParseListNode* args, ExceptionSink* xsink) {
    assert(!args || args->empty() || args->size() == 1);

    QoreHashNode* init = nullptr;

    if (args && !args->empty()) {
        ValueEvalOptimizedRefHolder a(rc, args->get(0), xsink);
        if (*xsink)
            return nullptr;

        if (a->getType() != NT_HASH) {
            xsink->raiseException("HASH-INIT-ERROR", "typed hash initializer value must be a hash; got type '%s' "
                "instead", a->getTypeName());
            return nullptr;
        }

        init = a.takeReferencedNode<QoreHashNode>();
    }

    return newComplexHashFromHash(typeInfo, init, xsink);
}

QoreHashNode* qore_hash_private::newComplexHashFromHash(const QoreTypeInfo* typeInfo, QoreHashNode* init_hash,
        ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> init(init_hash, xsink);

    // check member types
    if (init) {
        if (!init->is_unique())
            init = init->copy();
        HashIterator i(*init);
        const QoreTypeInfo* vti = QoreTypeInfo::getReturnComplexHashOrNothing(typeInfo);
        assert(vti);
        while (i.next()) {
            // check types
            HashAssignmentHelper hah(i);
            QoreValue qv(hash_assignment_priv::get(hah)->swap(QoreValue()));
            QoreTypeInfo::acceptInputKey(vti, i.getKey(), qv, xsink);
            hash_assignment_priv::get(hah)->swap(qv);
            if (*xsink)
                return nullptr;
        }
    } else {
        init = new QoreHashNode;
    }
    // mark new hash with new type
    assert(init->is_unique());
    init->priv->complexTypeInfo = typeInfo;
    return init.release();
}

int qore_hash_private::checkKey(const char* key, ExceptionSink* xsink) const {
    if (hashdecl && !typed_hash_decl_private::get(*hashdecl)->findMember(key)) {
        xsink->raiseException("INVALID-MEMBER", "error accessing unknown member '%s' of hashdecl '%s'", key,
            hashdecl->getName());
        return -1;
    }

    return 0;
}

QoreValue qore_hash_private::getKeyValueExistence(const char* key, bool& exists, ExceptionSink* xsink) const {
    assert(key);

    if (checkKey(key, xsink)) {
        return QoreValue();
    }

    return getKeyValueExistenceIntern(key, exists);
}

QoreValue qore_hash_private::getKeyValueExistenceIntern(const char* key, bool& exists) const {
    hm_hm_t::const_iterator i = hm.find(key);

    if (i != hm.end()) {
        exists = true;
        return (*i->second)->val;
    }

    exists = false;
    return QoreValue();
}

QoreValue qore_hash_private::getKeyValueIntern(const char* key) const {
    hm_hm_t::const_iterator i = hm.find(key);
    return i != hm.end() ? (*i->second)->val : QoreValue();
}

QoreHashNode::QoreHashNode(bool ne) : AbstractQoreNode(NT_HASH, !ne, ne), priv(new qore_hash_private) {
}

QoreHashNode::QoreHashNode() : AbstractQoreNode(NT_HASH, true, false), priv(new qore_hash_private) {
}

QoreHashNode::QoreHashNode(const TypedHashDecl* hd, ExceptionSink* xsink) : QoreHashNode() {
    priv->setHashDecl(hd);
    typed_hash_decl_private::get(*hd)->initHash(this, nullptr, xsink);
}

QoreHashNode::QoreHashNode(const QoreTypeInfo* valueTypeInfo) : QoreHashNode() {
    if (valueTypeInfo == autoTypeInfo) {
        priv->complexTypeInfo = autoHashTypeInfo;
    } else if (QoreTypeInfo::hasType(valueTypeInfo)) {
        priv->complexTypeInfo = qore_get_complex_hash_type(valueTypeInfo);
    }
}

QoreHashNode::~QoreHashNode() {
    delete priv;
}

QoreHashNode* QoreHashNode::realCopy() const {
    return copy();
}

QoreValue QoreHashNode::getKeyValue(const char* key) const {
    return priv->getKeyValueIntern(key);
}

QoreValue QoreHashNode::getKeyValueExistence(const char* key, bool& exists, ExceptionSink* xsink) const {
    return priv->getKeyValueExistence(key, exists, xsink);
}

QoreValue QoreHashNode::getKeyValueExistence(const char* key, bool& exists) const {
    return priv->getKeyValueExistenceIntern(key, exists);
}

QoreValue QoreHashNode::getKeyValue(const char* key, ExceptionSink* xsink) const {
    bool exists;
    return getKeyValueExistence(key, exists, xsink);
}

QoreValue QoreHashNode::getKeyValueExistence(const QoreString& key, bool& exists, ExceptionSink* xsink) const {
    TempEncodingHelper tmp(key, QCS_DEFAULT, xsink);
    return *xsink ? QoreValue() : getKeyValueExistence(key.c_str(), exists, xsink);
}

QoreValue QoreHashNode::getKeyValue(const QoreString& key, ExceptionSink* xsink) const {
    TempEncodingHelper tmp(key, QCS_DEFAULT, xsink);
    return *xsink ? QoreValue() : getKeyValue(key.c_str(), xsink);
}

// performs a lexical compare, return -1, 0, or 1 if the "this" value is less than, equal, or greater than
// the "val" passed
//DLLLOCAL virtual int compare(const AbstractQoreNode* val) const;
// the type passed must always be equal to the current type
bool QoreHashNode::is_equal_soft(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    if (!v || v->getType() != NT_HASH) {
        return false;
    }

    return !compareSoft(reinterpret_cast<const QoreHashNode*>(v), xsink);
}

bool QoreHashNode::is_equal_hard(const AbstractQoreNode* v, ExceptionSink* xsink) const {
    if (!v || v->getType() != NT_HASH) {
        return false;
    }

    return !compareHard(reinterpret_cast<const QoreHashNode*>(v), xsink);
}

const char* QoreHashNode::getTypeName() const {
    return qore_hash_type_name;
}

const char* QoreHashNode::getFirstKey() const  {
    return priv->getFirstKey();
}

const char* QoreHashNode::getLastKey() const {
    return priv->getLastKey();
}

// deprecated
int64 QoreHashNode::getKeyAsBigInt(const char* key, bool &found) const {
    return priv->getKeyAsBigInt(key, found);
}

// deprecated
bool QoreHashNode::getKeyAsBool(const char* key, bool &found) const {
    return priv->getKeyAsBool(key, found);
}

void QoreHashNode::deleteKey(const QoreString* key, ExceptionSink* xsink) {
    assert(reference_count() == 1);
    TempEncodingHelper tmp(key, QCS_DEFAULT, xsink);
    if (*xsink) {
        return;
    }

    priv->deleteKey(tmp->c_str(), xsink);
}

QoreValue QoreHashNode::takeKeyValue(const char* key) {
    assert(reference_count() == 1);
    return priv->takeKeyValueIntern(key);
}

void QoreHashNode::removeKey(const QoreString* key, ExceptionSink* xsink) {
   assert(reference_count() == 1);
   TempEncodingHelper tmp(key, QCS_DEFAULT, xsink);
   if (*xsink)
      return;

   priv->removeKey(tmp->c_str(), xsink);
}

int QoreHashNode::setKeyValue(const char* key, QoreValue value, ExceptionSink* xsink) {
    assert(reference_count() == 1);
    hash_assignment_priv ha(*priv, key);
    ha.assign(value, xsink);
    return xsink && *xsink ? -1 : 0;
}

int QoreHashNode::setKeyValue(const QoreString& key, QoreValue value, ExceptionSink* xsink) {
    assert(xsink);
    TempEncodingHelper tmp(key, QCS_DEFAULT, xsink);
    if (*xsink) {
        value.discard(xsink);
        return -1;
    }

    return setKeyValue(tmp->c_str(), value, xsink);
}

QoreValue& QoreHashNode::getKeyValueReference(const char* key) {
    assert(reference_count() == 1);
    return priv->getValueRef(key);
}

// retrieve keys in order they were inserted
QoreListNode* QoreHashNode::getKeys() const {
   return priv->getKeys();
}

// retrieve values in order they were inserted
QoreListNode* QoreHashNode::getValues() const {
   return priv->getValues();
}

// adds all elements (and references them) from the hash passed, leaves the
// hash passed alone
// order is maintained
void QoreHashNode::merge(const QoreHashNode* h, ExceptionSink* xsink) {
    priv->merge(*h->priv, xsink);
}

// returns the same order
QoreHashNode* QoreHashNode::copy() const {
    return priv->copy();
}

QoreHashNode* QoreHashNode::hashRefSelf() const {
    ref();
    return const_cast<QoreHashNode*>(this);
}

// returns a hash with the same order
QoreValue QoreHashNode::evalImpl(RuntimeConfig& rc, bool& needs_deref, ExceptionSink* xsink) const {
    assert(needs_deref);
    if (value) {
        needs_deref = false;
        return const_cast<QoreHashNode*>(this);
    }

    return priv->evalImpl(xsink);
}

// does a "soft" compare (values of different types are converted if necessary and then compared)
// 0 = equal, 1 = not equal
bool QoreHashNode::compareSoft(const QoreHashNode* h, ExceptionSink* xsink) const {
    if (h->priv->size() != priv->size())
        return 1;

    ConstHashIterator hi(this);
    while (hi.next()) {
        hm_hm_t::const_iterator j = h->priv->hm.find(hi.getKey());
        if (j == h->priv->hm.end())
            return 1;

        if (!hi.get().isEqualSoft((*j->second)->val, xsink)) {
            return 1;
        }
    }
    return 0;
}

// does a "hard" compare (types must be exactly the same)
// 0 = equal, 1 = not equal
bool QoreHashNode::compareHard(const QoreHashNode* h, ExceptionSink* xsink) const {
    if (h->priv->size() != priv->size())
        return 1;

    ConstHashIterator hi(this);
    while (hi.next()) {
        hm_hm_t::const_iterator j = h->priv->hm.find(hi.getKey());
        if (j == h->priv->hm.end())
            return 1;

        if (!hi.get().isEqualHard((*j->second)->val)) {
            return 1;
        }
    }
    return 0;
}

bool QoreHashNode::derefImpl(ExceptionSink* xsink) {
    priv->derefImpl(xsink);
    weakDeref();
    return false;
}

void QoreHashNode::clear(ExceptionSink* xsink, bool reverse) {
    assert(is_unique());
    priv->clear(xsink, reverse);
}

void QoreHashNode::deleteKey(const char* key, ExceptionSink* xsink) {
    assert(reference_count() == 1);
    priv->deleteKey(key, xsink);
}

void QoreHashNode::removeKey(const char* key, ExceptionSink* xsink) {
    assert(reference_count() == 1);
    return priv->removeKey(key, xsink);
}

size_t QoreHashNode::size() const {
    return priv->size();
}

bool QoreHashNode::empty() const {
    return priv->empty();
}

bool QoreHashNode::existsKey(const char* key) const {
    return priv->existsKey(key);
}

bool QoreHashNode::existsKeyValue(const char* key) const {
    return priv->existsKeyValue(key);
}

void QoreHashNode::clearNeedsEval() {
    value = true;
    needs_eval_flag = false;
}

void QoreHashNode::setNeedsEval() {
    value = false;
    needs_eval_flag = true;
}

int QoreHashNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    QoreContainerHelper cch(this);
    if (!cch) {
        str.sprintf("{ERROR: recursive reference to hash %p}", this);
        return 0;
    }

    if (foff == FMT_YAML_SHORT) {
        str.concat('{');
        ConstHashIterator hi(this);
        while (hi.next()) {
            str.sprintf("%s: ", hi.getKey());
            if (hi.get().getAsString(str, foff, xsink))
                return -1;
            if (!hi.last())
                str.concat(", ");
        }
        str.concat('}');
        return 0;
    }

    if (foff <= FMT_YAML_LONG) {
        // Multi-line YAML format; indent encoded as FMT_YAML_LONG - foff
        int indent = FMT_YAML_LONG - foff;
        if (!size()) {
            if (indent > 0)
                str.addch(' ', indent);
            str.concat("{}");
            return 0;
        }
        ConstHashIterator hi(this);
        while (hi.next()) {
            if (indent > 0)
                str.addch(' ', indent);
            str.sprintf("%s:", hi.getKey());
            QoreValue v = hi.get();
            qore_type_t vtype = v.getType();
            if (vtype == NT_HASH || vtype == NT_LIST) {
                // Complex value: newline then indented content
                str.concat('\n');
                if (v.getAsString(str, foff - 2, xsink))
                    return -1;
            } else {
                // Simple value: space then value in YAML short format
                str.concat(' ');
                if (v.getAsString(str, FMT_YAML_SHORT, xsink))
                    return -1;
            }
            if (!hi.last())
                str.concat('\n');
        }
        return 0;
    }

    if (!size()) {
        str.concat(&EmptyHashString);
        return 0;
    }
    str.concat("hash: (");

    if (foff != FMT_NONE) {
        size_t elements = size();
        str.sprintf("%lu member%s)\n", elements, elements == 1 ? "" : "s");
    }

    ConstHashIterator hi(this);
    while (hi.next()) {
        if (foff != FMT_NONE)
            str.addch(' ', foff + 2);

        str.sprintf("%s : ", hi.getKey());

        if (hi.get().getAsString(str, foff != FMT_NONE ? foff + 2 : foff, xsink))
            return -1;

        if (!hi.last()) {
            if (foff != FMT_NONE)
                str.concat('\n');
            else
                str.concat(", ");
        }
    }

    if (foff == FMT_NONE)
        str.concat(')');

    return 0;
}

QoreString* QoreHashNode::getAsString(bool &del, int foff, ExceptionSink* xsink) const {
    del = false;
    size_t elements = size();
    if (!elements && foff != FMT_YAML_SHORT && foff > FMT_YAML_LONG)
        return &EmptyHashString;

    TempString rv(new QoreString);
    if (getAsString(*(*rv), foff, xsink))
        return nullptr;

    del = true;
    return rv.release();
}

QoreHashNode* QoreHashNode::getSlice(const QoreListNode* value_list, ExceptionSink* xsink) const {
    ReferenceHolder<QoreHashNode> rv(priv->getCopy(), xsink);

    ConstListIterator li(value_list);
    while (li.next()) {
        QoreStringValueHelper key(li.getValue(), QCS_DEFAULT, xsink);
        if (*xsink)
            return nullptr;

        bool exists;
        QoreValue v = getKeyValueExistence(key->c_str(), exists, xsink);
        if (*xsink)
            return nullptr;
        if (!exists)
            continue;
        rv->setKeyValue(key->c_str(), v.refSelf(), xsink);
        if (*xsink)
            return nullptr;
    }
    return rv.release();
}

int QoreHashNode::parseInit(QoreValue& val, QoreParseContext& parse_context) {
    parse_context.typeInfo = priv->getTypeInfo();
    return 0;
}

bool QoreHashNode::getAsBoolImpl() const {
   // check if we should do perl-style boolean evaluation
   if (runtime_check_parse_option(PO_STRICT_BOOLEAN_EVAL))
      return false;
   return !empty();
}

const TypedHashDecl* QoreHashNode::getHashDecl() const {
   return priv->getHashDecl();
}

const QoreTypeInfo* QoreHashNode::getValueTypeInfo() const {
   return priv->getValueTypeInfo();
}

const QoreTypeInfo* QoreHashNode::getTypeInfo() const {
   return priv->getTypeInfo();
}

void QoreHashNode::weakRef() {
    priv->weakRef();
}

bool QoreHashNode::weakDeref() {
    bool b = priv->weakDeref();
    if (b) {
        delete this;
    }
    return b;
}

HashIterator::HashIterator(QoreHashNode* qh) : h(qh), priv(new qhi_priv()) {
}

HashIterator::HashIterator(QoreHashNode& qh) : h(&qh), priv(new qhi_priv()) {
}

HashIterator::~HashIterator() {
    delete priv;
}

QoreHashNode* HashIterator::getHash() const {
    return h;
}

QoreValue HashIterator::getReferenced() const {
    return !priv->valid() ? QoreValue() : (*(priv->i))->val.refSelf();
}

QoreString* HashIterator::getKeyString() const {
    return !priv->valid() ? nullptr : new QoreString((*(priv->i))->key);
}

QoreStringNode* HashIterator::getKeyStringNode() const {
    return !priv->valid() ? nullptr : new QoreStringNode((*(priv->i))->key);
}

bool HashIterator::next() {
    return h ? priv->next(h->priv->member_list) : false;
}

bool HashIterator::prev() {
    return h ? priv->prev(h->priv->member_list) : false;
}

const char* HashIterator::getKey() const {
    if (!priv->valid()) {
        return nullptr;
    }

    return (*(priv->i))->key.c_str();
}

QoreValue HashIterator::get() const {
    if (!priv->valid()) {
        return QoreValue();
    }

    return (*(priv->i))->val;
}

const QoreTypeInfo* HashIterator::getTypeInfo() const {
    if (!priv->valid()) {
        return nullptr;
    }

    return (*(priv->i))->val.getTypeInfo();
}

void HashIterator::deleteKey(ExceptionSink* xsink) {
    if (!priv->valid()) {
        return;
    }

    assert(h->is_unique());

    if (needs_scan((*(priv->i))->val)) {
        h->priv->incScanCount(-1);
    }

    (*(priv->i))->val.discard(xsink);

    qhlist_t::iterator ni = priv->i;
    priv->prev(h->priv->member_list);

    hm_hm_t::iterator i = h->priv->hm.find((*ni)->key.c_str());
    assert(i != h->priv->hm.end());
    h->priv->hm.erase(i);
    h->priv->internDeleteKey(ni);
}

int HashIterator::assign(QoreValue val, ExceptionSink* xsink) {
    if (!priv->valid()) {
        val.discard(xsink);
        return -1;
    }

    assert(h->is_unique());

    hash_assignment_priv ha(*h->priv, *(priv->i));
    ha.assign(val, xsink);

    return *xsink ? -1 : 0;
}

QoreValue HashIterator::removeKeyValue() {
    if (!priv->valid())
        return QoreValue();

    assert(h->is_unique());

    QoreValue rv = (*(priv->i))->val;

    if (needs_scan(rv)) {
        h->priv->incScanCount(-1);
    }

    qhlist_t::iterator ni = priv->i;
    priv->prev(h->priv->member_list);

    hm_hm_t::iterator i = h->priv->hm.find((*ni)->key.c_str());
    assert(i != h->priv->hm.end());
    h->priv->hm.erase(i);
    h->priv->internDeleteKey(ni);

    return rv;
}

bool HashIterator::last() const {
    if (!priv->valid())
        return false;

    qhlist_t::const_iterator ni = priv->i;
    ++ni;
    return ni == h->priv->member_list.end() ? true : false;
}

bool HashIterator::first() const {
    if (!priv->valid())
        return false;

    return priv->i == h->priv->member_list.begin();
}

bool HashIterator::empty() const {
    return h->empty();
}

bool HashIterator::valid() const {
    return priv->valid();
}

ReverseHashIterator::ReverseHashIterator(QoreHashNode* h) : HashIterator(h) {
}

ReverseHashIterator::ReverseHashIterator(QoreHashNode& h) : HashIterator(h) {
}

ReverseHashIterator::~ReverseHashIterator() {
}

bool ReverseHashIterator::last() const {
   return HashIterator::first();
}

bool ReverseHashIterator::first() const {
   return HashIterator::last();
}

bool ReverseHashIterator::next() {
   return HashIterator::prev();
}

bool ReverseHashIterator::prev() {
   return HashIterator::next();
}

ConstHashIterator::ConstHashIterator(const QoreHashNode* qh) : h(qh), priv(new qhi_priv()) {
}

ConstHashIterator::ConstHashIterator(const QoreHashNode& qh) : h(&qh), priv(new qhi_priv()) {
}

ConstHashIterator::ConstHashIterator(const ConstHashIterator& old) : h(old.h->hashRefSelf()),
        priv(new qhi_priv(*old.priv)) {
}

ConstHashIterator::~ConstHashIterator() {
   delete priv;
}

const QoreHashNode* ConstHashIterator::getHash() const {
   return h;
}

QoreValue ConstHashIterator::getReferenced() const {
    return !priv->valid() ? QoreValue() : (*(priv->i))->val.refSelf();
}

QoreString* ConstHashIterator::getKeyString() const {
   return !priv->valid() ? nullptr : new QoreString((*(priv->i))->key);
}

QoreStringNode* ConstHashIterator::getKeyStringNode() const {
    return !priv->valid() ? nullptr : new QoreStringNode((*(priv->i))->key);
}

bool ConstHashIterator::next() {
   return h ? priv->next(h->priv->member_list) : false;
}

bool ConstHashIterator::prev() {
   return h ? priv->prev(h->priv->member_list) : false;
}

const char* ConstHashIterator::getKey() const {
   if (!priv->valid())
      return 0;
   return (*(priv->i))->key.c_str();
}

const QoreValue ConstHashIterator::get() const {
    if (!priv->valid())
        return QoreValue();

    return (*(priv->i))->val;
}

const QoreTypeInfo* ConstHashIterator::getTypeInfo() const {
    if (!priv->valid())
        return nullptr;

    return (*(priv->i))->val.getTypeInfo();
}

bool ConstHashIterator::last() const {
   if (!priv->valid())
      return false;

   qhlist_t::const_iterator ni = priv->i;
   ++ni;
   return ni == h->priv->member_list.end() ? true : false;
}

bool ConstHashIterator::first() const {
   if (!priv->valid())
      return false;

   return priv->i == h->priv->member_list.begin();
}

bool ConstHashIterator::empty() const {
   return h->empty();
}

bool ConstHashIterator::valid() const {
   return priv->valid();
}

void ConstHashIterator::reset() {
   priv->reset();
}

ReverseConstHashIterator::ReverseConstHashIterator(const QoreHashNode* h) : ConstHashIterator(h) {
}

ReverseConstHashIterator::ReverseConstHashIterator(const QoreHashNode& h) : ConstHashIterator(h) {
}

ReverseConstHashIterator::~ReverseConstHashIterator() {
}

bool ReverseConstHashIterator::last() const {
    return ConstHashIterator::first();
}

bool ReverseConstHashIterator::first() const {
    return ConstHashIterator::last();
}

bool ReverseConstHashIterator::next() {
    return ConstHashIterator::prev();
}

bool ReverseConstHashIterator::prev() {
    return ConstHashIterator::next();
}

hash_assignment_priv::hash_assignment_priv(qore_hash_private& n_h, const char* key, bool must_already_exist,
        qore_object_private* obj) : h(n_h), om(must_already_exist ? h.findMember(key)
            : h.findCreateMember(key)), o(obj) {
}

hash_assignment_priv::hash_assignment_priv(QoreHashNode& n_h, const char* key, bool must_already_exist)
        : h(*n_h.priv), om(must_already_exist ? h.findMember(key) : h.findCreateMember(key)) {
}

hash_assignment_priv::hash_assignment_priv(QoreHashNode& n_h, const std::string& key, bool must_already_exist)
        : h(*n_h.priv), om(must_already_exist ? h.findMember(key.c_str()) : h.findCreateMember(key.c_str())) {
}

hash_assignment_priv::hash_assignment_priv(ExceptionSink* xsink, QoreHashNode& n_h, const QoreString& key,
        bool must_already_exist) : h(*n_h.priv) {
    TempEncodingHelper k(key, QCS_DEFAULT, xsink);
    if (*xsink)
        return;

    om = must_already_exist ? h.findMember(k->c_str()) : h.findCreateMember(k->c_str());
}

hash_assignment_priv::hash_assignment_priv(ExceptionSink* xsink, QoreHashNode& n_h, const QoreString* key,
        bool must_already_exist) : h(*n_h.priv) {
    TempEncodingHelper k(key, QCS_DEFAULT, xsink);
    if (*xsink)
        return;

    om = must_already_exist ? h.findMember(k->c_str()) : h.findCreateMember(k->c_str());
}

void hash_assignment_priv::reassign(const char* key, bool must_already_exist) {
    om = must_already_exist ? h.findMember(key) : h.findCreateMember(key);
}

const char* hash_assignment_priv::getKey() const {
    return om ? om->key.c_str() : nullptr;
}

QoreValue hash_assignment_priv::swapImpl(QoreValue v) {
    assert(om);
    QoreValue old = om->val;
    om->val = v;

    bool before = needs_scan(old);
    bool after = needs_scan(v);
    if (before) {
        if (!after) {
            if (o)
                o->incScanCount(-1);
            else
                h.incScanCount(-1);
        }
    } else if (after) {
        if (o)
            o->incScanCount(1);
        else
            h.incScanCount(1);
    }

    return old;
}

void hash_assignment_priv::assign(QoreValue v, ExceptionSink* xsink) {
    ValueHolder val(v, xsink);
    if (h.hashdecl) {
        if (typed_hash_decl_private::get(*h.hashdecl)->runtimeAssignKey(om->key.c_str(), val, xsink)) {
            return;
        }
    } else if (h.complexTypeInfo) {
        QoreTypeInfo::acceptInputKey(QoreTypeInfo::getUniqueReturnComplexHash(h.complexTypeInfo), om->key.c_str(),
            *val, xsink);
        // allow this function to be called with xsink = nullptr, otherwise the *xsink will assert
        // anyway if there is an exception it would dump core when the exception is raised
        if (xsink && *xsink) {
            return;
        }
    } else {
        // perform type stripping
        ValueHolder v(val.release(), xsink);
        val = copy_strip_complex_types(*v);
    }

    swapImpl(val.release()).discard(xsink);
}

void hash_assignment_priv::assign(QoreValue v, SafeDerefHelper& sdh, ExceptionSink* xsink) {
    ValueHolder val(v, xsink);
    if (h.hashdecl) {
        if (typed_hash_decl_private::get(*h.hashdecl)->runtimeAssignKey(om->key.c_str(), val, xsink)) {
            return;
        }
    } else if (h.complexTypeInfo) {
        QoreTypeInfo::acceptInputKey(QoreTypeInfo::getUniqueReturnComplexHash(h.complexTypeInfo), om->key.c_str(),
            *val, xsink);
        // allow this function to be called with xsink = nullptr, otherwise the *xsink will assert
        // anyway if there is an exception it would dump core when the exception is raised
        if (xsink && *xsink) {
            return;
        }
    } else {
        // perform type stripping
        ValueHolder v(val.release(), xsink);
        val = copy_strip_complex_types(*v);
    }

    sdh.deref(swapImpl(val.release()));
}

QoreValue hash_assignment_priv::getImpl() const {
    return om->val;
}

HashAssignmentHelper::HashAssignmentHelper(QoreHashNode& h, const char* key, bool must_already_exist)
        : priv(new hash_assignment_priv(*h.priv, key, must_already_exist)) {
}

HashAssignmentHelper::HashAssignmentHelper(QoreHashNode& h, const std::string& key, bool must_already_exist)
        : priv(new hash_assignment_priv(*h.priv, key.c_str(), must_already_exist)) {
}

HashAssignmentHelper::HashAssignmentHelper(ExceptionSink* xsink, QoreHashNode& h, const QoreString& key,
        bool must_already_exist) : priv(0) {
    TempEncodingHelper k(key, QCS_DEFAULT, xsink);
    if (*xsink)
        return;

    priv = new hash_assignment_priv(*h.priv, k->c_str(), must_already_exist);
}

HashAssignmentHelper::HashAssignmentHelper(ExceptionSink* xsink, QoreHashNode& h, const QoreString* key,
        bool must_already_exist) : priv(0) {
    TempEncodingHelper k(key, QCS_DEFAULT, xsink);
    if (*xsink)
        return;

    priv = new hash_assignment_priv(*h.priv, k->c_str(), must_already_exist);
}

HashAssignmentHelper::HashAssignmentHelper(HashIterator &hi)
        : priv(new hash_assignment_priv(*hi.h->priv, *(hi.priv->i))) {
}

HashAssignmentHelper::~HashAssignmentHelper() {
    delete priv;
}

const char* HashAssignmentHelper::getKey() const {
    return priv->getKey();
}

void HashAssignmentHelper::reassign(const char* key, bool must_already_exist) {
    priv->reassign(key);
}

void HashAssignmentHelper::reassign(const std::string& key, bool must_already_exist) {
    priv->reassign(key.c_str());
}

HashAssignmentHelper::operator bool() const {
    return priv;
}

void HashAssignmentHelper::assign(QoreValue v, ExceptionSink* xsink) {
    assert(priv);
    priv->assign(v, xsink);
}

QoreValue HashAssignmentHelper::swap(QoreValue v, ExceptionSink* xsink) {
    assert(priv);
    return priv->swap(v);
}

QoreValue HashAssignmentHelper::get() const {
    assert(priv);
    return **priv;
}

QoreValue HashAssignmentHelper::operator*() const {
    assert(priv);
    return (**priv);
}

void QoreParseHashNode::doDuplicateWarning(const QoreProgramLocation* newloc, const char* key) {
    qore_program_private::makeParseWarning(getProgram(), *newloc, QP_WARN_DUPLICATE_HASH_KEY, "DUPLICATE-HASH-KEY",
        "hash key '%s' has already been given in this hash; the value given in the last occurrence will be " \
        "assigned to the hash; to avoid seeing this warning, remove the extraneous key definitions or turn off the " \
        "warning by using '%%disable-warning duplicate-hash-key' in your code", key);
}

int QoreParseHashNode::getAsString(QoreString& str, int foff, ExceptionSink* xsink) const {
    str.sprintf("expression hash with %d member%s", (int)keys.size(), keys.size() == 1 ? "" : "s");
    return 0;
}

QoreString* QoreParseHashNode::getAsString(bool& del, int foff, ExceptionSink* xsink) const {
    del = true;
    QoreString* rv = new QoreString;
    getAsString(*rv, foff, xsink);
    return rv;
}

const char* QoreParseHashNode::getTypeName() const {
    return "hash";
}
