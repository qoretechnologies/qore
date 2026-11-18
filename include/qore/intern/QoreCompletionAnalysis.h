/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreCompletionAnalysis.h

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

#ifndef _QORE_INTERN_QORECOMPLETIONANALYSIS_H
#define _QORE_INTERN_QORECOMPLETIONANALYSIS_H

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

class QoreProgram;
class UserVariantBase;
class AbstractQoreFunctionVariant;

//! The missing return checks of the code of a parse, which wait until the parse has initialized all of its code
/** The MISSING-RETURN check of a function, method or closure with a declared return type that does not accept
    NOTHING raises the error when the end of its code is certainly reachable without a return statement (issue
    #5940).  A call reaches the end of the code only if its target can return, which the check finds from the code
    of the target, so it waits until the parse has initialized all of the code that it parses: the checks are
    collected while the parse initializes its code (from start() until run() or clear()) and made by run(); a check
    outside of that phase (of code initialized at run time, such as an expression of
    Program::getExpression()) is made at once, with the code that is initialized then.

    The variants are held with a reference while they wait, as code that the parse initialized can be discarded
    before the checks are made.

    It also keeps what the calls of the code of the Program analyzed can do (whether they return and raise
    exceptions), by the serial of the variant (which is never reused), so that the code that the checks call is
    analyzed once; it is cleared when a parse starts to initialize code and when its checks have been made, as the
    analysis of a call depends on the variants of the functions that the code calls.
*/
class QoreMissingReturnChecks {
public:
    DLLLOCAL QoreMissingReturnChecks() = default;

    DLLLOCAL ~QoreMissingReturnChecks() {
        clear();
    }

    QoreMissingReturnChecks(const QoreMissingReturnChecks&) = delete;
    QoreMissingReturnChecks& operator=(const QoreMissingReturnChecks&) = delete;

    //! Starts collecting the checks of the code that the parse initializes
    DLLLOCAL void start() {
        active = true;
        clearCache();
    }

    //! Collects the check of the variant if the parse is initializing its code; returns true if the check waits
    DLLLOCAL bool defer(const UserVariantBase* uvb);

    //! Makes the collected checks, raising a MISSING-RETURN parse exception in @a pgm for each code that misses a
    //! return, and releases the variants
    DLLLOCAL void run(QoreProgram* pgm);

    //! Stops collecting and releases the variants without making their checks
    DLLLOCAL void clear() noexcept;

    //! Returns true and what a call of the variant with the given serial can do if it has been analyzed
    DLLLOCAL bool get(uint64_t serial, unsigned char& returns, unsigned char& thr) const {
        std::lock_guard<std::mutex> lock(cache_lock);
        auto i = cache.find(serial);
        if (i == cache.end()) {
            return false;
        }
        returns = i->second.first;
        thr = i->second.second;
        return true;
    }

    //! Keeps what a call of the variant with the given serial can do
    DLLLOCAL void set(uint64_t serial, unsigned char returns, unsigned char thr) {
        std::lock_guard<std::mutex> lock(cache_lock);
        cache[serial] = std::make_pair(returns, thr);
    }

    //! Forgets what calls of the code of the Program can do
    DLLLOCAL void clearCache() noexcept {
        std::lock_guard<std::mutex> lock(cache_lock);
        cache.clear();
    }

private:
    std::vector<AbstractQoreFunctionVariant*> pending;
    //! what the calls of the variants can do, by the serial of the variant
    std::unordered_map<uint64_t, std::pair<unsigned char, unsigned char>> cache;
    //! protects the cache: code can be initialized at run time in several threads (ex: Expression objects)
    mutable std::mutex cache_lock;
    bool active = false;
};

//! Collects the missing return checks of the code that a parse initializes while it lives (issue #5940)
/** The checks that have not been made when it is destroyed are released without being made
*/
class QoreMissingReturnCheckHelper {
public:
    DLLLOCAL QoreMissingReturnCheckHelper(QoreMissingReturnChecks& checks) : checks(checks) {
        checks.start();
    }

    DLLLOCAL ~QoreMissingReturnCheckHelper() {
        checks.clear();
    }

    QoreMissingReturnCheckHelper(const QoreMissingReturnCheckHelper&) = delete;
    QoreMissingReturnCheckHelper& operator=(const QoreMissingReturnCheckHelper&) = delete;

private:
    QoreMissingReturnChecks& checks;
};

//! Raises a MISSING-RETURN parse exception in @a pgm if the end of the code of the variant is certainly reachable
//! and its declared return type does not accept NOTHING (issue #5940); returns -1 if the exception was raised
DLLLOCAL int qore_parse_check_missing_return(QoreProgram* pgm, const UserVariantBase* uvb);

#endif
