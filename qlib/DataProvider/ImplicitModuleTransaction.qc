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

    #! Internal helper used by static transaction methods
    addImpl(code rb_code) {
        rollback_list += rb_code;
    }

    #! Internal helper used by static transaction methods
    int savepointImpl() {
        return rollback_list.size();
    }

    #! Internal helper used by static transaction methods
    rollbackImpl(int sp) {
        if (sp < rollback_list.size()) {
            list<code> to_rollback = rollback_list[sp..];
            rollback_list = sp > 0 ? rollback_list[0..sp - 1] : new list<code>();
            # every closure runs, even if one raises an exception; the first one is raised afterwards (qore#6337)
            *hash<ExceptionInfo> first;
            foreach code rb_code in (to_rollback) {
                try {
                    rb_code();
                } catch (hash<ExceptionInfo> ex) {
                    if (!first) {
                        first = ex;
                    }
                }
            }
            if (first) {
                throw first.err, first.desc, first.arg;
            }
        }
    }

    #! Internal helper used by static transaction methods
    commitImpl(int sp) {
        if (sp < rollback_list.size()) {
            rollback_list = sp > 0 ? rollback_list[0..sp - 1] : new list<code>();
        }
    }

    #! Adds a rollback closure to be executed if there is an active exception in module initialization
    static add(code rb_code) {
        if (!rb) {
            rb = new ImplicitModuleTransaction();
        }
        rb.addImpl(rb_code);
    }

    #! Loads a module and rolls back the registrations that its initialization made if loading it fails
    /** The registrations of the module are kept if it is loaded; if loading it raises an exception, the rollback
        closures added since the load started are run (all of them, even if one raises an exception) and the
        exception of the load is raised again.  This is the only part of the API that code restricted from the
        \c PROCESS_CONTROL domain can use: savepoint(), rollback() and commit() act on the rollback closures of other
        loads too (qore#6337)

        @param module_str the module to load, as given to @ref Qore::load_module() "load_module()"

        @throw any exception raised while loading the module

        @since %DataProvider 3.6
    */
    static loadModule(string module_str) {
        if (!rb) {
            rb = new ImplicitModuleTransaction();
        }
        int sp = rb.savepointImpl();
        try {
            load_module(module_str);
            rb.commitImpl(sp);
        } catch (hash<ExceptionInfo> ex) {
            # the error of the load is the one raised
            try {
                rb.rollbackImpl(sp);
            } catch (hash<ExceptionInfo> rex) {
            }
            rethrow;
        }
    }

    #! Returns the current savepoint (number of rollback closures registered so far)
    /** Call this before \c load_module() to mark the boundary between pre-existing
        rollback closures and new ones added during the module load.
    */
    static int savepoint() [dom=PROCESS_CONTROL] {
        if (!rb) {
            return 0;
        }
        return rb.savepointImpl();
    }

    #! Executes rollback closures added since the given savepoint and removes them
    /** Call this when module loading fails to clean up partial registrations from the
        failed module without affecting registrations from previous successful loads.
    */
    static rollback(int sp) [dom=PROCESS_CONTROL] {
        if (!rb) {
            return;
        }
        # execute closures added after the savepoint in reverse order
        rb.rollbackImpl(sp);
    }

    #! Discards rollback closures added since the given savepoint (module loaded successfully)
    /** Call this after a successful module load to discard the rollback closures
        for that module, since its registrations should be kept.
    */
    static commit(int sp) [dom=PROCESS_CONTROL] {
        if (!rb) {
            return;
        }
        rb.commitImpl(sp);
    }
}
}

#! Private definitions
namespace Priv {
# Holds and optionally executes rollback code
thread_local ImplicitModuleTransaction rb;
}
