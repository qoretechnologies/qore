# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DataProviderSandboxState class definition

/*  DataProviderSandboxState.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Sandbox state shared by the factories and the action catalog (see @ref dataprovider_external_io)
/** The catalog reports the modules that register an app with
    @ref DataProvider::DataProviderAppInfo::external_io "external_io" and without
    @ref DataProvider::DataProviderAppInfo::sandbox_aware "sandbox_aware" in @ref uncontrolled_modules; a factory
    refuses a sandboxed caller a provider if a class of its own class hierarchy, or of the hierarchy of the class of
    the providers it creates, belongs to one of them (AbstractDataProviderFactory::checkSandboxCreate()).  A subclass
    defined elsewhere, a factory created directly, and a factory registered under another name are refused alike.

    This class is not exported: code that imports the module cannot change what it refuses.
*/
class DataProviderSandboxState {
    public {
        #! The modules that register an app with uncontrolled external I/O: module name -> True
        /** Read without a lock by the factories; changed only by setModuleUncontrolled()
        */
        static hash<string, bool> uncontrolled_modules = {};

        #! Incremented after each change of @ref uncontrolled_modules, so that the factories can cache their decision
        static int generation = 0;
    }

    private:internal {
        #! Serializes the changes
        static Mutex lck();
    }

    #! Refuses a registration in the process-wide catalog to sandboxed code
    /** An app or factory name registered by sandboxed code would be taken from the module that owns it: that
        module's later registration would fail, denying its apps to everyone, or a sandboxed caller could decide the
        declarations of a name that it does not own.  Sandboxed code (Qore::SandboxManager::isSandboxed(), also
        through a policy barrier or a system-policy capability) therefore registers no app and no factory; modules
        register them in their initialization, which no sandbox governs.

        @param what the kind of registration, for the error: \c "application" or \c "data provider factory"
        @param name the name registered

        @throw APP-SANDBOX-ACCESS-DENIED sandboxed code registers an app or a factory
    */
    static checkRegistration(string what, string name) {
        if (SandboxManager::isSandboxed()) {
            throw "APP-SANDBOX-ACCESS-DENIED", sprintf("sandboxed code cannot register %s %y in the process-wide "
                "data provider catalog", what, name);
        }
    }

    #! The module now does, or no longer does, register an app with uncontrolled external I/O
    static setModuleUncontrolled(string module_name, bool uncontrolled) {
        AutoLock al(lck);
        if (uncontrolled) {
            uncontrolled_modules{module_name} = True;
        } else {
            remove uncontrolled_modules{module_name};
        }
        # after the change: a factory that read the old generation scans the modules again
        ++generation;
    }

    #! Returns the names of the modules of the given classes and of all of their parent classes, each once
    /** @param classes the classes whose hierarchies are walked

        @return the module names; a class that belongs to no module contributes none
    */
    static list<string> getHierarchyModules(list<Class> classes) {
        hash<string, bool> modules = {};
        hash<string, bool> seen = {};
        while (classes) {
            Class cls = shift classes;
            string name = cls.getPathName();
            if (seen{name}) {
                continue;
            }
            seen{name} = True;
            if (*string module_name = cls.getModuleName()) {
                modules{module_name} = True;
            }
            map classes += cast<Class>($1.cls), cls.getParentClasses();
        }
        return keys modules;
    }
}
}
