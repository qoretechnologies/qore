# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FireworksDataProvider root class definition

/** FireworksDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace FireworksDataProvider {
public class FireworksDataProvider inherits FireworksDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "fireworks",
            "type": "FireworksDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };
    }

    private:internal {
        #! the process-wide registry of child data providers; private:internal, so that only this class
        #! changes it (qore#6744)
        static hash<string, Reflection::Class> childMap = DefaultChildMap;
    }

    private {
        const DefaultChildMap = {
            "chat-completion":
                Class::forName("FireworksDataProvider::FireworksChatCompletionDataProvider"),
            "rerank": Class::forName("FireworksDataProvider::FireworksRerankDataProvider"),
            "create-embeddings":
                Class::forName("FireworksDataProvider::FireworksEmbeddingDataProvider"),
        };
    }

    private:internal {
        static Mutex child_lock();
    }

    constructor(RestClientIo::RestClientIo rest) : FireworksDataProviderCommon(rest) {
    }

    constructor(*hash<auto> options) : FireworksDataProviderCommon(options) {
    }

    string getName() {
        return ProviderInfo.name;
    }

    #! Registers an additional child without replacing a built-in action
    /** @param pathname unique child path
        @param cls provider class accepting a REST client
        @throw FIREWORKS-CHILD-ERROR the child name is already registered
        @throw SANDBOX-REGISTRATION-ERROR code that is sandboxed or acts for sandboxed code cannot change the
            registry, which is process-wide (see @ref sandbox_process_wide_registries)
    */
    static registerChild(string pathname, Reflection::Class cls) [dom=PROCESS_CONTROL] {
        # the registry is process-wide: sandboxed code cannot change it (qore#6744)
        SandboxManager::checkProcessWideRegistration("register a child data provider");
        AutoLock al(child_lock);
        if (childMap{pathname} || inlist(pathname, getSchemaActionNames())) {
            throw "FIREWORKS-CHILD-ERROR", sprintf("child %y already registered", pathname);
        }
        childMap{pathname} = cls;
    }

    private *list<string> getChildProviderNamesImpl() {
        AutoLock al(child_lock);
        hash<string, bool> names = map {$1: True}, getSchemaActionNames();
        map names{$1} = True, keys childMap;
        return keys names;
    }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls;
        {
            AutoLock al(child_lock);
            cls = childMap{name};
        }
        if (!cls) {
            if (inlist(name, getSchemaActionNames())) {
                return getSchemaProvider(name);
            }
            return;
        }
        return cls.newObject(rest);
    }

    private static list<string> getSchemaActionNames() {
        list<string> names;
        foreach RestSchemaActionSet aset in (FireworksSchema::getActionSets()) {
            names += aset.getActionNames();
        }
        return names;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
