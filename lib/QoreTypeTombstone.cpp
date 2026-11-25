/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreTypeTombstone.cpp

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

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

// The tombstones of retired declarations (issue #6419; see design/derived-type-retirement.md)
//
// A derived type that outlives a declaration that it names (list<Foo> held by a value after Foo's Program was deleted)
// is repointed to the declaration's tombstone, so that the declaration can be freed.  A tombstone is an empty,
// immortal declaration of the same kind that is equal to the declaration as the type checks compare declarations:
//
// - a class: the same name and signature hash (qore_class_private::equal()), with the tombstones of its base classes
//   (so the type checks that walk its hierarchy find the same classes) and of its type parameters' bounds and
//   defaults;
// - a hashdecl: the same name and members, with the tombstones of their types (typed_hash_decl_private::equal()),
//   its parent's tombstone and its type parameters; a concrete declaration of a generic hashdecl is that of the
//   generic hashdecl's tombstone with the tombstones of its type arguments;
// - an enum: the same name, base type and members (qore_enum_decl_private::parseEqual()).
//
// The tombstones are interned by a fingerprint of these properties, so that a Program created and deleted repeatedly
// from the same source adds no tombstone after its first deletion.  The recursion ends at builtin (system)
// declarations and builtin types; a tombstone never points to a user declaration, so it keeps nothing alive.

#include <qore/Qore.h>

#include "qore/intern/QoreClassIntern.h"
#include "qore/intern/QoreTypeInfo.h"
#include "qore/intern/QoreTypeOwner.h"
#include "qore/intern/qore_enum_decl_private.h"
#include "qore/intern/typed_hash_decl_private.h"
#include "qore/intern/QoreTypeDeclPin.h"
#include "qore/intern/QoreForkHandlers.h"

#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

// (not in an anonymous namespace: QoreTombstoneBuilder is a friend of the declarations' private classes)
//! The interned tombstones, never freed before library cleanup
struct QoreTombstoneRegistry {
    std::mutex m;
    std::map<std::string, QoreClass*> classes;
    std::map<std::string, TypedHashDecl*> hashdecls;
    std::map<std::string, QoreEnumDecl*> enums;
    //! the tombstone declarations: classes, hashdecls (including concrete declarations of generic tombstones) and enums
    std::set<const void*> tombs;
    //! the fingerprints of declarations that refer back to nothing outside them, interned: a fingerprint names one by
    //! its ID, so that its length does not grow with the paths by which it reaches a declaration
    std::map<std::string, size_t> fp_ids;
    //! true once library cleanup has freed the tombstones
    bool freed = false;
    //! making a tombstone takes leaf locks only; a child of fork() gets the lock free (qore#7549)
    QoreForkStdMutex fork_handler{m, QFLR_TYPE_TOMBSTONES, "type_tombstones"};
};

static QoreTombstoneRegistry& tombstone_registry() {
    // not destroyed: declarations are released at static destruction too
    static QoreTombstoneRegistry* r = new QoreTombstoneRegistry;
    return *r;
}

// created during static initialization, so that its fork handler is not registered while another registered lock is
// held (see design/fork-handlers.md)
[[maybe_unused]] static QoreTombstoneRegistry& tombstone_registry_init = tombstone_registry();

//! the deepest nesting of declarations and types followed when a tombstone is made; deeper ones are not tombstoned
/** A declaration whose tombstone cannot be made is kept until library cleanup (QoreTypeOwner::type_owner_unfreeable),
    as before issue #6419.  A fingerprint that fails stops at once: the declarations on its path are not memoized, so
    a walk that went on would follow every path of a hierarchy that reaches a class by many paths (a chain of
    diamonds)
*/
constexpr int QORE_TOMBSTONE_MAX_DEPTH = 64;

static void fp_ptr(std::string& fp, const void* p) {
    char buf[32];
    snprintf(buf, sizeof buf, "%p", p);
    fp += buf;
}

static void fp_str(std::string& fp, const std::string& s) {
    // the length makes the fingerprint unambiguous whatever the string contains
    fp += std::to_string(s.size());
    fp += ':';
    fp += s;
}

//! Makes the tombstones for one request; the caller holds the registry lock
/** A tombstone is registered before the tombstones that it refers to are made, so a declaration that refers back to
    one being made (a type parameter's bound naming its class, a hashdecl member naming its hashdecl) finds it
*/
class QoreTombstoneBuilder {
public:
    DLLLOCAL QoreTombstoneBuilder(QoreTombstoneRegistry& r) : r(r) {
    }

    //! On failure, the tombstones registered by this request are unregistered (and leaked, as another one made by it
    //! can refer to them)
    DLLLOCAL ~QoreTombstoneBuilder() {
        if (!failed) {
            return;
        }
        for (QoreTypeOwner* o : cached) {
            o->type_owner_tombstone = nullptr;
        }
        for (const std::string& fp : new_classes) {
            r.classes.erase(fp);
        }
        for (const std::string& fp : new_hashdecls) {
            r.hashdecls.erase(fp);
        }
        for (const std::string& fp : new_enums) {
            r.enums.erase(fp);
        }
    }

    bool failed = false;

    // fingerprints ---------------------------------------------------------------------------------------------------

    //! Appends the fingerprint of a type; \a user is set if it names a user declaration
    DLLLOCAL void fpType(std::string& fp, const QoreTypeInfo* ti, bool& user, int depth) {
        if (failed) {
            return;
        }
        if (!ti) {
            fp += '-';
            return;
        }
        if (depth > QORE_TOMBSTONE_MAX_DEPTH) {
            failed = true;
            return;
        }
        size_t start = fp.size();
        bool u = false;
        fpTypeIntern(fp, ti, u, depth + 1);
        if (!u) {
            // a type that names no user declaration is never retired: its address identifies it
            fp.resize(start);
            fp += 'B';
            fp_ptr(fp, ti);
        } else {
            user = true;
        }
    }

    DLLLOCAL void fpClassRef(std::string& fp, const qore_class_private* p, int depth) {
        if (failed) {
            return;
        }
        if (p->sys) {
            fp += 'S';
            fp_ptr(fp, p);
            return;
        }
        if (refOrMemo(fp, p)) {
            return;
        }
        if (depth > QORE_TOMBSTONE_MAX_DEPTH) {
            failed = true;
            return;
        }
        stack.push_back(p);
        size_t memo_start = fp.size();
        size_t outer_min_ref = min_ref;
        min_ref = SIZE_MAX;
        fp += "C{";
        fp_str(fp, p->path);
        fp_str(fp, p->name);
        if (p->hash) {
            QoreString h;
            p->hash.toString(h);
            fp_str(fp, h.c_str());
        } else {
            fp += '0';
        }
        fp += p->raw_accepts_parameterized ? 'r' : '-';
        fp += p->raw_construction_defaults_to_auto ? 'a' : '-';
        fp += "|tp";
        for (size_t i = 0, e = p->type_params.size(); i < e; ++i) {
            const QoreGenericTypeParam& tp = p->type_params[i];
            fp += '(';
            fp_str(fp, tp.name);
            fp_str(fp, tp.bound_type);
            fp_str(fp, tp.default_type);
            bool u = false;
            fpType(fp, i < p->type_param_bound_types.size() ? p->type_param_bound_types[i] : nullptr, u, depth + 1);
            fpType(fp, i < p->type_param_default_types.size() ? p->type_param_default_types[i] : nullptr, u,
                depth + 1);
            fp += ')';
        }
        fp += "|b";
        if (p->scl) {
            for (BCNode* n : *p->scl) {
                // a parent class is kept alive by the list of the class's ancestors (BCSMList::append() references
                // it), not by the edge: a class whose hierarchy was not initialized (such as one that is circular or
                // inherits a class whose hierarchy is invalid) has edges to parent classes that its list does not
                // hold, and a parse rollback may have freed them already, so the edge is compared with the list
                // without reading the parent class (qore#7464): with the canonical class of each entry, which the
                // entry's reference keeps alive, as the edge holds the canonical class (BCNode::setClassPointer())
                // and an entry can be another QoreClass of the class (one imported into the Program, as the AOT
                // loader adds it with qore_class_private::addBaseClass())
                if (failed || !n->sclass
                    || std::none_of(p->scl->sml.begin(), p->scl->sml.end(),
                        [n](const class_virt_pair_t& i) {
                            return qore_class_private::get(*i.first)->cls == n->sclass;
                        })) {
                    failed = true;
                    break;
                }
                fp += '(';
                fp += std::to_string(static_cast<int>(n->access));
                fp += n->is_virtual ? 'v' : '-';
                fpClassRef(fp, qore_class_private::get(*n->sclass), depth + 1);
                bool u = false;
                fpType(fp, n->type_info, u, depth + 1);
                fp += ')';
            }
        }
        fp += '}';
        memoLeave(fp, p, memo_start, outer_min_ref);
        stack.pop_back();
    }

    DLLLOCAL void fpHashDeclRef(std::string& fp, const typed_hash_decl_private* p, int depth) {
        if (failed) {
            return;
        }
        if (p->isTypeOwnerSystem()) {
            fp += 'S';
            fp_ptr(fp, p);
            return;
        }
        if (refOrMemo(fp, p)) {
            return;
        }
        if (depth > QORE_TOMBSTONE_MAX_DEPTH) {
            failed = true;
            return;
        }
        stack.push_back(p);
        size_t memo_start = fp.size();
        size_t outer_min_ref = min_ref;
        min_ref = SIZE_MAX;
        if (p->parameterized_base) {
            fp += "HP{";
            fpHashDeclRef(fp, typed_hash_decl_private::get(*p->parameterized_base), depth + 1);
            for (const QoreTypeInfo* arg : p->type_args) {
                fp += ',';
                bool u = false;
                fpType(fp, arg, u, depth + 1);
            }
            fp += '}';
            memoLeave(fp, p, memo_start, outer_min_ref);
            stack.pop_back();
            return;
        }
        fp += "H{";
        fp_str(fp, p->path);
        fp_str(fp, p->name);
        fp += "|tp";
        for (const QoreGenericTypeParam& tp : p->type_params) {
            fp += '(';
            fp_str(fp, tp.name);
            fp_str(fp, tp.bound_type);
            fp_str(fp, tp.default_type);
            fp += ')';
        }
        fp += "|p";
        if (p->parentHashDecl) {
            fpHashDeclRef(fp, typed_hash_decl_private::get(*p->parentHashDecl), depth + 1);
        }
        fp += "|m";
        for (auto& i : p->members.member_list) {
            fp += '(';
            fp_str(fp, i.first);
            if (i.second) {
                bool u = false;
                fpType(fp, i.second->getTypeInfo(), u, depth + 1);
                fpDefault(fp, i.second->exp);
            }
            fp += ')';
        }
        fp += '}';
        memoLeave(fp, p, memo_start, outer_min_ref);
        stack.pop_back();
    }

    DLLLOCAL void fpEnumRef(std::string& fp, const qore_enum_decl_private* p) {
        if (p->isTypeOwnerSystem()) {
            fp += 'S';
            fp_ptr(fp, p);
            return;
        }
        fp += "E{";
        fp_str(fp, p->getName());
        fp_str(fp, p->getPath());
        fp_ptr(fp, p->getBaseTypeInfo());
        for (const QoreEnumMember* m : p->getMembers()) {
            fp += '(';
            fp_str(fp, m->getName());
            if (!fpValue(fp, m->getValue())) {
                failed = true;
            }
            fp += ')';
        }
        fp += '}';
    }

    // tombstones -----------------------------------------------------------------------------------------------------

    //! Returns the tombstone of a type: the type itself if it names no user declaration
    DLLLOCAL const QoreTypeInfo* tombType(const QoreTypeInfo* ti, int depth);

    DLLLOCAL QoreClass* tombClass(const QoreClass* qc, int depth);

    DLLLOCAL TypedHashDecl* tombHashDecl(const TypedHashDecl* hd, int depth);

    DLLLOCAL QoreEnumDecl* tombEnum(const QoreEnumDecl* ed);

private:
    QoreTombstoneRegistry& r;

    //! the declarations whose tombstones this request cached, cleared if it fails
    std::vector<QoreTypeOwner*> cached;

    DLLLOCAL void cache(QoreTypeOwner& o, const void* tomb) {
        o.type_owner_tombstone = tomb;
        cached.push_back(&o);
    }

    //! the declarations whose fingerprints are being made, for references back to them
    std::vector<const void*> stack;
    //! the lowest position on the stack that the fingerprint being made refers back to
    size_t min_ref = SIZE_MAX;
    //! the fingerprints of declarations that refer back to nothing outside them, so that they can be reused at any
    //! depth (a hierarchy that reaches a declaration by many paths is fingerprinted once)
    std::map<const void*, std::string> memo;

    //! Appends a reference back to a declaration on the stack, or its fingerprint if it is known; returns true if one
    //! was appended
    /** A reference back is the distance from the top of the stack, so that the fingerprint of a declaration that
        refers back only to declarations inside it does not depend on the depth that it is made at
    */
    DLLLOCAL bool refOrMemo(std::string& fp, const void* p) {
        for (size_t i = 0, e = stack.size(); i < e; ++i) {
            if (stack[i] == p) {
                fp += '#';
                fp += std::to_string(e - 1 - i);
                min_ref = std::min(min_ref, i);
                return true;
            }
        }
        auto i = memo.find(p);
        if (i != memo.end()) {
            fp += i->second;
            return true;
        }
        return false;
    }

    //! Replaces the fingerprint of the declaration on top of the stack with its interned ID if it refers back to
    //! nothing below it
    DLLLOCAL void memoLeave(std::string& fp, const void* p, size_t start, size_t outer_min_ref) {
        if (min_ref >= stack.size() - 1 && !failed) {
            auto i = r.fp_ids.emplace(fp.substr(start), r.fp_ids.size()).first;
            fp.resize(start);
            std::string id = "@" + std::to_string(i->second) + ";";
            fp += id;
            memo[p] = id;
        }
        min_ref = std::min(min_ref, outer_min_ref);
    }
    //! the tombstones made by this request, by declaration
    std::map<const void*, void*> building;
    std::vector<std::string> new_classes, new_hashdecls, new_enums;

    DLLLOCAL void fpTypeIntern(std::string& fp, const QoreTypeInfo* ti, bool& user, int depth);

    //! Appends a value of a member default or an enum member; returns false if it cannot be fingerprinted
    DLLLOCAL static bool fpValue(std::string& fp, const QoreValue& v) {
        switch (v.getType()) {
            case NT_NOTHING:
                fp += 'n';
                return true;
            case NT_INT:
                fp += 'i';
                fp += std::to_string(v.getAsBigInt());
                return true;
            case NT_BOOLEAN:
                fp += v.getAsBool() ? 'T' : 'F';
                return true;
            case NT_FLOAT: {
                char buf[64];
                snprintf(buf, sizeof buf, "f%a", v.getAsFloat());
                fp += buf;
                return true;
            }
            case NT_STRING: {
                // also a short string held in the value itself
                QoreStringValueHelper str(v);
                fp += 's';
                fp_ptr(fp, str->getEncoding());
                fp_str(fp, std::string(str->c_str(), str->size()));
                return true;
            }
            default:
                break;
        }
        return false;
    }

    //! A member default that is not a simple value is not copied to the tombstone
    DLLLOCAL static void fpDefault(std::string& fp, const QoreValue& v) {
        fp += '=';
        if (!fpValue(fp, v)) {
            fp += '?';
        }
    }

    DLLLOCAL static QoreValue tombDefault(const QoreValue& v) {
        std::string tmp;
        return fpValue(tmp, v) ? v.refSelf() : QoreValue();
    }

    //! Returns the tombstone of a type parameter type of a declaration being made
    DLLLOCAL const QoreTypeInfo* tombTypeParam(const QoreTypeParameterTypeInfo* tpi, int depth);
};

//! Returns true if a type names a user declaration in its specs, for the kinds of types that are not handled
//! explicitly (see QoreTombstoneBuilder::tombType())
static bool qore_type_names_user_decl(const QoreTypeInfo* ti, int depth) {
    if (!ti || depth > QORE_TOMBSTONE_MAX_DEPTH) {
        return depth > QORE_TOMBSTONE_MAX_DEPTH;
    }
    for (const QoreAcceptSpec& as : ti->getAcceptSpecs()) {
        const QoreTypeSpec& spec = as.spec;
        if (const QoreClass* qc = spec.getClass()) {
            if (!qore_class_private::get(*qc)->sys) {
                return true;
            }
        } else if (const TypedHashDecl* hd = spec.getHashDecl()) {
            if (!typed_hash_decl_private::get(*hd)->isTypeOwnerSystem()) {
                return true;
            }
        } else if (const QoreEnumDecl* ed = spec.getEnum()) {
            if (!qore_enum_decl_private::get(*ed)->isTypeOwnerSystem()) {
                return true;
            }
        } else if (spec.getParameterizedClassTypeInfo() || spec.getTypeParameterTypeInfo()
                || spec.getWildcardTypeInfo()) {
            return true;
        } else if (const QoreTypeInfo* elem = spec.getElementType()) {
            if (elem != ti && qore_type_names_user_decl(elem, depth + 1)) {
                return true;
            }
        } else if (const QoreTypeInfo* ref = spec.getComplexReference()) {
            if (ref != ti && qore_type_names_user_decl(ref, depth + 1)) {
                return true;
            }
        }
    }
    return false;
}

void QoreTombstoneBuilder::fpTypeIntern(std::string& fp, const QoreTypeInfo* ti, bool& user, int depth) {
    // declarations' own types
    if (dynamic_cast<const QoreClassTypeInfo*>(ti)) {
        const QoreClass* qc = ti->getAcceptSpecs()[0].spec.getClass();
        const qore_class_private* p = qc ? qore_class_private::get(*qc) : nullptr;
        if (!p || p->sys) {
            return;
        }
        user = true;
        fp += dynamic_cast<const QoreClassOrNothingTypeInfo*>(ti) ? "c*" : "c";
        fpClassRef(fp, p, depth);
        return;
    }
    if (dynamic_cast<const QoreHashDeclTypeInfo*>(ti)) {
        const TypedHashDecl* hd = ti->getAcceptSpecs()[0].spec.getHashDecl();
        const typed_hash_decl_private* p = hd ? typed_hash_decl_private::get(*hd) : nullptr;
        if (!p || p->isTypeOwnerSystem()) {
            return;
        }
        user = true;
        fp += dynamic_cast<const QoreHashDeclOrNothingTypeInfo*>(ti) ? "h*" : "h";
        fpHashDeclRef(fp, p, depth);
        return;
    }
    if (dynamic_cast<const QoreEnumTypeInfo*>(ti)) {
        const QoreEnumDecl* ed = ti->getAcceptSpecs()[0].spec.getEnum();
        const qore_enum_decl_private* p = ed ? qore_enum_decl_private::get(*ed) : nullptr;
        if (!p || p->isTypeOwnerSystem()) {
            return;
        }
        user = true;
        fp += dynamic_cast<const QoreEnumOrNothingTypeInfo*>(ti) ? "e*" : "e";
        fpEnumRef(fp, p);
        return;
    }
    // derived types
    if (const QoreParameterizedClassTypeInfo* pti = dynamic_cast<const QoreParameterizedClassTypeInfo*>(ti)) {
        const qore_class_private* p = qore_class_private::get(*pti->getBaseClass());
        fp += pti->isOrNothing() ? "P*" : "P";
        if (!p->sys) {
            user = true;
        }
        fpClassRef(fp, p, depth);
        for (const QoreTypeInfo* arg : pti->getTypeArgs()) {
            fp += ',';
            fpType(fp, arg, user, depth);
        }
        return;
    }
    if (const QoreTypeParameterTypeInfo* tpi = dynamic_cast<const QoreTypeParameterTypeInfo*>(ti)) {
        const void* owner = nullptr;
        if (const QoreClass* qc = tpi->getOwnerClass()) {
            owner = qore_class_private::get(*qc);
        } else if (const TypedHashDecl* hd = tpi->getOwnerHashDecl()) {
            owner = typed_hash_decl_private::get(*hd);
        } else if (tpi->getOwnerSignature()) {
            // the type parameter of a function or method: not tombstoned
            failed = true;
            return;
        } else {
            // an orphaned type parameter type is retired and never freed in stage 1: it is immortal
            return;
        }
        for (size_t i = 0, e = stack.size(); i < e; ++i) {
            if (stack[i] == owner) {
                user = true;
                fp += tpi->isOrNothing() ? "T*" : "T";
                // relative to the top of the stack, as a reference back (see refOrMemo())
                fp += std::to_string(stack.size() - 1 - i);
                min_ref = std::min(min_ref, i);
                fp += '.';
                fp += std::to_string(tpi->getIndex());
                return;
            }
        }
        // the type parameter of a declaration that is not being fingerprinted
        failed = true;
        return;
    }
    if (const QoreWildcardTypeInfo* wti = dynamic_cast<const QoreWildcardTypeInfo*>(ti)) {
        fp += 'W';
        fp += std::to_string(static_cast<int>(wti->getKind()));
        fpType(fp, wti->getBound(), user, depth);
        return;
    }
    if (const QoreUnionTypeInfo* uti = dynamic_cast<const QoreUnionTypeInfo*>(ti)) {
        fp += uti->isOrNothing() ? "U*" : "U";
        for (const QoreTypeInfo* m : uti->getMemberTypes()) {
            fp += ',';
            fpType(fp, m, user, depth);
        }
        return;
    }
    if (const QoreComplexCodeTypeInfo* cti = dynamic_cast<const QoreComplexCodeTypeInfo*>(ti)) {
        fp += cti->isOrNothing() ? "F*" : "F";
        fp += cti->hasVarArgs() ? 'v' : '-';
        fpType(fp, cti->getReturnType(), user, depth);
        for (const QoreTypeInfo* p : cti->getParamTypes()) {
            fp += ',';
            fpType(fp, p, user, depth);
        }
        return;
    }
    const QoreTypeSpec& spec = ti->getAcceptSpecs()[0].spec;
    const char* kind = nullptr;
    const QoreTypeInfo* elem = nullptr;
    if (dynamic_cast<const QoreComplexSoftListTypeInfo*>(ti)) {
        kind = "SL";
        elem = spec.getComplexSoftList();
    } else if (dynamic_cast<const QoreComplexSoftListOrNothingTypeInfo*>(ti)) {
        kind = "SL*";
        elem = spec.getComplexSoftList();
    } else if (dynamic_cast<const QoreComplexListOrNothingTypeInfo*>(ti)) {
        kind = "L*";
        elem = spec.getComplexList();
    } else if (dynamic_cast<const QoreComplexListTypeInfo*>(ti)) {
        kind = "L";
        elem = spec.getComplexList();
    } else if (dynamic_cast<const QoreComplexHashOrNothingTypeInfo*>(ti)) {
        kind = "H*";
        elem = spec.getComplexHash();
    } else if (dynamic_cast<const QoreComplexHashTypeInfo*>(ti)) {
        kind = "H";
        elem = spec.getComplexHash();
    } else if (dynamic_cast<const QoreComplexReferenceOrNothingTypeInfo*>(ti)) {
        kind = "R*";
        elem = spec.getComplexReference();
    } else if (dynamic_cast<const QoreComplexReferenceTypeInfo*>(ti)) {
        kind = "R";
        elem = spec.getComplexReference();
    } else if (dynamic_cast<const QoreComplexHardReferenceTypeInfo*>(ti)) {
        kind = "HR";
        elem = spec.getComplexReference();
    }
    if (kind) {
        fp += kind;
        fpType(fp, elem, user, depth);
        return;
    }
    // any other type: one that names a user declaration is not tombstoned
    if (qore_type_names_user_decl(ti, 0)) {
        failed = true;
    }
}

const QoreTypeInfo* QoreTombstoneBuilder::tombTypeParam(const QoreTypeParameterTypeInfo* tpi, int depth) {
    if (const QoreClass* qc = tpi->getOwnerClass()) {
        QoreClass* t = tombClass(qc, depth);
        return t ? qore_get_type_parameter_type(t, tpi->getIndex(), tpi->getParameterName().c_str(),
            tpi->isOrNothing()) : nullptr;
    }
    if (const TypedHashDecl* hd = tpi->getOwnerHashDecl()) {
        TypedHashDecl* t = tombHashDecl(hd, depth);
        return t ? qore_get_hashdecl_type_parameter_type(t, tpi->getIndex(), tpi->getParameterName().c_str(),
            tpi->isOrNothing()) : nullptr;
    }
    if (tpi->getOwnerSignature()) {
        return nullptr;
    }
    // orphaned: immortal in stage 1
    return tpi;
}

const QoreTypeInfo* QoreTombstoneBuilder::tombType(const QoreTypeInfo* ti, int depth) {
    if (!ti || failed) {
        return ti;
    }
    if (depth > QORE_TOMBSTONE_MAX_DEPTH) {
        failed = true;
        return nullptr;
    }
    ++depth;
    auto fail = [this]() -> const QoreTypeInfo* {
        failed = true;
        return nullptr;
    };
    if (dynamic_cast<const QoreClassTypeInfo*>(ti)) {
        const QoreClass* qc = ti->getAcceptSpecs()[0].spec.getClass();
        if (!qc || qore_class_private::get(*qc)->sys) {
            return ti;
        }
        QoreClass* t = tombClass(qc, depth);
        if (!t) {
            return fail();
        }
        return dynamic_cast<const QoreClassOrNothingTypeInfo*>(ti) ? t->getOrNothingTypeInfo() : t->getTypeInfo();
    }
    if (dynamic_cast<const QoreHashDeclTypeInfo*>(ti)) {
        const TypedHashDecl* hd = ti->getAcceptSpecs()[0].spec.getHashDecl();
        if (!hd || typed_hash_decl_private::get(*hd)->isTypeOwnerSystem()) {
            return ti;
        }
        TypedHashDecl* t = tombHashDecl(hd, depth);
        if (!t) {
            return fail();
        }
        return dynamic_cast<const QoreHashDeclOrNothingTypeInfo*>(ti) ? t->getTypeInfo(true) : t->getTypeInfo();
    }
    if (dynamic_cast<const QoreEnumTypeInfo*>(ti)) {
        const QoreEnumDecl* ed = ti->getAcceptSpecs()[0].spec.getEnum();
        if (!ed || qore_enum_decl_private::get(*ed)->isTypeOwnerSystem()) {
            return ti;
        }
        QoreEnumDecl* t = tombEnum(ed);
        if (!t) {
            return fail();
        }
        return t->getTypeInfo(dynamic_cast<const QoreEnumOrNothingTypeInfo*>(ti) != nullptr);
    }
    if (const QoreParameterizedClassTypeInfo* pti = dynamic_cast<const QoreParameterizedClassTypeInfo*>(ti)) {
        const QoreClass* base = pti->getBaseClass();
        const QoreClass* tbase = qore_class_private::get(*base)->sys ? base : tombClass(base, depth);
        if (!tbase) {
            return fail();
        }
        bool changed = tbase != base;
        type_vec_t args;
        for (const QoreTypeInfo* arg : pti->getTypeArgs()) {
            const QoreTypeInfo* targ = tombType(arg, depth);
            if (failed) {
                return nullptr;
            }
            changed = changed || targ != arg;
            args.push_back(targ);
        }
        return changed ? qore_get_parameterized_class_type(tbase, args, pti->isOrNothing()) : ti;
    }
    if (const QoreTypeParameterTypeInfo* tpi = dynamic_cast<const QoreTypeParameterTypeInfo*>(ti)) {
        const QoreTypeInfo* rv = tombTypeParam(tpi, depth);
        return rv ? rv : fail();
    }
    if (const QoreWildcardTypeInfo* wti = dynamic_cast<const QoreWildcardTypeInfo*>(ti)) {
        const QoreTypeInfo* bound = wti->getBound();
        const QoreTypeInfo* tbound = tombType(bound, depth);
        if (failed) {
            return nullptr;
        }
        if (tbound == bound) {
            return ti;
        }
        switch (wti->getKind()) {
            case QoreWildcardKind::Extends:
                return qore_get_wildcard_extends_type(tbound);
            case QoreWildcardKind::Super:
                return qore_get_wildcard_super_type(tbound);
            default:
                return fail();
        }
    }
    if (const QoreUnionTypeInfo* uti = dynamic_cast<const QoreUnionTypeInfo*>(ti)) {
        bool changed = false;
        type_vec_t members;
        for (const QoreTypeInfo* m : uti->getMemberTypes()) {
            const QoreTypeInfo* tm = tombType(m, depth);
            if (failed) {
                return nullptr;
            }
            changed = changed || tm != m;
            members.push_back(tm);
        }
        if (!changed) {
            return ti;
        }
        const QoreTypeInfo* rv = qore_get_union_type(members, uti->isOrNothing());
        return rv ? rv : fail();
    }
    if (const QoreComplexCodeTypeInfo* cti = dynamic_cast<const QoreComplexCodeTypeInfo*>(ti)) {
        const QoreTypeInfo* ret = cti->getReturnType();
        const QoreTypeInfo* tret = tombType(ret, depth);
        if (failed) {
            return nullptr;
        }
        bool changed = tret != ret;
        type_vec_t params;
        for (const QoreTypeInfo* p : cti->getParamTypes()) {
            const QoreTypeInfo* tp = tombType(p, depth);
            if (failed) {
                return nullptr;
            }
            changed = changed || tp != p;
            params.push_back(tp);
        }
        return changed ? qore_get_complex_code_type(tret, params, cti->hasVarArgs(), cti->isOrNothing()) : ti;
    }
    const QoreTypeSpec& spec = ti->getAcceptSpecs()[0].spec;
    const QoreTypeInfo* (*make)(const QoreTypeInfo*) = nullptr;
    const QoreTypeInfo* elem = nullptr;
    if (dynamic_cast<const QoreComplexSoftListTypeInfo*>(ti)) {
        make = qore_get_complex_softlist_type;
        elem = spec.getComplexSoftList();
    } else if (dynamic_cast<const QoreComplexSoftListOrNothingTypeInfo*>(ti)) {
        make = qore_get_complex_softlist_or_nothing_type;
        elem = spec.getComplexSoftList();
    } else if (dynamic_cast<const QoreComplexListOrNothingTypeInfo*>(ti)) {
        make = qore_get_complex_list_or_nothing_type;
        elem = spec.getComplexList();
    } else if (dynamic_cast<const QoreComplexListTypeInfo*>(ti)) {
        make = qore_get_complex_list_type;
        elem = spec.getComplexList();
    } else if (dynamic_cast<const QoreComplexHashOrNothingTypeInfo*>(ti)) {
        make = qore_get_complex_hash_or_nothing_type;
        elem = spec.getComplexHash();
    } else if (dynamic_cast<const QoreComplexHashTypeInfo*>(ti)) {
        make = qore_get_complex_hash_type;
        elem = spec.getComplexHash();
    } else if (dynamic_cast<const QoreComplexReferenceOrNothingTypeInfo*>(ti)) {
        make = qore_get_complex_reference_or_nothing_type;
        elem = spec.getComplexReference();
    } else if (dynamic_cast<const QoreComplexReferenceTypeInfo*>(ti)) {
        make = qore_get_complex_reference_type;
        elem = spec.getComplexReference();
    } else if (dynamic_cast<const QoreComplexHardReferenceTypeInfo*>(ti)) {
        make = qore_get_complex_hard_reference_type;
        elem = spec.getComplexReference();
    }
    if (make) {
        const QoreTypeInfo* telem = tombType(elem, depth);
        if (failed) {
            return nullptr;
        }
        return telem == elem ? ti : make(telem);
    }
    if (qore_type_names_user_decl(ti, 0)) {
        return fail();
    }
    return ti;
}

QoreClass* QoreTombstoneBuilder::tombClass(const QoreClass* qc, int depth) {
    qore_class_private* p = qore_class_private::get(*const_cast<QoreClass*>(qc));
    if (p->sys) {
        return const_cast<QoreClass*>(qc);
    }
    if (p->type_owner.type_owner_tombstone) {
        return static_cast<QoreClass*>(const_cast<void*>(p->type_owner.type_owner_tombstone));
    }
    {
        auto i = building.find(p);
        if (i != building.end()) {
            return static_cast<QoreClass*>(i->second);
        }
    }
    if (failed || depth > QORE_TOMBSTONE_MAX_DEPTH) {
        failed = true;
        return nullptr;
    }
    std::string fp;
    {
        std::vector<const void*> saved;
        saved.swap(stack);
        fpClassRef(fp, p, 0);
        stack.swap(saved);
    }
    if (failed) {
        return nullptr;
    }
    {
        auto i = r.classes.find(fp);
        if (i != r.classes.end()) {
            cache(p->type_owner, i->second);
            return i->second;
        }
    }
    // an empty class equal to this one (see the top of the file); builtin, so it is never retired
    QoreClass* t = new QoreClass(p->name.c_str(), p->path.c_str());
    qore_class_private* tp = qore_class_private::get(*t);
    tp->sys = true;
    tp->committed = true;
    r.classes[fp] = t;
    r.tombs.insert(t);
    new_classes.push_back(fp);
    building[p] = t;

    tp->type_params = p->type_params;
    tp->raw_accepts_parameterized = p->raw_accepts_parameterized;
    tp->raw_construction_defaults_to_auto = p->raw_construction_defaults_to_auto;
    for (const QoreTypeInfo* b : p->type_param_bound_types) {
        tp->type_param_bound_types.push_back(tombType(b, depth + 1));
    }
    for (const QoreTypeInfo* d : p->type_param_default_types) {
        tp->type_param_default_types.push_back(tombType(d, depth + 1));
    }
    if (p->scl) {
        for (BCNode* n : *p->scl) {
            QoreClass* parent = n->sclass ? tombClass(n->sclass, depth + 1) : nullptr;
            if (!parent) {
                failed = true;
                return nullptr;
            }
            // as for a builtin class: the class holds its base classes (the tombstone of a user class is immortal)
            if (n->is_virtual) {
                t->addBuiltinVirtualBaseClass(parent);
            } else {
                t->addBuiltinBaseClass(parent);
            }
            BCNode* tn = tp->scl->back();
            tn->access = n->access;
            tn->type_info = tombType(n->type_info, depth + 1);
        }
        for (const QoreTypeInfo* vp : p->parameterized_vparents) {
            tp->parameterized_vparents.push_back(tombType(vp, depth + 1));
        }
    }
    if (failed) {
        return nullptr;
    }
    if (p->hash) {
        tp->hash = p->hash;
    }
    tp->initialized = true;
    tp->hierarchy_initialized = true;
    cache(p->type_owner, t);
    return t;
}

TypedHashDecl* QoreTombstoneBuilder::tombHashDecl(const TypedHashDecl* hd, int depth) {
    typed_hash_decl_private* p = typed_hash_decl_private::get(*const_cast<TypedHashDecl*>(hd));
    if (p->isTypeOwnerSystem()) {
        return const_cast<TypedHashDecl*>(hd);
    }
    if (p->type_owner.type_owner_tombstone) {
        return static_cast<TypedHashDecl*>(const_cast<void*>(p->type_owner.type_owner_tombstone));
    }
    {
        auto i = building.find(p);
        if (i != building.end()) {
            return static_cast<TypedHashDecl*>(i->second);
        }
    }
    if (failed || depth > QORE_TOMBSTONE_MAX_DEPTH) {
        failed = true;
        return nullptr;
    }
    if (p->parameterized_base) {
        // the concrete declaration of the generic hashdecl's tombstone, which caches it
        TypedHashDecl* base = tombHashDecl(p->parameterized_base, depth + 1);
        if (!base) {
            failed = true;
            return nullptr;
        }
        type_vec_t args;
        for (const QoreTypeInfo* arg : p->type_args) {
            args.push_back(tombType(arg, depth + 1));
        }
        if (failed) {
            return nullptr;
        }
        const TypedHashDecl* rv = typed_hash_decl_private::get(*base)->getParameterizedHashDecl(args);
        if (!rv) {
            failed = true;
            return nullptr;
        }
        r.tombs.insert(rv);
        cache(p->type_owner, rv);
        return const_cast<TypedHashDecl*>(rv);
    }
    std::string fp;
    {
        std::vector<const void*> saved;
        saved.swap(stack);
        fpHashDeclRef(fp, p, 0);
        stack.swap(saved);
    }
    if (failed) {
        return nullptr;
    }
    {
        auto i = r.hashdecls.find(fp);
        if (i != r.hashdecls.end()) {
            cache(p->type_owner, i->second);
            return i->second;
        }
    }
    TypedHashDecl* t = new TypedHashDecl(p->name.c_str(), p->path.c_str());
    typed_hash_decl_private* tp = typed_hash_decl_private::get(*t);
    tp->sys = true;
    tp->parse_init_done = true;
    tp->type_params = p->type_params;
    r.hashdecls[fp] = t;
    r.tombs.insert(t);
    new_hashdecls.push_back(fp);
    building[p] = t;
    if (p->parentHashDecl) {
        TypedHashDecl* parent = tombHashDecl(p->parentHashDecl, depth + 1);
        if (!parent) {
            failed = true;
            return nullptr;
        }
        tp->setParentHashDecl(parent);
    }
    for (auto& i : p->members.member_list) {
        const QoreTypeInfo* mt = i.second ? tombType(i.second->getTypeInfo(), depth + 1) : nullptr;
        if (failed) {
            return nullptr;
        }
        tp->addMember(i.first, mt, i.second ? tombDefault(i.second->exp) : QoreValue());
    }
    cache(p->type_owner, t);
    return t;
}

QoreEnumDecl* QoreTombstoneBuilder::tombEnum(const QoreEnumDecl* ed) {
    qore_enum_decl_private* p = qore_enum_decl_private::get(*const_cast<QoreEnumDecl*>(ed));
    if (p->isTypeOwnerSystem()) {
        return const_cast<QoreEnumDecl*>(ed);
    }
    if (p->type_owner.type_owner_tombstone) {
        return static_cast<QoreEnumDecl*>(const_cast<void*>(p->type_owner.type_owner_tombstone));
    }
    if (failed) {
        return nullptr;
    }
    std::string fp;
    fpEnumRef(fp, p);
    if (failed) {
        return nullptr;
    }
    auto i = r.enums.find(fp);
    if (i != r.enums.end()) {
        cache(p->type_owner, i->second);
        return i->second;
    }
    QoreEnumDecl* t = new QoreEnumDecl(p->getName(), p->getPath(), p->getBaseTypeInfo());
    qore_enum_decl_private* tp = qore_enum_decl_private::get(*t);
    tp->sys = true;
    tp->parse_init_done = true;
    for (const QoreEnumMember* m : p->getMembers()) {
        t->addMember(m->getName(), m->getValue().refSelf());
    }
    r.enums[fp] = t;
    r.tombs.insert(t);
    new_enums.push_back(fp);
    cache(p->type_owner, t);
    return t;
}
const QoreClass* qore_class_tombstone(const QoreClass* qc) {
    QoreTombstoneRegistry& r = tombstone_registry();
    std::lock_guard<std::mutex> lock(r.m);
    if (r.freed) {
        return nullptr;
    }
    QoreTombstoneBuilder b(r);
    QoreClass* rv = b.tombClass(qc, 0);
    return b.failed ? nullptr : rv;
}

const TypedHashDecl* qore_hashdecl_tombstone(const TypedHashDecl* hd) {
    QoreTombstoneRegistry& r = tombstone_registry();
    std::lock_guard<std::mutex> lock(r.m);
    if (r.freed) {
        return nullptr;
    }
    QoreTombstoneBuilder b(r);
    TypedHashDecl* rv = b.tombHashDecl(hd, 0);
    return b.failed ? nullptr : rv;
}

const QoreEnumDecl* qore_enum_tombstone(const QoreEnumDecl* ed) {
    QoreTombstoneRegistry& r = tombstone_registry();
    std::lock_guard<std::mutex> lock(r.m);
    if (r.freed) {
        return nullptr;
    }
    QoreTombstoneBuilder b(r);
    QoreEnumDecl* rv = b.tombEnum(ed);
    return b.failed ? nullptr : rv;
}

const QoreTypeInfo* qore_type_tombstone(const QoreTypeInfo* ti, bool& ok) {
    QoreTombstoneRegistry& r = tombstone_registry();
    std::lock_guard<std::mutex> lock(r.m);
    if (r.freed) {
        ok = false;
        return ti;
    }
    QoreTombstoneBuilder b(r);
    const QoreTypeInfo* rv = b.tombType(ti, 0);
    ok = !b.failed;
    return ok ? rv : ti;
}

#ifdef DEBUG
std::mutex* qore_type_tombstone_dbg_fork_lock() {
    return &tombstone_registry().m;
}
#endif

bool qore_is_type_tombstone(const void* decl) {
    QoreTombstoneRegistry& r = tombstone_registry();
    std::lock_guard<std::mutex> lock(r.m);
    return r.tombs.count(decl) > 0;
}

void qore_type_tombstone_counts(size_t& classes, size_t& hashdecls, size_t& enums) {
    QoreTombstoneRegistry& r = tombstone_registry();
    std::lock_guard<std::mutex> lock(r.m);
    classes = r.classes.size();
    hashdecls = r.hashdecls.size();
    enums = r.enums.size();
}

void qore_free_type_tombstones() {
    std::map<std::string, QoreClass*> classes;
    std::map<std::string, TypedHashDecl*> hashdecls;
    std::map<std::string, QoreEnumDecl*> enums;
    {
        QoreTombstoneRegistry& r = tombstone_registry();
        std::lock_guard<std::mutex> lock(r.m);
        r.freed = true;
        classes.swap(r.classes);
        hashdecls.swap(r.hashdecls);
        enums.swap(r.enums);
        r.tombs.clear();
    }
    // the tombstones refer to each other only by references that their destruction releases (base classes, parent
    // hashdecls), or by pointers that are not read when they are freed
    for (auto& i : hashdecls) {
        typed_hash_decl_private::get(*i.second)->deref();
    }
    for (auto& i : enums) {
        qore_enum_decl_private::get(*i.second)->deref();
    }
    for (auto& i : classes) {
        qore_class_private::get(*i.second)->deref(true, true, true);
    }
}

bool qore_type_needs_decl_pin(const QoreTypeInfo* ti) {
    if (!ti) {
        return false;
    }
    // on the path of every generic call (CodeEvaluationHelper::setTypeParamInstantiation()): classified with the
    // type's flag, not a dynamic_cast (issue #6126)
    if (ti->isComplexCode()) {
        const QoreComplexCodeTypeInfo* cti = static_cast<const QoreComplexCodeTypeInfo*>(ti);
        if (qore_type_needs_decl_pin(cti->getReturnType())) {
            return true;
        }
        for (const QoreTypeInfo* p : cti->getParamTypes()) {
            if (qore_type_needs_decl_pin(p)) {
                return true;
            }
        }
        return false;
    }
    return qore_type_names_user_decl(ti, 0);
}

bool qore_match_common_type_pinned(const QoreTypeInfo*& ctype, const QoreTypeInfo* ntype, QoreTypeDeclPin& pin) {
    const QoreTypeInfo* old = ctype;
    bool rv = QoreTypeInfo::matchCommonType(ctype, ntype);
    if (ctype != old) {
        pin.set(ctype);
    }
    return rv;
}

void qore_type_owner_make_tombstone(QoreTypeOwner* owner) {
    const QoreTypeInfo* ti = owner->getTypeOwnerType();
    if (!ti) {
        return;
    }
    bool ok;
    qore_type_tombstone(ti, ok);
}
