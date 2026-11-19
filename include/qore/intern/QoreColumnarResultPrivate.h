/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_columnar_result_private.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_INTERN_QORECOLUMNARRESULTPRIVATE_H
#define _QORE_INTERN_QORECOLUMNARRESULTPRIVATE_H

#include <qore/QoreColumnarResult.h>

#include <utility>

//! The library-internal operations of QoreColumnarResult (qore#4380)
/** These functions were non-inline DLLLOCAL members of the public QoreColumnarResult class: they were never exported, so
    they are not part of the module API and live here, in the implementation of the library.  Construct the object
    from the columnar result and call the operation:

    @code
    qore_columnar_result_private(result).attach(self);
    @endcode

    The object holds only the pointer to the columnar result, and it is a friend of QoreColumnarResult.
*/
class qore_columnar_result_private {
public:
    //! the columnar result
    QoreColumnarResult* b;

    DLLLOCAL explicit qore_columnar_result_private(QoreColumnarResult* n_b) : b(n_b) {
    }

    //! for a holder of the columnar result (ReferenceHolder, SimpleRefHolder, ...)
    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_columnar_result_private(H& holder) : qore_columnar_result_private(holder.operator->()) {
    }

    //! for the operations that are const members of the columnar result
    DLLLOCAL explicit qore_columnar_result_private(const QoreColumnarResult* n_b) : b(const_cast<QoreColumnarResult*>(n_b)) {
    }

    //! Creates a columnar result from a hash-of-lists given by a script (ColumnarResult::fromColumnHash())
    /** As fromColumnHash(), except that a value in a column stored as a dense integer buffer must be an int, float,
        number, numeric string (an optional sign and decimal digits, with optional surrounding whitespace, whose value
        fits in an int64), NULL or NOTHING; any other value raises \c COLUMNAR-RESULT-ERROR.  A float or number is
        converted as by QoreValue::getAsBigInt() (truncated toward zero).
    */
    DLLLOCAL static QoreColumnarResult* fromScriptColumnHash(const QoreHashNode* columns, const QoreHashNode* desc,
        ExceptionSink* xsink);

    //! Creates a columnar result from row hashes given by a script (ColumnarResult::fromRows())
    /** Values are checked as by fromScriptColumnHash().
    */
    DLLLOCAL static QoreColumnarResult* fromScriptRows(const QoreListNode* rows, const QoreHashNode* desc,
        ExceptionSink* xsink);

    //! Called once the result is the private data of the object: the object reports its column containers
    /** @since %Qore 3.0 */
    DLLLOCAL void attach(QoreObject& self) const;

    DLLLOCAL int setRowCount(size_t n_rows, ExceptionSink* xsink);

    //! Adds a column; @a flags select the conversion and the check of the schema, which raises @a err
    DLLLOCAL int addColumnIntern(const char* name, QoreValue data, const QoreColumnarTypeDescriptor& schema,
        int flags, const char* err, ExceptionSink* xsink);

    //! Creates a columnar result from a hash-of-lists, checking script values if @a script_values is true
    DLLLOCAL static QoreColumnarResult* fromColumnHashIntern(const QoreHashNode* columns, const QoreHashNode* desc,
        bool script_values, ExceptionSink* xsink);

    //! Creates a columnar result from row hashes, checking script values if @a script_values is true
    DLLLOCAL static QoreColumnarResult* fromRowsIntern(const QoreListNode* rows, const QoreHashNode* desc,
        bool script_values, ExceptionSink* xsink);
};

#endif // _QORE_INTERN_QORECOLUMNARRESULTPRIVATE_H
