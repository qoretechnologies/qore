# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GroqDataProvider root class definition

/** GroqDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GroqDataProvider module
public namespace GroqDataProvider {
#! The Groq data provider root class
public class GroqDataProvider inherits GroqDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "groq",
            "type": "GroqDataProvider",
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
        #! Default child data providers
        const DefaultChildMap = {
            "chat-completion":
                Class::forName("GroqDataProvider::GroqChatCompletionDataProvider"),
            "list-models":
                Class::forName("GroqDataProvider::GroqListModelsDataProvider"),
        };
    }

    private:internal {
        static Mutex child_lock();
    }

    #! Creates the object from a RestClientIo client
    constructor(RestClientIo::RestClientIo rest) : GroqDataProviderCommon(rest) {
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : GroqDataProviderCommon(options) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Registers a new child data provider
    /** @param pathname unique child identifier
        @param cls provider class accepting a REST client
        @throw GROQ-CHILD-ERROR a built-in or additional child already uses the identifier
        @throw SANDBOX-REGISTRATION-ERROR code that is sandboxed or acts for sandboxed code cannot change the
            registry, which is process-wide (see @ref sandbox_process_wide_registries)
    */
    static registerChild(string pathname, Reflection::Class cls) {
        # the registry is process-wide: sandboxed code cannot change it (qore#6744)
        SandboxManager::checkProcessWideRegistration("register a child data provider");
        AutoLock al(child_lock);
        if (childMap{pathname} || GroqActions::Operations{pathname}) {
            throw "GROQ-CHILD-ERROR", sprintf("child %y already registered", pathname);
        }
        childMap{pathname} = cls;
    }

    #! Returns data provider summary info
    /** @return summaries for all built-in and additional children */
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        list<hash<DataProviderSummaryInfo>> rv = ();
        foreach string name in (getChildProviderNamesImpl()) {
            push rv, getChildProvider(name).getSummaryInfo();
        }
        return rv;
    }

    #! Returns a list of child data provider names, if any
    private *list<string> getChildProviderNamesImpl() {
        AutoLock al(child_lock);
        return (keys childMap) + (keys GroqActions::Operations);
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls;
        {
            AutoLock al(child_lock);
            cls = childMap{name};
        }
        if (!cls) {
            if (GroqActions::Operations{name}) {
                return new GroqApiDataProvider(rest, name);
            }
            return;
        }
        return cls.newObject(rest);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
