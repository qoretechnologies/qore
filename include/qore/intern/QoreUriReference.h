/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreUriReference.h

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

#ifndef _QORE_INTERN_QOREURIREFERENCE_H
#define _QORE_INTERN_QOREURIREFERENCE_H

#include <qore/common.h>

#include <string>

class ExceptionSink;

//! An RFC 3986 URI reference split into its five components
/** Every operation works on octets: percent-encoded octets, reserved delimiters, and non-ASCII octets are carried
    through unchanged, so a reference is never decoded or re-encoded as a side effect of resolving it.

    A component can be undefined or defined and empty; the two are distinguished with the \c has_* flags because
    RFC 3986 section 5.3 recomposition depends on the difference (\c "http://h/p?" keeps its empty query).
*/
struct QoreUriReference {
    std::string scheme;
    std::string authority;
    std::string path;
    std::string query;
    std::string fragment;

    bool has_scheme = false;
    bool has_authority = false;
    bool has_query = false;
    bool has_fragment = false;

    //! Parses a URI reference according to RFC 3986 appendix B
    /** Parsing never fails; a component that does not match the RFC 3986 grammar is kept as given.  A scheme is
        recognized only if it matches the RFC 3986 \c scheme production, so a relative path such as \c "a b:c" is not
        mistaken for a URI with a scheme.

        @param str the reference
        @param len the length of the reference in bytes
        @param xsink if not nullptr, parsing stops early when the thread is cancelled, and the exception is raised
        here
    */
    DLLLOCAL void parse(const char* str, size_t len, ExceptionSink* xsink = nullptr);

    //! Recomposes the reference according to RFC 3986 section 5.3
    /** @param include_fragment if false, any fragment is omitted
        @param encode if true, octets that cannot appear in a URI are percent-encoded in the path, query, and
        fragment; see appendEncoded()
        @param xsink if not nullptr, the operation stops early when the thread is cancelled, and the exception is
        raised here

        A path that starts with \c "//" is prefixed with \c "/." if there is no authority, so that it is not taken
        for one when the result is parsed again
    */
    DLLLOCAL std::string compose(bool include_fragment = true, bool encode = false,
            ExceptionSink* xsink = nullptr) const;

    //! Resolves @p ref against this reference per RFC 3986 section 5.2.2
    /** The strict transformation is used: a reference with a scheme is never treated as relative.  This object's
        fragment is ignored, as required for a base URI.

        This object is normally an absolute URI.  It can also be a relative reference, as for an XML Base that has
        no absolute ancestor; the target is then a relative reference as well, and \c ".." segments that cannot be
        removed from a relative path are kept.

        @param ref the reference to resolve
        @param xsink if not nullptr, the operation stops early when the thread is cancelled, and the exception is
        raised here
    */
    DLLLOCAL QoreUriReference resolve(const QoreUriReference& ref, ExceptionSink* xsink = nullptr) const;

    //! Returns true if this is a relative-path reference whose first segment contains a colon
    /** Such a reference is not valid (RFC 3986 section 4.2), as it would be taken for a URI with a scheme
    */
    DLLLOCAL bool hasColonInFirstRelativeSegment() const;

    //! Appends @p in to @p out, percent-encoding octets that cannot appear in a URI
    /** Controls, space, non-ASCII octets, and the characters \c "<>\\^`{|}" are encoded (RFC 3987 section 3.1); existing
        percent-encoded octets and all URI delimiters are kept as given
    */
    DLLLOCAL static void appendEncoded(std::string& out, const std::string& in, ExceptionSink* xsink = nullptr);

    //! Returns the HTTP request target for this reference: the path (\c "/" if empty) and any query
    DLLLOCAL std::string getRequestTarget() const;

    //! Returns true if the scheme is defined and matches @p str case-insensitively
    DLLLOCAL bool isScheme(const char* str) const;

    //! Removes dot segments from a path according to RFC 3986 section 5.2.4
    DLLLOCAL static std::string removeDotSegments(const std::string& path, ExceptionSink* xsink = nullptr);

    //! Removes dot segments from a relative path, keeping \c ".." segments that cannot be removed
    /** RFC 3986 section 5.2.4 is only defined for the paths of absolute URIs; applied to a relative path it drops
        leading \c ".." segments and can make the path absolute.  This variant keeps the path relative.
    */
    DLLLOCAL static std::string removeRelativeDotSegments(const std::string& path, ExceptionSink* xsink = nullptr);

    //! Returns true if @p str is a syntactically valid RFC 3986 scheme
    DLLLOCAL static bool isValidScheme(const char* str, size_t len);

    //! Returns true if @p authority can be used as the authority of a request
    /** Controls, space, the characters \c "\"<>\\^`{|}", and malformed percent-encoded octets are rejected, so that
        the host cannot inject data into a request; non-ASCII octets are accepted, as in IRIs
    */
    DLLLOCAL static bool isValidAuthority(const std::string& authority);
};

#endif
