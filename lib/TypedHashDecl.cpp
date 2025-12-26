/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    TypedHashDecl.cpp

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

#include "qore/intern/typed_hash_decl_private.h"
#include "qore/intern/qore_program_private.h"
#include "qore/intern/QoreParseHashNode.h"
#include "qore/intern/QoreHashNodeIntern.h"

bool HashDeclMemberInfo::equal(const HashDeclMemberInfo& other) const {
    return QoreTypeInfo::equal(typeInfo, other.typeInfo);
}

int HashDeclMemberInfo::parseInit(const char* name, bool priv) {
    int err = 0;
    if (!typeInfo) {
        typeInfo = QoreParseTypeInfo::resolveAndDelete(parseTypeInfo, loc, err);
        parseTypeInfo = nullptr;
    }
#ifdef DEBUG
    else assert(!parseTypeInfo);
#endif

    if (!exp) {
        return err;
    }

    QoreParseContext parse_context;
    if (parse_init_value(exp, parse_context) && !err) {
        err = -1;
    }
    const QoreTypeInfo* argTypeInfo = parse_context.typeInfo;
    if (parse_context.lvids) {
        parse_error(*loc, "illegal local variable declaration in initialization expression for hashdecl member '%s'",
            name);
        while (parse_context.lvids--) {
            pop_local_var();
        }
        if (!err) {
            err = -1;
        }
    }
    // throw a type exception only if parse exceptions are enabled
    if (!QoreTypeInfo::parseAccepts(typeInfo, argTypeInfo)) {
        if (getProgram()->getParseExceptionSink()) {
            QoreStringNode* desc = new QoreStringNode("initialization expression for ");
            desc->sprintf("hashdecl member '%s' returns ", name);
            QoreTypeInfo::getThisType(argTypeInfo, *desc);
            desc->concat(", but the member was declared as ");
            QoreTypeInfo::getThisType(typeInfo, *desc);
            qore_program_private::makeParseException(getProgram(), *loc, "PARSE-TYPE-ERROR", desc);
        }
        if (!err) {
            err = -1;
        }
    }
    return err;
}

// NOTE: the new namespace will be set manually after this call
typed_hash_decl_private::typed_hash_decl_private(const typed_hash_decl_private& old, TypedHashDecl* thd) :
        loc(old.loc),
        name(old.name),
        path(old.path),
        thd(thd),
        from_module(old.from_module),
        orig(old.orig),
        typeInfo(new QoreHashDeclTypeInfo(thd, name.c_str(), path.c_str())),
        orNothingTypeInfo(new QoreHashDeclOrNothingTypeInfo(thd, name.c_str(), path.c_str())),
        pub(false),
        sys(old.sys),
        parse_init_done(old.parse_init_done) {
    // copy member list
    for (auto& i : old.members.member_list) {
        members.addNoCheck(strdup(i.first), i.second ? new HashDeclMemberInfo(*i.second) : nullptr);
    }
}

int typed_hash_decl_private::parseInitHashDeclInitialization(const QoreProgramLocation* loc,
        QoreParseContext& parse_context, QoreParseListNode* args, bool& runtime_check) const {
    runtime_check = false;

    parse_context.typeInfo = nullptr;
    QoreValue arg;
    int err = 0;
    if (!qore_hash_private::parseInitHashInitialization(loc, parse_context, args, arg, err)) {
        if (parseCheckHashDeclInitialization(loc, parse_context.typeInfo, arg, "initializer value", runtime_check,
            true) && !err) {
            err = -1;
        }
    }
    return err;
}

int typed_hash_decl_private::parseCheckHashDeclInitialization(const QoreProgramLocation* loc,
        const QoreTypeInfo* expTypeInfo, QoreValue exp, const char* context_action, bool& runtime_check,
        bool strict_check) const {
    const TypedHashDecl* hd2 = QoreTypeInfo::getUniqueReturnHashDecl(expTypeInfo);
    if (hd2) {
        return parseCheckHashDeclAssignment(loc, *hd2->priv, context_action, runtime_check, strict_check);
    }

    return parseCheckHashDeclAssignment(loc, exp, context_action, runtime_check, strict_check);
}

// see if the assignment is valid
int typed_hash_decl_private::parseCheckHashDeclAssignment(const QoreProgramLocation* loc,
        const typed_hash_decl_private& hd, const char* context, bool& needs_runtime_check, bool strict_check) const {
    int err = 0;
    unsigned possible_matches = 0;
    for (auto& i : hd.members.member_list) {
        HashDeclMemberInfo* m = members.find(i.first);
        if (!m) {
            if (!strict_check) {
                continue;
            }
            parse_error(*loc, "hashdecl '%s' cannot be initialized from %s with hashdecl '%s' due to key '%s' " \
                "present in hashdecl '%s' but not in the target hashdecl '%s'", name.c_str(), context,
                hd.name.c_str(), i.first, hd.name.c_str(), name.c_str());
            if (!err) {
                err = -1;
            }
        } else {
            bool may_not_match = false;
            qore_type_result_e res = QoreTypeInfo::parseAccepts(m->getTypeInfo(), i.second->getTypeInfo(),
                may_not_match);

            if (res && (res == QTI_IDENT || !strict_check || !may_not_match)) {
                ++possible_matches;
                continue;
            }

            if ((res == QTI_WILDCARD || res == QTI_AMBIGUOUS || res == QTI_NEAR) && may_not_match) {
                parse_error(*loc, "hashdecl '%s' initializer value for key '%s' from hashdecl '%s' from %s has " \
                    "incompatible type '%s'; expecting '%s'; types may not be compatible at runtime; use " \
                    "cast<hash<%s>>() to force a runtime check", name.c_str(), i.first, hd.name.c_str(), context,
                    QoreTypeInfo::getName(i.second->getTypeInfo()), QoreTypeInfo::getName(m->getTypeInfo()),
                    name.c_str());
                if (!err) {
                    err = -1;
                }
            } else {
                if (strict_check) {
                    parse_error(*loc, "hashdecl '%s' initializer value for key '%s' from hashdecl '%s' from %s has " \
                        "incompatible type '%s'; expecting '%s'", name.c_str(), i.first, hd.name.c_str(), context,
                        QoreTypeInfo::getName(i.second->getTypeInfo()), QoreTypeInfo::getName(m->getTypeInfo()));
                    if (!err) {
                        err = -1;
                    }
                }
            }
        }
    }
    if (!err && !possible_matches) {
        parse_error(*loc, "hashdecl '%s' cannot be assigned from hashdecl '%s' as there are no common keys with " \
            "compatible types", name.c_str(), hd.name.c_str());
        err = -1;
    }
    return err;
}

// see if the assignment is valid
int typed_hash_decl_private::parseCheckHashDeclAssignment(const QoreProgramLocation* loc, QoreValue n,
        const char* context, bool& runtime_check, bool strict_check) const {
    assert(!runtime_check);

    int err = 0;

    switch (n.getType()) {
        case NT_HASH: {
            ConstHashIterator i(n.get<const QoreHashNode>());
            while (i.next()) {
                HashDeclMemberInfo* m = members.find(i.getKey());
                if (!m) {
                    parse_error(*loc, "hashdecl '%s' initializer value from %s contains unknown key '%s'",
                        name.c_str(), context, i.getKey());
                    err = -1;
                } else {
                    const QoreTypeInfo* kti = i.getTypeInfo();
                    bool may_not_match = false;
                    qore_type_result_e res = QoreTypeInfo::parseAccepts(m->getTypeInfo(), kti, may_not_match);
                    if (may_not_match && !runtime_check)
                        runtime_check = true;
                    if (res && (res == QTI_IDENT || (!strict_check || !may_not_match)))
                        continue;
                    parse_error(*loc, "hashdecl '%s' initializer value from %s cannot be assigned from key '%s' " \
                        "with incompatible value type '%s'; expecting '%s'", name.c_str(), context, i.getKey(),
                        QoreTypeInfo::getName(kti), QoreTypeInfo::getName(m->getTypeInfo()));
                    if (!err) {
                        err = -1;
                    }
                }
            }
            break;
        }
        case NT_PARSE_HASH: {
            const QoreParseHashNode* phn = n.get<const QoreParseHashNode>();
            // do not check or raise errors if the parse hash node failed in parsing already
            if (!phn->hasParseError()) {
                const QoreParseHashNode::nvec_t& keys = phn->getKeys();
                const QoreParseHashNode::tvec_t& vtypes = phn->getValueTypes();
                assert(keys.size() == vtypes.size());

                for (unsigned i = 0; i < keys.size(); ++i) {
                    // check key
                    QoreValue kn = keys[i];
                    const QoreStringNode* key = kn.getType() == NT_STRING ? kn.get<const QoreStringNode>() : nullptr;
                    if (key) {
                        const HashDeclMemberInfo* m = members.find(key->c_str());
                        if (!m) {
                            parse_error(*loc, "hashdecl '%s' hash initializer value from %s contains unknown key '%s'",
                                name.c_str(), context, key->c_str());
                            if (!err) {
                                err = -1;
                            }
                            continue;
                        }
                        // check value type
                        const QoreTypeInfo* vti = vtypes[i];
                        bool may_not_match = false;
                        qore_type_result_e res = QoreTypeInfo::parseAccepts(m->getTypeInfo(), vti, may_not_match);
                        if (may_not_match && !runtime_check)
                            runtime_check = true;
                        if (res && (res == QTI_IDENT || (!strict_check || !may_not_match)))
                            continue;

                        if ((res == QTI_WILDCARD || res == QTI_AMBIGUOUS || res == QTI_NEAR) && may_not_match) {
                            parse_error(*loc, "hashdecl '%s' initializer value for key '%s' from %s has incompatible " \
                                "type '%s'; expecting '%s'; types may not be compatible at runtime; use " \
                                "cast<hash<%s>>() to force a runtime check", name.c_str(), key->c_str(), context,
                                QoreTypeInfo::getName(vti), QoreTypeInfo::getName(m->getTypeInfo()), name.c_str());
                        } else {
                            parse_error(*loc, "hashdecl '%s' initializer value for key '%s' from %s has incompatible " \
                                "type '%s'; expecting '%s'; types may not be compatible at runtime", name.c_str(),
                                key->c_str(), context, QoreTypeInfo::getName(vti),
                                QoreTypeInfo::getName(m->getTypeInfo()), name.c_str());
                        }
                        if (!err) {
                            err = -1;
                        }
                    } else if (!runtime_check) {
                        runtime_check = true;
                    }
                }
            }
            break;
        }
        default:
            runtime_check = true;
            break;
    }
    return err;
}

int typed_hash_decl_private::parseCheckComplexHashAssignment(const QoreProgramLocation* loc,
        const QoreTypeInfo* vti) const {
    assert(QoreTypeInfo::hasType(vti));
    int err = 0;
    for (auto& i : members.member_list) {
        if (!QoreTypeInfo::parseAccepts(vti, i.second->getTypeInfo())) {
            parse_error(*loc, "cannot initialize a hash<string, %s> value from hashdecl '%s' due to member '%s' " \
                "with incompatible type '%s'", QoreTypeInfo::getName(vti), name.c_str(), i.first,
                QoreTypeInfo::getName(i.second->getTypeInfo()));
            if (!err) {
                err = -1;
            }
        }
    }
    return err;
}

int typed_hash_decl_private::parseCheckMemberAccess(const QoreProgramLocation* loc, const char* mem,
        const QoreTypeInfo*& memberTypeInfo, int pflag) const {
    const_cast<typed_hash_decl_private*>(this)->parseInit();
    const HashDeclMemberInfo* m = members.find(mem);

    if (!m) {
        parse_error(*loc, "illegal access to unknown member '%s' in hashdecl '%s'", mem, name.c_str());
        return -1;
    }

    memberTypeInfo = m->getTypeInfo();
    return 0;
}

QoreHashNode* typed_hash_decl_private::newHash(const QoreParseListNode* args, bool runtime_check,
        ExceptionSink* xsink) const {
    assert(!args || args->empty() || args->size() == 1);
    ValueEvalOptimizedRefHolder a(args && !args->empty() ? args->get(0) : QoreValue(), xsink);
    if (*xsink)
        return nullptr;

    const QoreHashNode* init = nullptr;
    if (a->getType() != NT_NOTHING) {
        if (runtime_check && a->getType() != NT_HASH) {
            xsink->raiseException("HASHDECL-INIT-ERROR", "hashdecl '%s' hash initializer value must be a hash; got type '%s' instead", name.c_str(), a->getTypeName());
            return nullptr;
        }

        init = a->get<const QoreHashNode>();
    }

    return newHash(init, runtime_check, xsink);
}

QoreHashNode* typed_hash_decl_private::newHash(const QoreHashNode* init, bool runtime_check, ExceptionSink* xsink,
        QoreHashNode* rv) const {
    if (runtime_check) {
        ConstHashIterator i(init);
        while (i.next()) {
            if (!members.find(i.getKey())) {
                xsink->raiseException("HASHDECL-INIT-ERROR", "hashdecl '%s' hash initializer value contains unknown key '%s'", name.c_str(), i.getKey());
                return nullptr;
            }
        }
    }

    ReferenceHolder<QoreHashNode> h(xsink);
    if (rv) {
        qore_hash_private::get(*rv)->setHashDecl(thd);
        h = rv;
    } else {
        h = qore_hash_private::newHashDecl(thd);
    }
    initHash(*h, init, xsink);
    return *xsink ? nullptr : h.release();
}

int typed_hash_decl_private::initHash(QoreHashNode* h, const QoreHashNode* init, ExceptionSink* xsink) const {
    int rc = initHashIntern(h, init, xsink);
    // xsink may be nullptr when being executed in a try block
    if (xsink && *xsink) {
        assert(rc);
        xsink->appendLastDescription(" (while initializing hashdecl '%s')", name.c_str());
    }
    return rc;
}

int typed_hash_decl_private::initHashIntern(QoreHashNode* h, const QoreHashNode* init, ExceptionSink* xsink) const {
#ifdef QORE_MANAGE_STACK
    if (xsink && check_stack(xsink)) {
        return -1;
    }
#endif

    for (auto& i : members.member_list) {
        // first try to use value given in init hash
        if (init) {
            const qore_hash_private* hi = qore_hash_private::get(*init);
            bool exists;
            ValueHolder val(hi->getReferencedKeyValueIntern(i.first, exists), xsink);
            if (exists) {
                // check types
                QoreTypeInfo::acceptInputKey(i.second->getTypeInfo(), i.first, *val, xsink);
                if (*xsink) {
                    return -1;
                }
                qore_hash_private* h_priv = qore_hash_private::get(*h);
                QoreValue& v = h_priv->getValueRef(i.first);
                assert(v.isNothing());
                v = val.release();
                // issue #3481: maintain DGC counts
                if (needs_scan(v)) {
                    h_priv->incScanCount(1);
                }
                continue;
            }
        }

        if (!i.second) {
            continue;
        }

        if (i.second->exp) {
            qore_hash_private* h_priv = qore_hash_private::get(*h);
            QoreValue& v = h_priv->getValueRef(i.first);
            assert(v.isNothing());

            ValueEvalOptimizedRefHolder val(i.second->exp, xsink);
            if (*xsink) {
                return -1;
            }

            QoreTypeInfo::acceptInputMember(i.second->getTypeInfo(), i.first, *val, xsink);
            if (*xsink) {
                return -1;
            }

            v = val.takeReferencedValue();
            // issue #3481: maintain DGC counts
            if (needs_scan(v)) {
                h_priv->incScanCount(1);
            }
        } else {
            QoreProgram* pgm = getProgram();
            // there may be no program context when an exception is thrown while attaching to a Program in an external thread
            if (pgm && (pgm->getParseOptions64() & PO_STRICT_TYPES)) {
                const QoreTypeInfo* key_type = i.second->getTypeInfo();
                bool requires_value = !QoreTypeInfo::parseAcceptsReturns(key_type, NT_NOTHING);
                if (!requires_value) {
                    continue;
                }
                if (init) {
                    xsink->raiseException("RUNTIME-TYPE-ERROR", "hash value to initialize hashdecl '%s' is missing a " \
                        "%s value for key '%s'", name.c_str(), QoreTypeInfo::getName(key_type), i.first);
                    return -1;
                }
                qore_hash_private* h_priv = qore_hash_private::get(*h);
                QoreValue& v = h_priv->getValueRef(i.first);
                assert(v.isNothing());
                v = QoreTypeInfo::getDefaultQoreValue(i.second->getTypeInfo());
                // issue #3481: maintain DGC counts
                if (needs_scan(v)) {
                    h_priv->incScanCount(1);
                }
            }
        }
    }

    return 0;
}

TypedHashDecl::TypedHashDecl(const char* name, const char* path)
        : priv(new typed_hash_decl_private(get_runtime_location(), name, path, this)) {
    assert(priv->typeInfo);
    assert(priv->orNothingTypeInfo);
}

TypedHashDecl::TypedHashDecl(typed_hash_decl_private* p) : priv(p) {
    assert(!priv->typeInfo);
    assert(!priv->orNothingTypeInfo);

    priv->typeInfo = new QoreHashDeclTypeInfo(this, priv->getName(), priv->getPath());
    priv->orNothingTypeInfo = new QoreHashDeclOrNothingTypeInfo(this, priv->getName(), priv->getPath());
}

TypedHashDecl::TypedHashDecl(const TypedHashDecl& old) : priv(new typed_hash_decl_private(*old.priv, this)) {
    assert(priv->typeInfo);
    assert(priv->orNothingTypeInfo);
}

TypedHashDecl::~TypedHashDecl() {
    delete priv;
}

void TypedHashDecl::addMember(const char* name, const QoreTypeInfo* memberTypeInfo, QoreValue init_val) {
    priv->addMember(name, memberTypeInfo, init_val);
}

const QoreTypeInfo* TypedHashDecl::getTypeInfo(bool or_nothing) const {
    return priv->getTypeInfo(or_nothing);
}

const char* TypedHashDecl::getName() const {
    return priv->getName();
}

bool TypedHashDecl::isSystem() const {
    return priv->isSystem();
}

bool TypedHashDecl::isPublic() const {
    return priv->isPublic();
}

const QoreExternalMemberBase* TypedHashDecl::findLocalMember(const char* name) const {
    return reinterpret_cast<const QoreExternalMemberBase*>(priv->findLocalMember(name));
}

const QoreExternalProgramLocation* TypedHashDecl::getSourceLocation() const {
    return reinterpret_cast<const QoreExternalProgramLocation*>(priv->getParseLocation());
}

std::string TypedHashDecl::getNamespacePath(bool anchored) const {
    std::string path;
    // when called during construction, this is nullptr
    if (priv) {
        if (priv->ns) {
            priv->ns->getPath(path);
            if (!path.empty()) {
                path += "::";
            }
            if (anchored) {
                path.insert(0, "::");
            }
        }
        path += getName();
    }
    return path;
}

bool TypedHashDecl::equal(const TypedHashDecl* other) const {
    if (!other) {
        return false;
    }

    return other->priv->orig == priv->orig;
}

const char* TypedHashDecl::getModuleName() const {
    return priv->getModuleName();
}

const QoreNamespace* TypedHashDecl::getNamespace() const {
    const qore_ns_private* ns = priv->getNamespace();
    return ns ? ns->ns : nullptr;
}

QoreHashNode* TypedHashDecl::doRuntimeCast(const QoreHashNode* h, ExceptionSink* xsink) const {
    return priv->newHash(h, true, xsink);
}

TypedHashDeclHolder::~TypedHashDeclHolder() {
    if (thd) {
        typed_hash_decl_private::get(*thd)->deref();
    }
}

TypedHashDecl* TypedHashDeclHolder::operator=(TypedHashDecl* nhd) {
    if (thd)
        typed_hash_decl_private::get(*thd)->deref();
    return thd = nhd;
}

class typed_hash_decl_member_iterator : public PrivateMemberIteratorBase<HashDeclMemberMap, QoreExternalMemberBase> {
public:
    DLLLOCAL typed_hash_decl_member_iterator(const typed_hash_decl_private& obj)
            : PrivateMemberIteratorBase<HashDeclMemberMap, QoreExternalMemberBase>(obj.members.member_list) {
    }
};

TypedHashDeclMemberIterator::TypedHashDeclMemberIterator(const TypedHashDecl& thd) :
    priv(new typed_hash_decl_member_iterator(*typed_hash_decl_private::get(thd))) {
}

TypedHashDeclMemberIterator::~TypedHashDeclMemberIterator() {
    delete priv;
}

bool TypedHashDeclMemberIterator::next() {
    return priv->next();
}

const QoreExternalMemberBase& TypedHashDeclMemberIterator::getMember() const {
    return priv->getMember();
}

const char* TypedHashDeclMemberIterator::getName() const {
    return priv->getName();
}
