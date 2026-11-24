/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_list_private.h

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

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_QORELISTPRIVATE_H
#define _QORE_QORELISTPRIVATE_H

#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include "qore/intern/qore_container_free.h"
#include "qore/intern/LexicalSemanticPolicy.h"
#include "qore/intern/QoreSandboxMemory.h"
#include "qore/intern/QoreAllocFailure.h"

typedef ReferenceHolder<QoreListNode> safe_qorelist_t;

#define LIST_PAD   15

struct qore_list_private {
    QoreValue* entry = nullptr;
    size_t length = 0;
    size_t allocated = 0;
    unsigned obj_count = 0;
    const QoreTypeInfo* complexTypeInfo = nullptr;
    QoreContainerWeakRefs weakRefs;
    std::unique_ptr<std::vector<size_t>> eval_pos_map;
    size_t eval_result_size = 0;
    //! the sandbox memory charge of entry (see design/sandbox-resource-limits.md); synced with allocated
    QoreSandboxChargeRecord sb;
    bool finalized : 1;
    bool vlist : 1;
    //! false if the list cannot be changed: it was freed, or it is a copy of a freed list (see copyFreedState())
    bool valid : 1;
    //! true once the list has been freed (see QoreListNode::derefImpl())
    bool freed : 1;
    //! true if the weak holders of the list hold one reference of their own since its free started (see
    //! QoreContainerWeakRefs); not in the bit field, as lockWeak() reads it while the free writes the bit field
    bool weak_hold = false;

    DLLLOCAL qore_list_private() : finalized(false), vlist(false), valid(true), freed(false) {
    }

    DLLLOCAL ~qore_list_private() {
        assert(!length);

        sb.release();
        if (entry) {
            free(entry);
        }
    }

    DLLLOCAL static QoreListNode* newList(bool needs_eval) {
        // Use autoTypeInfo to prevent type stripping when entries are evaluated.
        // Without it, push() → stripVal() → copy_strip_complex_types() strips
        // hashdecl types from evaluated hash entries (e.g., hash<PipelineOptionInfo>
        // becomes plain hash).
        QoreListNode* l = new QoreListNode(autoTypeInfo);
        if (needs_eval) {
            l->value = false;
            l->needs_eval_flag = true;
        }
        return l;
    }

    DLLLOCAL const QoreTypeInfo* getValueTypeInfo() const {
        return complexTypeInfo ? QoreTypeInfo::getComplexListValueType(complexTypeInfo) : nullptr;
    }

    DLLLOCAL const QoreTypeInfo* getTypeInfo() const {
        return complexTypeInfo ? complexTypeInfo : listTypeInfo;
    }

    DLLLOCAL void getTypeName(QoreString& str) const {
        if (complexTypeInfo)
            str.concat(QoreTypeInfo::getName(complexTypeInfo));
        else
            str.concat("list");
    }

    DLLLOCAL int checkValid(ExceptionSink* xsink) {
        if (!valid) {
            if (xsink) {
                xsink->raiseException("LIST-ERROR", "Cannot modify a list that has already gone out of scope");
            }
            return -1;
        }
        return 0;
    }

    DLLLOCAL QoreListNode* getCopy() const {
        QoreListNode* l = new QoreListNode;
        if (complexTypeInfo) {
            l->priv->complexTypeInfo = complexTypeInfo;
        }
        return l;
    }

    DLLLOCAL QoreListNode* getEmptyCopy(bool is_value) const {
        QoreListNode* l = newList(!is_value);
        if (complexTypeInfo) {
            l->priv->complexTypeInfo = complexTypeInfo;
        }
        return l;
    }

    DLLLOCAL QoreListNode* copyCheckNewElementType(const QoreTypeInfo* newElementType) const {
        QoreListNode* rv = copyValue();
        rv->priv->setListTypeFromNewElementType(newElementType);
        return rv;
    }

    DLLLOCAL void setListTypeFromNewElementType(const QoreTypeInfo* newElementType) {
        complexTypeInfo = getListTypeForNewElementType(complexTypeInfo, length, newElementType);
    }

    //! returns the type of a list of type \a list_type with \a length elements after a value of type
    //! \a newElementType is added to it (at either end)
    DLLLOCAL static const QoreTypeInfo* getListTypeForNewElementType(const QoreTypeInfo* list_type, size_t length,
            const QoreTypeInfo* newElementType) {
        const QoreTypeInfo* orig_ctype, * ctype;
        orig_ctype = ctype = QoreTypeInfo::getUniqueReturnComplexList(list_type);
        // Treat 'auto' as unset only for true first-element initialization.
        // An already-populated list<auto> may contain heterogeneous values;
        // appending or concatenating a typed value must not retroactively
        // narrow the whole list to the new element's type.
        if (ctype == autoTypeInfo && !length) {
            ctype = newElementType;
            if (ctype && ctype != autoTypeInfo && ctype != anyTypeInfo) {
                return qore_get_complex_list_type(ctype);
            }
            return list_type;
        }
        if ((!ctype || ctype == anyTypeInfo) && (!newElementType || newElementType == anyTypeInfo)) {
            return nullptr;
        }
        if (QoreTypeInfo::matchCommonType(ctype, newElementType)) {
            if (ctype != orig_ctype) {
                return qore_get_complex_list_type(ctype);
            }
            return list_type;
        }
        return autoListTypeInfo;
    }

    //! returns a copy of the list converted to another type: a new list, which can be changed also when this list was
    //! freed under a weak reference, as the copy of a hash converted to another type (qore#7199; see copyFreedState())
    DLLLOCAL QoreListNode* copy(const QoreTypeInfo* newComplexTypeInfo) const {
        QoreListNode* l = new QoreListNode;
        l->priv->complexTypeInfo = newComplexTypeInfo;
        copyIntern(*l->priv);
        return l;
    }

    //! A copy of a freed list cannot be changed either (qore#7163)
    /** A list freed under weak references is held by its weak holders (see QoreContainerWeakRefs), so a new
        reference to it is never its only one, and changing the list through that reference copies it; the copy reads
        as empty, as the list does, and changing it raises \c LIST-ERROR, as changing the list itself does.  A value
        computed from it (sorted, or with an element added: see copyValue()) is a new list, and so is its conversion to
        another list type (a cast or the list given to a variable or parameter of another type: getPlainList(),
        newComplexListFromValue()), which can be changed (qore#7199).

        The copy is not freed: it is freed as any other list when its last reference is released.
    */
    DLLLOCAL void copyFreedState(qore_list_private& l) const {
        if (!valid) {
            l.valid = false;
        }
    }

    // strip = copy without type information
    DLLLOCAL QoreListNode* copy(bool strip = false) const {
        // issue #2791 perform type stripping at the source
        if (!strip || !complexTypeInfo) {
            QoreListNode* l = getCopy();
            copyIntern(*l->priv);
            copyFreedState(*l->priv);
            return l;
        }
        QoreListNode* l = new QoreListNode;
        l->priv->reserve(length);
        for (size_t i = 0; i < length; ++i) {
            l->priv->pushIntern(copy_strip_complex_types(entry[i]));
        }
        copyFreedState(*l->priv);
        return l;
    }

    //! Returns a copy of the list to compute a new value from (ex: to sort), which can be changed also when the list
    //! was freed (see copyFreedState())
    DLLLOCAL QoreListNode* copyValue() const {
        QoreListNode* l = getCopy();
        copyIntern(*l->priv);
        return l;
    }

    DLLLOCAL void copyIntern(qore_list_private& l) const {
        l.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            l.pushIntern(entry[i].refSelf());
        }
    }

    DLLLOCAL QoreListNode* concatenate(const QoreListNode* l, ExceptionSink* xsink) const {
        // issue #3429: maintain types unless we have a plain list; convert to list<auto> if the types are not compatible
        ReferenceHolder<QoreListNode> rv(copyCheckNewElementType(QoreTypeInfo::getUniqueReturnComplexList(l->priv->complexTypeInfo)), xsink);
        return mergeWithoutTypeConversion(rv, *l->priv);
    }

    DLLLOCAL QoreListNode* concatenateElement(QoreValue e, ExceptionSink* xsink) const {
        // issue #3429: maintain types unless we have a plain list; convert to list<auto> if the types are not compatible
        ReferenceHolder<QoreListNode> rv(copyCheckNewElementType(e.getTypeInfo()), xsink);
        if (rv->priv->pushWithoutTypeConversion(e, xsink)) {
            return nullptr;
        }
        return rv.release();
    }

    DLLLOCAL QoreListNode* prependElement(QoreValue e, ExceptionSink* xsink) const {
        // issue #3429: maintain types unless we have a plain list; convert to list<auto> if the types are not compatible
        // the type of the new list is that of this list with the new element added, as for concatenateElement()
        // (qore#5936: the list type was given as the element type of the new list, which then had no type in common
        // with the element, so element + list<int> gave list<auto>)
        ReferenceHolder<QoreListNode> rv(new QoreListNode, xsink);
        rv->priv->complexTypeInfo = getListTypeForNewElementType(complexTypeInfo, length, e.getTypeInfo());
        if (rv->priv->pushWithoutTypeConversion(e, xsink)) {
            return nullptr;
        }
        // add the rest of the list elements
        return mergeWithoutTypeConversion(rv, *this);
    }

    DLLLOCAL int checkVal(ValueHolder& holder, ExceptionSink* xsink) const {
        return checkVal(holder, nullptr, xsink);
    }

    //! checks and converts a value for the list as checkVal() above; \a vtype is the type of the value, if known
    DLLLOCAL int checkVal(ValueHolder& holder, const QoreTypeInfo* vtype, ExceptionSink* xsink) const {
        return checkValNativeImpl<false>(holder, vtype, xsink);
    }

    DLLLOCAL int checkValNative(ValueHolder& holder, ExceptionSink* xsink) const {
        return checkValNativeImpl<true>(holder, nullptr, xsink);
    }

    template <bool Native>
    DLLLOCAL int checkValNativeImpl(ValueHolder& holder, const QoreTypeInfo* vtype, ExceptionSink* xsink) const {
        if (complexTypeInfo) {
            const QoreTypeInfo* vti = QoreTypeInfo::getUniqueReturnComplexList(complexTypeInfo);
            // the value's type is only needed for an element type; a value of the element type itself needs no
            // conversion: tested inline, before the full comparison, which runs for every element assigned
            // (qore#6628; superSetOf() itself is unchanged here, see qore#6575)
            const QoreTypeInfo* src_ti;
            if (QoreTypeInfo::hasType(vti) && vti != (src_ti = vtype ? vtype : holder->getTypeInfo())
                    && !QoreTypeInfo::superSetOf(vti, src_ti)) {
                QoreValue v(holder.release());
                // the outcome must reflect only this conversion; see ScopedTypeCheckSink.  Without this, a caller
                // with a long-lived sink silently drops elements after an unrelated error
                ScopedTypeCheckSink ts(xsink);
                if constexpr (Native) {
                    QoreTypeInfo::acceptAssignment(vti, "<list element assignment>", v, *ts,
                        qore_get_native_semantic_policy(), QLVTS_List);
                } else {
                    QoreTypeInfo::acceptAssignment(vti, "<list element assignment>", v, *ts, QLVTS_List);
                }
                holder = v;
                if (ts.raised()) {
                    (*ts)->appendLastDescription(" (while converting types for list<%s> subtype)",
                        QoreTypeInfo::getName(vti));
                    return -1;
                }
                return 0;
            }
            return 0;
        } else {
            return stripVal(holder, xsink);
        }
        return 0;
    }

    DLLLOCAL int checkVal(ValueHolder& holder, const QoreTypeInfo* vtype, ExceptionSink* xsink,
            LexicalSemanticPolicy policy) const {
        if (complexTypeInfo) {
            const QoreTypeInfo* vti = QoreTypeInfo::getUniqueReturnComplexList(complexTypeInfo);
            // the value's type is only needed for an element type; a value of the element type itself needs no
            // conversion: tested inline, before the full comparison, which runs for every element assigned
            // (qore#6628; superSetOf() itself is unchanged here, see qore#6575)
            const QoreTypeInfo* src_ti;
            if (QoreTypeInfo::hasType(vti) && vti != (src_ti = vtype ? vtype : holder->getTypeInfo())
                    && !QoreTypeInfo::superSetOf(vti, src_ti)) {
                QoreValue v(holder.release());
                // the outcome must reflect only this conversion; see ScopedTypeCheckSink.  Without this, a caller
                // with a long-lived sink silently drops elements after an unrelated error
                ScopedTypeCheckSink ts(xsink);
                QoreTypeInfo::acceptAssignment(vti, "<list element assignment>", v, *ts, policy, QLVTS_List);
                holder = v;
                if (ts.raised()) {
                    (*ts)->appendLastDescription(" (while converting types for list<%s> subtype)",
                        QoreTypeInfo::getName(vti));
                    return -1;
                }
                return 0;
            }
            return 0;
        } else {
            return stripVal(holder, xsink);
        }
        return 0;
    }

    DLLLOCAL int stripVal(ValueHolder& holder, ExceptionSink* xsink) const {
        switch (holder->getType()) {
            case NT_LIST:
            case NT_HASH: {
                // the outcome must reflect only this strip (the deref of the old value); see checkVal() above
                ScopedTypeCheckSink ts(xsink);
                {
                    ValueHolder v(holder.release(), *ts);
                    holder = copy_strip_complex_types(*v);
                }
                return ts.raised() ? -1 : 0;
            }
            default:
                break;
        }
        return 0;
    }

    // assumes types are compatible; performs type stripping if necessary
    DLLLOCAL int pushWithoutTypeConversion(QoreValue val, ExceptionSink* xsink) {
        if (!complexTypeInfo) {
            return pushStrip(val, xsink);
        }
        pushIntern(val);
        return 0;
    }

    DLLLOCAL int pushStrip(QoreValue val, ExceptionSink* xsink) {
        ValueHolder holder(val, xsink);
        if (stripVal(holder, xsink)) {
            return -1;
        }
        pushIntern(holder.release());
        return 0;
    }

    DLLLOCAL int push(QoreValue val, ExceptionSink* xsink) {
        return push(val, nullptr, xsink);
    }

    //! Pushes a value using the source operation's conversion policy.
    DLLLOCAL int push(QoreValue val, ExceptionSink* xsink, LexicalSemanticPolicy policy) {
        ValueHolder holder(val, xsink);
        if (checkVal(holder, nullptr, xsink, policy)) {
            return -1;
        }
        pushIntern(holder.release());
        return 0;
    }

    //! Public-list mutation checks with a source-bound conversion entry.
    DLLLOCAL static int push(QoreListNode& list, QoreValue val, ExceptionSink* xsink, LexicalSemanticPolicy policy);

    DLLLOCAL static int insert(QoreListNode& list, QoreValue val, ExceptionSink* xsink,
            LexicalSemanticPolicy policy);

    //! Native public APIs consult the bridge only when the existing type check requires a conversion.
    DLLLOCAL int pushNative(QoreValue val, ExceptionSink* xsink) {
        ValueHolder holder(val, xsink);
        if (checkValNative(holder, xsink)) return -1;
        pushIntern(holder.release());
        return 0;
    }

    DLLLOCAL int mergeNative(const QoreListNode* list, ExceptionSink* xsink) {
        // a list freed under a weak reference cannot be changed (qore#7199)
        if (checkValid(xsink)) {
            return -1;
        }
        reserve(length + list->size());
        ConstListIterator i(list);
        while (i.next()) {
            if (pushNative(i.getReferencedValue(), xsink)) return -1;
        }
        return 0;
    }

    DLLLOCAL static int pushCanonical(QoreListNode& list, QoreValue val, ExceptionSink* xsink);
    DLLLOCAL static int insertCanonical(QoreListNode& list, QoreValue val, ExceptionSink* xsink);
    DLLLOCAL static int mergeCanonical(QoreListNode& list, const QoreListNode* other, ExceptionSink* xsink) {
        return list.priv->merge(other, xsink);
    }

    //! pushes a value as push() above; \a vtype is the type of the value, if known
    DLLLOCAL int push(QoreValue val, const QoreTypeInfo* vtype, ExceptionSink* xsink) {
        ValueHolder holder(val, xsink);
        if (checkVal(holder, vtype, xsink)) {
            return -1;
        }
        pushIntern(holder.release());
        return 0;
    }

    DLLLOCAL int merge(const QoreListNode* list, ExceptionSink* xsink) {
        // a list freed under a weak reference cannot be changed (qore#7199)
        if (checkValid(xsink)) {
            return -1;
        }
        reserve(length + list->size());
        ConstListIterator i(list);
        while (i.next()) {
            if (push(i.getReferencedValue(), xsink))
                return -1;
        }
        return 0;
    }

    DLLLOCAL int merge(const QoreListNode* list, ExceptionSink* xsink, LexicalSemanticPolicy policy) {
        // a list freed under a weak reference cannot be changed (qore#7199)
        if (checkValid(xsink)) {
            return -1;
        }
        reserve(length + list->size());
        ConstListIterator i(list);
        size_t count = 0;
        while (i.next()) {
            if (++count % 100 == 0 && qore_check_cancel(xsink, "list merge")) {
                return -1;
            }
            if (push(i.getReferencedValue(), xsink, policy)) {
                return -1;
            }
        }
        return 0;
    }

    // fast merge without type conversion; performs type stripping if necessary
    DLLLOCAL static QoreListNode* mergeWithoutTypeConversion(ReferenceHolder<QoreListNode>& rv, const qore_list_private& list) {
        for (size_t i = 0; i < list.length; ++i) {
            QoreValue v = list.entry[i];
            if (!rv->priv->complexTypeInfo) {
                v = copy_strip_complex_types(v);
            } else {
                v.refSelf();
            }
            rv->priv->pushIntern(v);
        }

        return rv.release();
    }

    //! When all or none of the occupied slots need scanning, the count already classifies this slot.
    DLLLOCAL bool entryNeedsScan(const QoreValue& slot) const {
        return obj_count && (obj_count == length || needs_scan(slot));
    }

    //! Move one reference between argument lists without classifying the same value twice.
    DLLLOCAL void moveEntryTo(size_t index, qore_list_private& dest, size_t dest_index) {
        assert(this != &dest && index < length);
        QoreValue& target = dest.getEntryReference(dest_index);
        assert(target.isNothing());
        if (entryNeedsScan(entry[index])) {
            incScanCount(-1);
            dest.incScanCount(1);
        }
        target = entry[index];
        entry[index].clear();
    }

    //! Initialize a new slot, possibly out of order when binding named arguments.
    DLLLOCAL void initEntry(size_t index, QoreValue val) {
        QoreValue& slot = getEntryReference(index);
        assert(slot.isNothing());
        slot = val;
        if (needs_scan(val)) {
            incScanCount(1);
        }
    }

    DLLLOCAL void pushIntern(QoreValue val) {
        initEntry(length, val);
    }

    DLLLOCAL size_t checkOffset(ptrdiff_t offset) {
        if (offset < 0) {
            offset = length + offset;
            return offset < 0 ? 0 : offset;
        } else if ((size_t)offset > length)
            return length;

        return offset;
    }

    DLLLOCAL void checkOffset(ptrdiff_t offset, ptrdiff_t len, size_t &n_offset, size_t &n_len) {
        n_offset = checkOffset(offset);
        if (len < 0) {
            len = length + len - n_offset;
            n_len = len < 0 ? 0 : len;
            return;
        }
        n_len = len;
    }

    QoreValue spliceSingle(size_t offset) {
        assert(offset < length);

        QoreValue rv = entry[offset];
        if (needs_scan(rv)) {
            incScanCount(-1);
        }

        size_t end = offset + 1;

        if (end != length) {
            memmove(static_cast<void*>(entry + offset), entry + end, sizeof(QoreValue) * (length - end));
            // zero out trailing entries
            zeroEntries(length - 1, length);
        } else // set last entry to 0
            entry[end - 1] = QoreValue();

        if (length > 0) {
            /* it is always greater than one but we need get rid with realloc's warning
             * "....exceeds maximum object size 9223372036854775807" when optimizer somehow
             * testing probably length bounds. Casting to size_t i.e. long unsigned int does not help
             */
            resize(length - 1);
        }

        return rv;
    }

    DLLLOCAL QoreListNode* spliceIntern(size_t offset, size_t len, bool extract) {
        //printd(5, "spliceIntern(offset: %d, len: %d, length: %d)\n", offset, len, length);
        size_t end;
        if (len > (length - offset)) {
            end = length;
            len = length - offset;
        } else
            end = offset + len;

        QoreListNode* rv = extract ? getCopy() : nullptr;

        // dereference all entries that will be removed or add to return value list
        for (size_t i = offset; i < end; i++) {
            removeEntry(entry[i], rv);
        }

        // move down entries if necessary
        if (end != length) {
            memmove(static_cast<void*>(entry + offset), entry + end, sizeof(QoreValue) * (length - end));
            // zero out trailing entries
            zeroEntries(length - len, length);
        } else // set last entry to 0
            entry[end - 1] = QoreValue();

        resize(length - len);

        return rv;
    }

    //! Replacement entry for source-bound AST operations; compiled callers use the canonical overload.
    DLLLOCAL static QoreListNode* replace(QoreListNode& list, ptrdiff_t offset, ptrdiff_t length, QoreValue value,
            bool extract, ExceptionSink* xsink, LexicalSemanticPolicy policy) {
        assert(list.reference_count() == 1);
        size_t n_offset, n_length;
        list.priv->checkOffset(offset, length, n_offset, n_length);
        return list.priv->spliceIntern<true>(n_offset, n_length, value, extract, xsink, policy);
    }

    DLLLOCAL static QoreListNode* replace(QoreListNode& list, ptrdiff_t offset, ptrdiff_t length, QoreValue value,
            bool extract, ExceptionSink* xsink) {
        assert(list.reference_count() == 1);
        size_t n_offset, n_length;
        list.priv->checkOffset(offset, length, n_offset, n_length);
        return list.priv->spliceIntern(n_offset, n_length, value, extract, xsink);
    }

    template <bool Lexical = false, bool Native = false>
    DLLLOCAL QoreListNode* spliceIntern(size_t offset, size_t len, const QoreValue l, bool extract,
            ExceptionSink* xsink, LexicalSemanticPolicy policy = {}) {
        auto check = [&](ValueHolder& value) {
            if constexpr (Native) {
                return checkValNative(value, xsink);
            } else if constexpr (Lexical) {
                return checkVal(value, nullptr, xsink, policy);
            } else {
                return checkVal(value, xsink);
            }
        };
        // check type compatibility before modifying the list
        ValueHolder holder(xsink);
        if (l.getType() == NT_LIST) {
            // create a new temporary list and check types first
            const qore_list_private* sl = l.get<QoreListNode>()->priv;
            QoreListNode* tmp;
            holder = tmp = sl->getCopy();
            tmp->priv->reserve(sl->length);
            for (size_t i = 0; i < sl->length; ++i) {
                ValueHolder eh(sl->entry[i].refSelf(), xsink);
                if (check(eh)) {
                    return nullptr;
                }
                tmp->priv->pushIntern(eh.release());
            }
        } else {
            holder = l.refSelf();
            if (check(holder)) {
                return nullptr;
            }
        }

        //printd(5, "spliceIntern(offset: %d, len: %d, length: %d)\n", offset, len, length);
        size_t end;
        if (len > (length - offset)) {
            end = length;
            len = length - offset;
        } else
            end = offset + len;

        QoreListNode* rv = extract ? getCopy() : nullptr;

        // dereference all entries that will be removed or add to return value list
        for (size_t i = offset; i < end; i++) {
            removeEntry(entry[i], rv);
        }

        // get number of entries to insert
        size_t n = l.getType() == NT_LIST ? l.get<const QoreListNode>()->size() : 1;
        // difference
        if (n > len) { // make bigger
            size_t ol = length;
            resize(length - len + n);
            // move trailing entries forward if necessary
            if (end != ol) {
                memmove(static_cast<void*>(entry + (end - len + n)), entry + end, sizeof(QoreValue) * (ol - end));
            }
        } else if (len > n) { // make list smaller
            // move the trailing entries down: only the length - end entries after the removed range exist
            memmove(static_cast<void*>(entry + offset + n), entry + end, sizeof(QoreValue) * (length - end));
            // zero out trailing entries
            zeroEntries(length - (len - n), length);
            // resize list
            resize(length - (len - n));
        }

        // add in new entries
        if (l.getType() != NT_LIST) {
            entry[offset] = holder.release();
            if (needs_scan(entry[offset]))
                incScanCount(1);
        }
        else {
            qore_list_private* lst = holder->get<const QoreListNode>()->priv;
            // Transfer the references and their scan count together.
            for (size_t i = 0; i < n; ++i) {
                entry[offset + i] = lst->entry[i];
                lst->entry[i].clear();
            }
            obj_count += lst->obj_count;
            lst->obj_count = 0;
            lst->length = 0;
        }

        return rv;
    }

    DLLLOCAL QoreValue& getEntryReference(size_t num) {
        if (num >= length) {
            resize(num + 1);
        }
        return entry[num];
    }

    DLLLOCAL QoreValue getAndClear(size_t i) {
        if (i >= length) {
            return QoreValue();
        }
        QoreValue rv = entry[i];
        entry[i] = QoreValue();

        if (needs_scan(rv)) {
            incScanCount(-1);
        }

        return rv;
    }

    DLLLOCAL static QoreListNode* getPlainList(QoreListNode* l) {
        if (!l->priv->complexTypeInfo)
            return l;
        // no exception is possible
        ReferenceHolder<QoreListNode> holder(l, nullptr);
        // the list converted to a plain list is a new list, which can be changed also when the list was freed under
        // a weak reference (qore#7199; see copyFreedState())
        QoreListNode* rv = l->priv->copy(true);
        rv->priv->valid = true;
        return rv;
    }

    DLLLOCAL QoreValue swap(qore_offset_t offset, QoreValue val) {
        QoreValue& p = getEntryReference((size_t)offset);

        bool before = needs_scan(p);
        bool after = needs_scan(val);
        if (before) {
            if (!after)
                --obj_count;
        }
        else if (after) {
            ++obj_count;
        }

        QoreValue rv = p;
        p = val;

        return rv;
    }

    DLLLOCAL QoreValue takeExists(size_t offset) {
        if (offset >= length) {
            return QoreValue();
        }

        QoreValue rv = entry[offset];
        // Transfer every owned representation, including inline enum references.
        entry[offset].clear();

        // no value of a list whose scan count is zero needs a scan (see QoreListNode::shift()); incScanCount()
        // asserts the count of a value that does
        assert(obj_count || !needs_scan(rv));
        if (obj_count && needs_scan(rv)) {
            incScanCount(-1);
        }

        return rv;
    }

    DLLLOCAL void reserve(size_t num) {
        if (num < length)
            return;
        // make larger
        if (num >= allocated) {
            grow(num);
        }
    }

    //! grows the list's storage to hold \a num entries; memory that cannot be allocated fails the operation with
    //! std::bad_alloc and leaves the list's entries unchanged (the storage was replaced with a null pointer, and the
    //! next write ended the process; qore#7466)
    DLLLOCAL void grow(size_t num) {
        size_t d = num >> 2;
        allocated = num + (d < LIST_PAD ? LIST_PAD : d);
        QoreValue* p = static_cast<QoreValue*>(qore_realloc_or_null(static_cast<void*>(entry),
            sizeof(QoreValue) * allocated));
        if (__builtin_expect(!p, 0)) {
            // the list keeps its storage, which holds at least its entries; the capacity is set only on success, so
            // that the code that does not fail stays as it was
            allocated = length;
            qore_throw_bad_alloc();
        }
        entry = p;
        sb.sync(sizeof(QoreValue) * allocated);
    }

    DLLLOCAL void resize(size_t num) {
        if (num < length) { // make smaller
            //entry = (QoreValue*)realloc(entry, sizeof(QoreValue*) * num);
            length = num;
            return;
        }
        // make larger
        if (num >= length) {
            if (num >= allocated) {
                grow(num);
            }
            zeroEntries(length, num);
        }
        length = num;
    }

    DLLLOCAL void zeroEntries(size_t start, size_t end) {
        for (size_t i = start; i < end; ++i) {
            entry[i] = QoreValue();
        }
    }

    DLLLOCAL void removeEntry(QoreValue& v, QoreListNode*& rv) {
        if (needs_scan(v)) {
            incScanCount(-1);
        }
        if (!rv)
            rv = new QoreListNode(autoTypeInfo);
        rv->priv->pushIntern(v);
    }

    DLLLOCAL int getLValue(size_t ind, LValueHelper& lvh, bool for_remove, ExceptionSink* xsink);

    //! raises RUNTIME-TYPE-ERROR for an assignment to element \a ind past the end of the list that would leave a gap
    /** the elements between the end of the list and \a ind would have no value (NOTHING), which a declared element
        type that does not take NOTHING refuses (ex: list<int>); an assignment at the end of the list leaves no gap,
        and a list whose element type takes NOTHING (list<*int>, list<auto>, an untyped list) pads the gap
        (qore#6435)

        @return -1 if an exception was raised, 0 if not
    */
    DLLLOCAL int checkAssignmentGap(size_t ind, ExceptionSink* xsink) const {
        return ind <= length || !elementTypeRejectsNothing() ? 0 : raiseAssignmentGapError(ind, xsink);
    }

    //! raises RUNTIME-TYPE-ERROR for delete or remove of element \a ind of a list whose element type does not take
    //! NOTHING held by an lvalue whose declared type \a holder_type requires the element type (qore#6435)
    DLLLOCAL void raiseElementRemovalError(size_t ind, bool for_del, const QoreTypeInfo* holder_type,
            ExceptionSink* xsink) const;

    //! Raises LIST-ERROR for delete or remove that removes nothing from a list freed under a weak reference or from a
    //! copy of one (qore#7163), which is empty; the removers call it only when they remove nothing (qore#7306)
    /** @return -1 if the list cannot be changed and LIST-ERROR was raised, 0 if not
    */
    DLLLOCAL int checkRemovalValid(ExceptionSink* xsink) {
        // a list whose last reference was released is empty, and so is a copy of it
        assert(valid || !length);
        return checkValid(xsink);
    }

    //! QoreListNode::extract(ptrdiff_t, ptrdiff_t) for a list that can be shared: the list of a weak reference,
    //! which a remove changes in place (qore#7306)
    DLLLOCAL QoreListNode* extract(ptrdiff_t offset, ptrdiff_t len);

    //! raises RUNTIME-TYPE-ERROR for delete or remove of element \a ind of a list whose element type does not take
    //! NOTHING reached through a weak reference, which keeps the type that its owners declared (qore#7306)
    DLLLOCAL void raiseWeakElementRemovalError(size_t ind, bool for_del, ExceptionSink* xsink) const;

    //! makes the element type of the list its or-nothing type (list<*int> for a list<int>) if its element type does
    //! not take NOTHING, as for delete or remove of an element of a list held by an lvalue that allows it (qore#6435)
    //! and for the list that remove of a range past the end of a list gives (qore#6434)
    DLLLOCAL void setOrNothingElementType() {
        if (complexTypeInfo) {
            complexTypeInfo = QoreTypeInfo::getOrNothingListType(complexTypeInfo);
        }
    }

    //! returns true if the element type of the list does not take NOTHING
    DLLLOCAL bool elementTypeRejectsNothing() const {
        return complexTypeInfo && !QoreTypeInfo::listElementTakesNothing(complexTypeInfo);
    }

private:
    //! raises the error of checkAssignmentGap() for a list whose element type does not take NOTHING
    DLLLOCAL __attribute__((noinline, cold)) int raiseAssignmentGapError(size_t ind, ExceptionSink* xsink) const;


public:

    // mergesort for controlled and interruptible sorts (stable)
    DLLLOCAL int mergesort(const ResolvedCallReferenceNode* fr, bool ascending, ExceptionSink* xsink);

    // quicksort for controlled and interruptible sorts (unstable)
    DLLLOCAL int qsort(const ResolvedCallReferenceNode* fr, size_t left, size_t right, bool ascending,
            ExceptionSink* xsink);

    DLLLOCAL void incScanCount(int dt) {
        assert(dt);
        assert(obj_count || (dt > 0));
        //printd(5, "qore_list_private::incScanCount() this: %p dt: %d: %d -> %d\n", this, dt, obj_count, obj_count + dt);
        obj_count += dt;
    }

    DLLLOCAL QoreListNode* eval(ExceptionSink* xsink);

    DLLLOCAL void setCallArgEvalMap(std::vector<size_t>&& pos_map, size_t result_size) {
        eval_pos_map = std::make_unique<std::vector<size_t>>(std::move(pos_map));
        eval_result_size = result_size;
    }

    DLLLOCAL bool hasCallArgEvalMap() const {
        return static_cast<bool>(eval_pos_map);
    }

    //! gives this list (a copy of the argument list of a named call) the parameter positions of the arguments of
    //! \a src, if any
    DLLLOCAL void copyCallArgEvalMap(const qore_list_private& src) {
        if (src.eval_pos_map) {
            eval_pos_map = std::make_unique<std::vector<size_t>>(*src.eval_pos_map);
            eval_result_size = src.eval_result_size;
        }
    }

    DLLLOCAL bool callArgEvalMapHasHoles() const {
        return eval_pos_map && eval_pos_map->size() < eval_result_size;
    }

    DLLLOCAL const std::vector<size_t>* getCallArgEvalMap() const {
        return eval_pos_map.get();
    }

    DLLLOCAL size_t getCallArgEvalResultSize() const {
        return eval_result_size;
    }

    DLLLOCAL bool weakDeref() {
        if (weakRefs.ROdereference()) {
            return true;
        }
        return false;
    }

    //! Frees the next entry of a list whose reference count has reached zero
    /** @param l the list
        @param next the index of the next entry, which is incremented before the entry is freed
        @param xsink for exceptions raised while freeing the entry

        @return false if all entries have been freed
    */
    DLLLOCAL static bool freeNextEntry(QoreListNode& l, size_t& next, ExceptionSink* xsink) {
        qore_list_private* p = l.priv;
        if (next == p->length) {
            return false;
        }
        QoreValue& entry = p->entry[next++];
        qore_container_free_helper::freeEntry(entry, xsink);
        return true;
    }

    //! Finishes freeing a list after its entries have been freed
    DLLLOCAL static void finishFree(QoreListNode& l) {
        qore_list_private* p = l.priv;
        p->length = 0;
        p->obj_count = 0;
        if (!p->weak_hold) {
            // no weak holder can take a reference to the list (see QoreContainerWeakRefs)
            delete &l;
            return;
        }
        // its weak holders keep the node allocated, and must read it as an empty list, not as the entries that no
        // longer exist
        p->valid = false;
        p->freed = true;
        l.weakDeref();
    }

    //! Returns a new reference to a list that a weak reference holds, or a new empty list that cannot be changed if
    //! its last reference was released (qore#7198; see WeakListReferenceNode::lock())
    /** A reference is never taken to a list whose free has started: its count is zero then, or its weak holders hold
        one reference of their own (\c weak_hold, see QoreContainerWeakRefs), which is taken after \c weak_hold is
        set; a reference taken while the list is referenced keeps its free from starting.  So \c weak_hold is read
        after the count was raised from a value that is not zero: either its value is that of the free, or no free can
        set it meanwhile.
    */
    DLLLOCAL static QoreListNode* lockWeak(QoreListNode& l) {
        // the reference is taken as QoreReferenceCounter::optRef() takes it, inline
        int count = l.references.load();
        while (count > 0) {
            if (l.references.compare_exchange_weak(count, count + 1)) {
                return !l.priv->weak_hold ? &l : lockWeakFreeing(l);
            }
        }
        return getFreedStandIn(l);
    }

    //! Releases the reference that lockWeak() took to a list whose free has started, and returns a new empty list
    /** The reference is not the list's last, as the caller holds a weak reference, and therefore its weak holders hold
        theirs.
    */
    DLLLOCAL static __attribute__((noinline)) QoreListNode* lockWeakFreeing(QoreListNode& l) {
#ifdef DEBUG
        bool last =
#endif
        l.ROdereference();
        assert(!last);
        return getFreedStandIn(l);
    }

    //! Returns a new empty list with the type of a list whose last reference was released, which cannot be changed
    //! (qore#7198; see lockWeak())
    DLLLOCAL static __attribute__((noinline)) QoreListNode* getFreedStandIn(const QoreListNode& l) {
        QoreListNode* rv = l.priv->getCopy();
        rv->priv->valid = false;
        return rv;
    }

    DLLLOCAL static void setNeedsEval(QoreListNode& l) {
        l.needs_eval_flag = true;
        l.value = false;
    }

    DLLLOCAL static int parseInitComplexListInitialization(const QoreProgramLocation* loc,
            QoreParseContext& parse_context, QoreParseListNode* args, QoreValue& new_args);

    DLLLOCAL static int parseInitListInitialization(const QoreProgramLocation* loc, QoreParseContext& parse_context,
            QoreParseListNode* args, QoreValue& new_args, int& err);

    DLLLOCAL static int parseCheckComplexListInitialization(const QoreProgramLocation* loc,
            const QoreTypeInfo* typeInfo, const QoreTypeInfo* expTypeInfo, const QoreValue exp,
            const char* context_action, bool strict_check = true);

    DLLLOCAL static int parseCheckTypedAssignment(const QoreProgramLocation* loc, const QoreValue arg,
            const QoreTypeInfo* vti, const char* context_action, bool strict_check = true);

    DLLLOCAL static QoreListNode* newComplexList(const QoreTypeInfo* typeInfo, const QoreValue args,
            ExceptionSink* xsink, LexicalSemanticPolicy policy);

    DLLLOCAL static QoreListNode* newComplexList(const QoreTypeInfo* typeInfo, const QoreValue args,
            ExceptionSink* xsink);

    // caller owns any reference in "init"
    DLLLOCAL static QoreListNode* newComplexListFromValue(const QoreTypeInfo* typeInfo, QoreValue init,
            ExceptionSink* xsink);

    DLLLOCAL static QoreListNode* newComplexListFromValue(const QoreTypeInfo* typeInfo, QoreValue init,
            ExceptionSink* xsink, LexicalSemanticPolicy policy);

    //! returns a typed list made by its constructor from the list of its arguments, which are its elements
    /** an argument that is not a valid element raises the error of getConstructorArgErrorDesc(), as at parse time
        (issue #6172); the caller owns any reference in "args"
    */
    DLLLOCAL static QoreListNode* newComplexListFromArgs(const QoreTypeInfo* typeInfo, QoreValue args,
            ExceptionSink* xsink);

    DLLLOCAL static QoreListNode* newComplexListFromArgs(const QoreTypeInfo* typeInfo, QoreValue args,
            ExceptionSink* xsink, LexicalSemanticPolicy policy);

    //! returns the description of the error of an argument of the constructor of a typed list that is not a valid
    //! element of the list, the same at parse time and at run time (issue #6172)
    /** @param vti the element type of the list
        @param index the position of the argument (from 0)
        @param arg_ti the type of the argument
    */
    DLLLOCAL static QoreStringNode* getConstructorArgErrorDesc(const QoreTypeInfo* vti, size_t index,
            const QoreTypeInfo* arg_ti);

    //! checks the arguments of the constructor of a typed list at parse time: they are the elements of the list
    DLLLOCAL static int parseCheckConstructorArgs(const QoreProgramLocation* loc, const QoreTypeInfo* vti,
            const QoreTypeInfo* args_type_info, const QoreValue args);

    template <bool Lexical>
    DLLLOCAL static QoreListNode* newComplexListFromValueImpl(const QoreTypeInfo* typeInfo, QoreValue init,
            ExceptionSink* xsink, LexicalSemanticPolicy policy, bool ctor_args = false);

    //! checks an argument of the constructor of a typed list at run time and converts it to the element type
    /** @return 0 for OK, -1 if an exception was raised
    */
    template <bool Lexical>
    DLLLOCAL static int acceptConstructorArg(const QoreTypeInfo* vti, size_t index, QoreValue& arg,
            ExceptionSink* xsink, LexicalSemanticPolicy policy);

    DLLLOCAL static const qore_list_private* get(const QoreListNode& l) {
        return l.priv;
    }

    DLLLOCAL static qore_list_private* get(QoreListNode& l) {
        return l.priv;
    }

    DLLLOCAL static const qore_list_private* get(const QoreListNode* l) {
        return l ? l->priv : nullptr;
    }

    DLLLOCAL static qore_list_private* get(QoreListNode* l) {
        return l ? l->priv : nullptr;
    }

    DLLLOCAL bool isFinalized() const {
        return finalized;
    }

    DLLLOCAL void setFinalized() {
        finalized = true;
    }

    DLLLOCAL bool isVariableList() const {
        return vlist;
    }

    DLLLOCAL void setVariableList() {
        vlist = true;
    }

    DLLLOCAL static unsigned getScanCount(const QoreListNode& l) {
        return l.priv->obj_count;
    }

    DLLLOCAL static void incScanCount(const QoreListNode& l, int dt) {
        l.priv->incScanCount(dt);
    }
};

//! Tracks a conversion that updates an entry through a reference, including its exceptional exits.
class QoreListEntryEdit {
public:
    DLLLOCAL QoreListEntryEdit(qore_list_private& list, size_t index)
            : list(list), value(list.getEntryReference(index)), scanned(list.entryNeedsScan(value)) {
    }

    DLLLOCAL ~QoreListEntryEdit() {
        bool now = needs_scan(value);
        if (now != scanned) {
            list.incScanCount(now ? 1 : -1);
        }
    }

    QoreListEntryEdit(const QoreListEntryEdit&) = delete;
    QoreListEntryEdit& operator=(const QoreListEntryEdit&) = delete;

    DLLLOCAL QoreValue& get() { return value; }

private:
    qore_list_private& list;
    QoreValue& value;
    bool scanned;
};

#endif
