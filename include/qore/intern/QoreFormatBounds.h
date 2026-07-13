/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreFormatBounds.h

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
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

    Note: the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_QOREFORMATBOUNDS_H
#define _QORE_QOREFORMATBOUNDS_H

#include <map>

//! bounds the value formatting done by the standard node getAsString() methods
/** the bounds are applied while the output is generated, so that the time and the memory used to format a value
    are bounded no matter how large the value is; a context is active for the current thread while a value is
    formatted with one of the bounded formatting functions

    containers are expanded at most once; the second and any further reference to a container is rendered as a
    YAML alias to the anchor rendered with the first, which is what makes the cost of formatting a value linear
    in the size of the value and not in the number of paths through it

    @see
    - q_format_bounded()
    - q_vsprintf_bounded()
    - QoreFormatBoundsHelper
*/
class QoreFormatBoundsContext {
public:
    DLLLOCAL QoreFormatBoundsContext(const QoreFormatBounds& bounds) : bounds(bounds) {
    }

    //! counts the references to each container in the value; must be called before the value is formatted
    DLLLOCAL void scan(const QoreValue val);

    //! called before a container is expanded
    /** renders an anchor for a container referenced more than once, an alias for a container already rendered,
        and the elision string given if the container cannot be expanded due to the bounds

        @return 0 if the container must be expanded, 1 if it was rendered here
    */
    DLLLOCAL int enterContainer(QoreString& str, const AbstractQoreNode* node, const char* elision);

    //! called when a container expanded after enterContainer() returned 0 has been rendered
    DLLLOCAL void exitContainer() {
        assert(depth);
        --depth;
    }

    //! called before each element of a container is rendered
    /** @param str the output string
        @param count the number of elements of the container already rendered
        @param total the number of elements in the container

        @return True if no more elements may be rendered; the elision marker is rendered in this case
    */
    DLLLOCAL bool elideElements(QoreString& str, size_t count, size_t total);

    //! renders a string subject to the byte limit
    /** @return 0 if the string must be rendered by the caller, 1 if it was rendered here
    */
    DLLLOCAL int renderString(QoreString& str, const QoreStringNode* val, ExceptionSink* xsink);

private:
    typedef std::map<const AbstractQoreNode*, unsigned> nmap_t;

    //! the maximum number of containers visited in the scan pass
    /** the byte limit bounds the cost of formatting the value in any case; exceeding this limit only means that
        containers not visited are rendered with an anchor even if they are referenced only once
    */
    static constexpr unsigned max_scan_nodes = 100000;

    const QoreFormatBounds& bounds;

    //! how many times each container is referenced in the value (scan pass)
    nmap_t counts;

    //! the anchor ID rendered for each container already expanded
    nmap_t anchors;

    //! the output string being generated and its size when formatting started
    const QoreString* out = nullptr;
    size_t start = 0;

    //! the current container nesting depth
    unsigned depth = 0;

    //! the last anchor ID rendered
    unsigned last_anchor = 0;

    //! set when any output has been elided
    bool truncated = false;

    DLLLOCAL void scanIntern(const QoreValue val, unsigned depth, unsigned& nodes);

    //! returns the number of bytes of output left in the budget; SIZE_MAX if the budget is unlimited
    DLLLOCAL size_t remaining(const QoreString& str) {
        if (!bounds.max_bytes) {
            return SIZE_MAX;
        }
        if (!out) {
            // the budget applies to the output generated for the value being formatted
            out = &str;
            start = str.size();
        }
        assert(out == &str);
        size_t used = str.size() - start;
        return used >= bounds.max_bytes ? 0 : bounds.max_bytes - used;
    }

    //! returns True if the byte limit has been reached; renders the truncation marker in this case
    DLLLOCAL bool overBudget(QoreString& str) {
        if (remaining(str)) {
            return false;
        }
        if (!truncated) {
            truncated = true;
            str.concat("...<truncated>");
        }
        return true;
    }
};

//! sets the format bounds context for the current thread while formatting a value
class QoreFormatBoundsContextHelper {
public:
    DLLLOCAL QoreFormatBoundsContextHelper(QoreFormatBoundsContext* ctx) : old_ctx(thread_get_format_bounds()) {
        thread_set_format_bounds(ctx);
    }

    DLLLOCAL ~QoreFormatBoundsContextHelper() {
        thread_set_format_bounds(old_ctx);
    }

private:
    QoreFormatBoundsContext* old_ctx;
};

//! applies the format bounds context of the current thread, if any, to a container being formatted
/** used in the getAsString() implementations of the container types; has no effect if no format bounds context
    is active in the current thread
*/
class QoreFormatBoundsHelper {
public:
    //! renders any anchor, alias, or elision marker for the container in the string
    /** @param str the output string
        @param node the container being formatted
        @param elision the string rendered in place of the container if it cannot be expanded
    */
    DLLLOCAL QoreFormatBoundsHelper(QoreString& str, const AbstractQoreNode* node, const char* elision)
            : ctx(thread_get_format_bounds()) {
        if (ctx) {
            if (ctx->enterContainer(str, node, elision)) {
                done = true;
                ctx = nullptr;
            } else {
                entered = true;
            }
        }
    }

    DLLLOCAL ~QoreFormatBoundsHelper() {
        if (entered) {
            ctx->exitContainer();
        }
    }

    //! returns True if the container was rendered by the helper and must not be expanded by the caller
    DLLLOCAL bool elided() const {
        return done;
    }

    //! returns True if no more elements of the container may be rendered
    DLLLOCAL bool elideElements(QoreString& str, size_t count, size_t total) {
        return ctx ? ctx->elideElements(str, count, total) : false;
    }

private:
    QoreFormatBoundsContext* ctx;
    bool entered = false;
    bool done = false;
};

#endif
