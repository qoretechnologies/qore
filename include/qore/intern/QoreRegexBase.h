/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreRegexBase.h

    regular expression substitution node definition

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

#ifndef _QORE_REGEXBASE_H

#define _QORE_REGEXBASE_H

#define PCRE2_CODE_UNIT_WIDTH 8

// base class for regex and regex substitution classes
#include <pcre2.h>
#include <string>

class ExceptionSink;

//! Size of error buffer for PCRE2 error messages
constexpr size_t qore_pcre2_errorbuf_size = 512;

//! Initial size of the per-thread PCRE2 JIT stack
constexpr size_t qore_pcre2_jit_stack_start_size = 32 * 1024;

//! Maximum size of the per-thread PCRE2 JIT stack
/** The JIT falls back to the interpreter (@ref PCRE2_ERROR_JIT_STACKLIMIT) if a match needs more
    backtracking stack than this.
 */
constexpr size_t qore_pcre2_jit_stack_max_size = 1024 * 1024;

//! Returns true for PCRE2 match return codes that terminate an iteration without an internal bug.
/** Covers @ref PCRE2_ERROR_NOMATCH and the full range of UTF-8 subject-validation errors
    (@ref PCRE2_ERROR_UTF8_ERR1 through @ref PCRE2_ERROR_UTF8_ERR21). When the subject string is
    tagged as UTF-8 but contains invalid byte sequences, pcre2_match() can return any code in that
    range (we have observed @c -22 from HTTP bodies masked via @c http_mask_data). All such codes
    indicate "we cannot match past this byte", so the match loop stops cleanly instead of
    asserting.

    Resource-limit codes (@ref PCRE2_ERROR_MATCHLIMIT, @ref PCRE2_ERROR_DEPTHLIMIT,
    @ref PCRE2_ERROR_HEAPLIMIT) are deliberately \a not covered here; they mean the engine gave up
    without deciding the match, so returning "no match" for them would be a silently wrong answer.
    See qore_pcre2_check_match_error().
 */
static inline bool qore_pcre2_expected_match_error(int rc) {
    // PCRE2_ERROR_UTF8_ERR1 = -3 (highest), PCRE2_ERROR_UTF8_ERR21 = -23 (lowest) — see pcre2.h
    return rc == PCRE2_ERROR_NOMATCH
        || (rc <= PCRE2_ERROR_UTF8_ERR1 && rc >= PCRE2_ERROR_UTF8_ERR21);
}

//! Executes a PCRE2 match with the calling thread's match context.
/** The thread's match context carries a heap-allocated JIT stack, which both keeps the JIT's
    backtracking stack off the machine stack (PCRE2 otherwise uses a 32 KB machine-stack buffer)
    and allows it to grow to @ref qore_pcre2_jit_stack_max_size.

    If the JIT still runs out of stack (@ref PCRE2_ERROR_JIT_STACKLIMIT), the match is transparently
    retried with the interpreter, which uses heap frames bounded by PCRE2's heap limit instead.

    @param code the compiled pattern
    @param subject the subject string
    @param length the length of the subject string in bytes
    @param startoffset the byte offset in the subject where matching starts
    @param options match options (ex: @ref PCRE2_NO_UTF_CHECK)
    @param md the match data block

    @return the pcre2_match() return code
 */
DLLLOCAL int qore_pcre2_match(const pcre2_code* code, PCRE2_SPTR8 subject, PCRE2_SIZE length,
        PCRE2_SIZE startoffset, uint32_t options, pcre2_match_data* md);

//! Raises a Qore exception for PCRE2 match errors that do not simply mean "no match".
/** Must only be called with a return code < 1 from qore_pcre2_match().

    @param rc the pcre2_match() return code
    @param xsink if not nullptr, the exception sink for the @c REGEX-ERROR exception

    @return 0 if \a rc is an expected terminating condition (see qore_pcre2_expected_match_error()),
    -1 if it is an error, in which case an exception has been raised if \a xsink is not nullptr
 */
DLLLOCAL int qore_pcre2_check_match_error(int rc, ExceptionSink* xsink);

#define check_re_options(a) (a & ~(PCRE2_CASELESS|PCRE2_DOTALL|PCRE2_EXTENDED|PCRE2_MULTILINE|PCRE2_UTF|PCRE2_UCP|PCRE2_UNGREEDY|PCRE2_DOLLAR_ENDONLY|PCRE2_ANCHORED|PCRE2_DUPNAMES))

class QoreRegexBase {
public:
    DLLLOCAL QoreRegexBase() {
    }

    DLLLOCAL QoreRegexBase(int options) : options(options) {
    }

    DLLLOCAL QoreRegexBase(QoreString* str, int options = PCRE2_UTF) : str(str), options(options) {
    }

    DLLLOCAL ~QoreRegexBase() {
        if (p) {
            pcre2_code_free(p);
        }
        delete str;
    }

    DLLLOCAL void setCaseInsensitive();
    DLLLOCAL void setDotAll();
    DLLLOCAL void setExtended();
    DLLLOCAL void setMultiline();
    DLLLOCAL void setUnicode();
    DLLLOCAL void setUngreedy();

    //! Returns the regex options flags
    DLLLOCAL int getOptions() const { return options; }

    //! OR additional option bits into the options bitmask (for AOT deserialization)
    DLLLOCAL void addOptions(int opts) { options |= opts; }

    //! Returns the original pattern string (preserved for AOT serialization)
    DLLLOCAL const char* getPatternCStr() const {
        return pattern_cache.empty() ? nullptr : pattern_cache.c_str();
    }

    //! Saves the pattern string before it's deleted during compilation
    DLLLOCAL void savePattern() {
        if (str && pattern_cache.empty()) {
            pattern_cache = str->c_str();
        }
    }

    //! Attaches JIT-compiled code to the pattern, if the platform and the pattern support it.
    /** Must be called immediately after a successful pcre2_compile() and before the object is
        published to other threads; pcre2_jit_compile() modifies the pcre2_code block, so it must
        not race with pcre2_match().

        Failures are ignored on purpose: PCRE2 falls back to the interpreter when a pattern has no
        JIT code attached, so an unsupported platform or pattern construct only costs performance.

        The JIT is not merely an optimization here: PCRE2's interpreter switches off its
        required-code-unit start-of-match optimization once the remaining subject reaches
        REQ_CU_MAX * 1000 (5,000,000) code units for unanchored patterns (see the "HOWEVER" comment
        in pcre2_match.c). Past that threshold an unanchored non-matching pattern is retried at
        every offset, which is quadratic in the subject length and can wedge a thread for hours.
        The JIT has no such cutoff.
     */
    DLLLOCAL void jitCompile();

protected:
    pcre2_code* p = nullptr;
    QoreString* str = nullptr;
    int options = PCRE2_UTF;
    std::string pattern_cache;  //!< preserved for AOT serialization
};

#endif
