# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DataProviderSandboxCheck class definition

/*  DataProviderSandboxCheck.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! A decision of a DataProviderSandboxCheck object
hashdecl DataProviderSandboxDecision {
    #! The DataProviderSandboxState::generation that the decision was made for
    int generation = -1;

    #! The module of the hierarchy that registers an app with uncontrolled external I/O, if any
    *string refused;

    #! The modules of the hierarchy that have deferred apps whose declarations are not known yet, if no module refuses
    *list<string> pending;
}

#! Refuses sandboxed callers an object of a module that registers an app with uncontrolled external I/O
/** An object that does I/O for its callers, such as a data provider factory or a connection, keeps one of these and
    calls check() in every method that does I/O or creates an object that does, for a sandboxed caller; needsCheck()
    tells, without building the arguments of check(), if the call is needed.  A trusted caller is not checked, so the
    common path is the caller's Qore::SandboxManager::isSandboxed() call.  The object is refused to a sandboxed
    caller if a class of its class hierarchy belongs to a module that registers an app with
    @ref DataProvider::DataProviderAppInfo::external_io "external_io" and without
    @ref DataProvider::DataProviderAppInfo::sandbox_aware "sandbox_aware" (see @ref dataprovider_external_io), so a
    subclass defined elsewhere is refused like the class that it inherits, whatever name it is registered under, and
    whoever created the object: trusted code may create it, and a sandboxed caller is refused when it uses it.

    The object is created with the object that it checks, in a member initializer: its methods run with the
    provenance of the code that created it (see Qore::SandboxManager::callWithSystemPolicy()), so one created while
    serving a sandboxed caller would find every later caller sandboxed.

    The modules of the hierarchy are found once, and the decision is cached until the set of modules with
    uncontrolled I/O, or with deferred apps, changes; the common path compares a generation, with no lock.  The
    declarations of a deferred app (DataProviderActionCatalog::registerPendingApp()) are known only once it is
    initialized, so a sandboxed caller first initializes the deferred apps of the hierarchy's modules, and is refused
    if one of them still cannot be decided.

    @par Example:
    @code{.py}
class MyConnection inherits AbstractConnection {
    private:internal {
        DataProviderSandboxCheck sandbox_check();
    }

    private object getImpl(bool connect = True, *hash<auto> rtopts) {
        # the common path is one call; a sandboxed caller builds the arguments only if the decision requires them
        if (SandboxManager::isSandboxed() && sandbox_check.needsCheck(True)) {
            sandbox_check.check("connection", getName(), (Class::getClass(self),), True);
        }
        ...
    }
}
    @endcode

    @since %DataProvider 3.12
*/
public final class DataProviderSandboxCheck {
    private:internal {
        #! The modules of the class hierarchies, found on the first check
        *list<string> modules;

        #! The cached decision: \c generation, and the \c refused module or the \c pending modules, if any
        /** Replaced as a whole, so that a thread never reads the decision of one generation with the generation of
            another
        */
        hash<DataProviderSandboxDecision> decision();
    }

    #! Returns @ref True if check() must be called
    /** The cached decision is checked first, so that a caller builds the arguments of check() only when the decision
        is not cached for the current state, or when it refuses a sandboxed caller

        @param sandboxed whether the caller is sandboxed; if not given, Qore::SandboxManager::isSandboxed() decides

        @return @ref True if check() must be called, @ref False if it would not throw
    */
    bool needsCheck(*bool sandboxed) {
        hash<DataProviderSandboxDecision> d = decision;
        if (d.generation != DataProviderSandboxState::generation) {
            return True;
        }
        return (d.refused || d.pending) && (sandboxed ?? SandboxManager::isSandboxed());
    }

    #! Refuses a sandboxed caller the object, as described for this class
    /** @param what what the object is, for the error (ex: \c "connection")
        @param name the name of the object, for the error
        @param classes the classes whose hierarchies decide; only used on the first call
        @param sandboxed whether the caller is sandboxed; if not given, Qore::SandboxManager::isSandboxed() decides,
        as for DataProviderActionCatalog::checkSandboxAccess()

        @throw APP-SANDBOX-ACCESS-DENIED the caller is sandboxed, and a module of the hierarchy registers an app with
        external I/O that is not sandbox-aware, or has a deferred app whose declarations cannot be known
    */
    check(string what, string name, list<Class> classes, *bool sandboxed) {
        hash<DataProviderSandboxDecision> d = decision;
        if (d.generation != DataProviderSandboxState::generation) {
            if (!exists modules) {
                modules = DataProviderSandboxState::getHierarchyModules(classes);
            }
            d = decide();
        }
        if (!d.refused && !d.pending) {
            return;
        }
        if (!(sandboxed ?? SandboxManager::isSandboxed())) {
            return;
        }
        if (d.pending) {
            # the declarations of a deferred app are known once it is initialized; for a sandboxed reader, the
            # initialization that trusted code registered runs on the catalog worker
            DataProviderActionCatalog::initLoadedApps(keys DataProviderSandboxState::getPendingApps(d.pending));
            d = decide();
            if (d.pending && !d.refused) {
                throw "APP-SANDBOX-ACCESS-DENIED", sprintf("%s %y belongs to module(s) %y, whose deferred "
                    "application(s) %y could not be initialized, so whether sandboxed code may use it cannot be "
                    "decided", what, name, d.pending, sort(keys DataProviderSandboxState::getPendingApps(d.pending)));
            }
            if (!d.refused) {
                return;
            }
        }
        DataProviderActionCatalog::checkModuleSandboxAccess(what, name, d.refused, True);
    }

    #! Decides from the current state, caches the decision and returns it
    private:internal hash<DataProviderSandboxDecision> decide() {
        # the generation is read before the state: a change made meanwhile makes the next call decide again
        hash<DataProviderSandboxDecision> d({"generation": DataProviderSandboxState::generation});
        list<string> pending = ();
        foreach string module_name in (modules) {
            if (DataProviderSandboxState::uncontrolled_modules{module_name}) {
                d.refused = module_name;
                break;
            }
            if (DataProviderSandboxState::pending_modules{module_name}) {
                pending += module_name;
            }
        }
        if (!d.refused && pending) {
            d.pending = pending;
        }
        decision = d;
        return d;
    }
}
}
