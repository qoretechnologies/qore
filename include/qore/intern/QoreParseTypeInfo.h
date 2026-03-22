/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreParseTypeInfo.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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

    Note that the Qore library is dual-licensed under LGPL and MIT licenses; see
    LICENSE.LGPL and LICENSE.MIT in the source code directory for details.
*/

#ifndef _QORE_QOREPARSETYPE_INFO_H
#define _QORE_QOREPARSETYPE_INFO_H

#include "qore/intern/NamedScope.h"
#include "qore/intern/QoreTypeInfo.h"

class QoreParseTypeInfo;
typedef std::vector<QoreParseTypeInfo*> parse_type_vec_t;

// this is basically just a wrapper around NamedScope
class QoreParseTypeInfo {
protected:
    std::string tname;

    DLLLOCAL QoreParseTypeInfo(const NamedScope* n_cscope) : cscope(n_cscope->copy()), or_nothing(false) {
        setName();
    }

    DLLLOCAL void setName() {
        if (or_nothing)
            tname = "*";
        tname += cscope->ostr;
        if (!subtypes.empty()) {
            tname += '<';
            tname += subtypes[0]->getName();
            for (unsigned i = 1; i < subtypes.size(); ++i) {
                tname += ", ";
                tname += subtypes[i]->getName();
            }
            tname += '>';
        }
    }

public:
    NamedScope* cscope; // namespace scope for class
    parse_type_vec_t subtypes;
    bool or_nothing;

    DLLLOCAL QoreParseTypeInfo(char* n_cscope, bool n_or_nothing = false) : cscope(new NamedScope(n_cscope)),
            or_nothing(n_or_nothing) {
        setName();
        //printd(5, "QoreParseTypeInfo::QoreParseTypeInfo() %s\n", tname.c_str());
    }

    DLLLOCAL QoreParseTypeInfo(char* n_cscope, bool n_or_nothing, parse_type_vec_t&& subtypes)
            : cscope(new NamedScope(n_cscope)), subtypes(subtypes), or_nothing(n_or_nothing) {
        setName();

        //printd(5, "QoreParseTypeInfo::QoreParseTypeInfo() %s\n", tname.c_str());
    }

    DLLLOCAL QoreParseTypeInfo(const QoreParseTypeInfo& old) : tname(old.tname),
            cscope(old.cscope ? new NamedScope(*old.cscope) : nullptr), or_nothing(old.or_nothing) {
        // copy subtypes
        for (const auto& i : old.subtypes)
            subtypes.push_back(new QoreParseTypeInfo(*i));
    }

    DLLLOCAL ~QoreParseTypeInfo() {
        delete cscope;
        for (auto& i : subtypes)
            delete i;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static bool parseStageOneIdenticalWithParsed(const QoreParseTypeInfo* pti, const QoreTypeInfo* typeInfo,
            bool& recheck) {
        if (pti && typeInfo)
            return pti->parseStageOneIdenticalWithParsed(typeInfo, recheck);
        else if (pti)
            return false;
        else if (typeInfo)
            return false;
        else
            return true;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static bool parseStageOneIdentical(const QoreParseTypeInfo* pti, const QoreParseTypeInfo* typeInfo,
            bool& recheck) {
        if (pti && typeInfo)
            return pti->parseStageOneIdentical(typeInfo, recheck);
        else
            return !(pti || typeInfo);
    }

    // static version of method, checking for null pointer
    DLLLOCAL static const QoreTypeInfo* resolveAndDelete(QoreParseTypeInfo* pti, const QoreProgramLocation* loc,
            int& err) {
        return pti ? pti->resolveAndDelete(loc, err) : nullptr;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static const QoreTypeInfo* resolve(QoreParseTypeInfo* pti, const QoreProgramLocation* loc, int& err) {
        return pti ? pti->resolve(loc, err) : nullptr;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static const QoreTypeInfo* resolveAny(QoreParseTypeInfo* pti, const QoreProgramLocation* loc, int& err) {
        return pti ? pti->resolveAny(loc, err) : nullptr;
    }

    DLLLOCAL static const QoreTypeInfo* resolveRuntime(QoreParseTypeInfo* pti) {
        return pti ? pti->resolveRuntime() : nullptr;
    }

#ifdef DEBUG
    DLLLOCAL const char* getCID() const { return cscope ? cscope->getIdentifier() : "n/a"; }

    // static version of method, checking for null pointer
    DLLLOCAL static const char* getCID(const QoreParseTypeInfo* pti) { return pti ? pti->getCID() : "n/a"; }
#endif

    DLLLOCAL QoreParseTypeInfo* copy() const {
        return new QoreParseTypeInfo(cscope);
    }

    // static version of method, checking for null pointer
    DLLLOCAL static const char* getName(const QoreParseTypeInfo* pti) {
        return pti ? pti->getName() : NO_TYPE_INFO;
    }

    // static version of method, checking for null pointer
    DLLLOCAL static void concatName(const QoreParseTypeInfo* pti, std::string& str) {
        if (pti)
            pti->concatName(str);
        else
            str.append(NO_TYPE_INFO);
    }

private:
    // used when parsing user code to find duplicate signatures
    DLLLOCAL bool parseStageOneIdenticalWithParsed(const QoreTypeInfo* typeInfo, bool& recheck) const {
        if (!typeInfo)
            return false;

        const QoreClass* qc = QoreTypeInfo::getUniqueReturnClass(typeInfo);
        if (!qc) {
            const TypedHashDecl* hd = QoreTypeInfo::getUniqueReturnHashDecl(typeInfo);
            if (!hd) {
                const QoreTypeInfo* ti = QoreTypeInfo::getUniqueReturnComplexHash(typeInfo);
                if (!ti) {
                    return false;
                }
                if (subtypes.size() == 2 && !strcmp(cscope->getIdentifier(), "hash"))
                    return recheck = true;
                return false;
            }
            if (subtypes.size() == 1 && !strcmp(cscope->getIdentifier(), "hash"))
                return recheck = true;
            return false;
        }

        // both have class info
        if (!strcmp(cscope->getIdentifier(), qc->getName()))
            return recheck = true;
        return false;
    }

    // used when parsing user code to find duplicate signatures
    DLLLOCAL bool parseStageOneIdentical(const QoreParseTypeInfo* typeInfo, bool& recheck) const {
        //printd(5, "QoreParseTypeInfo::parseStageOneIdentical() this: %p '%s' == %p '%s'\n", this, tname.c_str(), typeInfo, typeInfo->tname.c_str());
        if (tname == typeInfo->tname) {
            return true;
        }
        // issue #3861: check if they could potentially refer to the same declaration; if the shorter string is the same as the
        // longer string, and the longer string is prefixed by "::", then the declarations are ambiguous and must be rechecked
        if (tname.size() > typeInfo->tname.size()) {
            recheck = checkAmbiguous(tname, typeInfo->tname);
        } else if (typeInfo->tname.size() > tname.size()) {
            recheck = checkAmbiguous(typeInfo->tname, tname);
        }
        return false;
    }

    DLLLOCAL static bool checkAmbiguous(const std::string& longer, const std::string& shorter) {
        // if the previous two character in longer are not '::', then the strings are not ambiguous
        if (longer.size() - shorter.size() < 2) {
            return false;
        }
        if (longer.compare(longer.size() - shorter.size() - 2, 2, "::")) {
            return false;
        }
        return !longer.compare(longer.size() - shorter.size(), shorter.size(), shorter);
    }

    // resolves complex types (classes, hashdecls, etc)
    DLLLOCAL const QoreTypeInfo* resolve(const QoreProgramLocation* loc, int& err) const;
    // also resolves base types
    DLLLOCAL const QoreTypeInfo* resolveAny(const QoreProgramLocation* loc, int& err) const;
    // resolves the current type to an QoreTypeInfo pointer and deletes itself
    DLLLOCAL const QoreTypeInfo* resolveAndDelete(const QoreProgramLocation* loc, int& err);
    DLLLOCAL const QoreTypeInfo* resolveSubtype(const QoreProgramLocation* loc, int& err) const;

    DLLLOCAL const QoreTypeInfo* resolveRuntime() const;
    DLLLOCAL const QoreTypeInfo* resolveRuntimeSubtype() const;
    DLLLOCAL static const QoreTypeInfo* resolveRuntimeClass(const NamedScope& cscope, bool or_nothing);

    DLLLOCAL const char* getName() const {
        return tname.c_str();
    }

    DLLLOCAL void concatName(std::string& str) const {
        assert(!tname.empty());
        str.append(tname);
    }

    DLLLOCAL static const QoreTypeInfo* resolveClass(const QoreProgramLocation* loc, const NamedScope& cscope,
            bool or_nothing, int& err);
};

#endif
