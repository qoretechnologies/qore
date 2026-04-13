# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DataProviderImplicitModuleTransaction.qc provides the DataProvider catalog API

/** DataProviderImplicitModuleTransaction.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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
*/

#! Contains all public definitions in the DataProvider module
public namespace DataProvider {
#! Implements a mechanism to roll back changes made in external module initialization if an error occurs
/** Rollback closures accumulate across module loads in a thread_local. The savepoint mechanism
    ensures that only closures added during a specific module load are rolled back or committed,
    without affecting closures from previous successful loads.

    @par Usage in tryLoad():
    @code
    int sp = ImplicitModuleTransaction::savepoint();
    try {
        load_module(name);
        ImplicitModuleTransaction::commit(sp);
    } catch (...) {
        ImplicitModuleTransaction::rollback(sp);
    }
    @endcode
*/
public class ImplicitModuleTransaction {
    public {}

    private {
        list<code> rollback_list();
    }

    #! Executes all rollback code if there is an active exception in module initialization
    destructor() {
        if (active_exception()) {
            map $1(), rollback_list;
        }
    }

    #! Adds a rollback closure to be executed if there is an active exception in module initialization
    static add(code rb_code) {
        if (!rb) {
            rb = new ImplicitModuleTransaction();
        }
        rb.rollback_list += rb_code;
    }

    #! Returns the current savepoint (number of rollback closures registered so far)
    /** Call this before \c load_module() to mark the boundary between pre-existing
        rollback closures and new ones added during the module load.
    */
    static int savepoint() {
        if (!rb) {
            return 0;
        }
        return rb.rollback_list.size();
    }

    #! Executes rollback closures added since the given savepoint and removes them
    /** Call this when module loading fails to clean up partial registrations from the
        failed module without affecting registrations from previous successful loads.
    */
    static rollback(int sp) {
        if (!rb) {
            return;
        }
        # execute closures added after the savepoint in reverse order
        if (sp < rb.rollback_list.size()) {
            list<code> to_rollback = rb.rollback_list[sp..];
            rb.rollback_list = sp > 0 ? rb.rollback_list[0..sp - 1] : new list<code>();
            map $1(), to_rollback;
        }
    }

    #! Discards rollback closures added since the given savepoint (module loaded successfully)
    /** Call this after a successful module load to discard the rollback closures
        for that module, since its registrations should be kept.
    */
    static commit(int sp) {
        if (!rb) {
            return;
        }
        if (sp < rb.rollback_list.size()) {
            rb.rollback_list = sp > 0 ? rb.rollback_list[0..sp - 1] : new list<code>();
        }
    }
}
}

#! Private definitions
namespace Priv {
# Holds and optionally executes rollback code
thread_local ImplicitModuleTransaction rb;
}
