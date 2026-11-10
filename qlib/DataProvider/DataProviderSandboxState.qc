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
#! The factories that sandboxed callers are refused, shared by the factory registry and the action catalog
/** A factory is refused if the module that defines its class registers an app with
    @ref DataProvider::DataProviderAppInfo::external_io "external_io" and without
    @ref DataProvider::DataProviderAppInfo::sandbox_aware "sandbox_aware" (see
    @ref dataprovider_external_io).  The factory registry reports the factories and their modules, and the catalog
    reports the modules that register such an app; the factory getters read @ref refused_factories, one lookup with
    no lock.

    This class is not exported: code that imports the module cannot change what it refuses.
*/
class DataProviderSandboxState {
    public {
        #! Factories that sandboxed callers are refused: factory name -> module name
        static hash<string, string> refused_factories = {};
    }

    private:internal {
        #! The factories of each module: module name -> factory name -> True
        static hash<string, hash<string, bool>> module_factories = {};

        #! The module of each factory: factory name -> module name
        static hash<string, string> factory_modules = {};

        #! The modules that register an app with uncontrolled external I/O: module name -> True
        static hash<string, bool> uncontrolled_modules = {};

        #! Serializes the changes, which the factory registry and the catalog make under different locks
        static Mutex lck();
    }

    #! A factory was registered; \a module_name is the module that defines its class, if any
    static factoryRegistered(string name, *string module_name) {
        AutoLock al(lck);
        DataProviderSandboxState::removeFactoryIntern(name);
        if (!module_name) {
            return;
        }
        factory_modules{name} = module_name;
        module_factories{module_name}{name} = True;
        if (uncontrolled_modules{module_name}) {
            refused_factories{name} = module_name;
        }
    }

    #! A factory was removed
    static factoryRemoved(string name) {
        AutoLock al(lck);
        DataProviderSandboxState::removeFactoryIntern(name);
    }

    #! The module now does, or no longer does, register an app with uncontrolled external I/O
    static setModuleUncontrolled(string module_name, bool uncontrolled) {
        AutoLock al(lck);
        if (uncontrolled) {
            uncontrolled_modules{module_name} = True;
            map refused_factories{$1} = module_name, keys module_factories{module_name};
        } else {
            remove uncontrolled_modules{module_name};
            map remove refused_factories{$1}, keys module_factories{module_name};
        }
    }

    #! Removes a factory; the lock is held
    private:internal static removeFactoryIntern(string name) {
        if (*string module_name = remove factory_modules{name}) {
            remove module_factories{module_name}{name};
            if (!module_factories{module_name}) {
                remove module_factories{module_name};
            }
        }
        remove refused_factories{name};
    }
}
}
