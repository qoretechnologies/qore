# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TogetherDataProvider root class definition

/** TogetherDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace TogetherDataProvider {
#! The Together AI data provider root class
public class TogetherDataProvider inherits TogetherDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "together",
            "type": "TogetherDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };
    }

    private {
        static hash<string, Reflection::Class> childMap = DefaultChildMap;

        const DefaultChildMap = {
            "chat-completion":
                Class::forName("TogetherDataProvider::TogetherChatCompletionDataProvider"),
            "create-embeddings":
                Class::forName("TogetherDataProvider::TogetherEmbeddingDataProvider"),
            "generate-image":
                Class::forName("TogetherDataProvider::TogetherImageGenerationDataProvider"),
        };
    }

    private:internal {
        static Mutex child_lock();
    }

    constructor(RestClientIo::RestClientIo rest) : TogetherDataProviderCommon(rest) {
    }

    constructor(*hash<auto> options) : TogetherDataProviderCommon(options) {
    }

    string getName() {
        return ProviderInfo.name;
    }

    #! Registers an additional child provider without replacing a built-in action
    /** @param pathname the new child identifier
        @param cls the provider class, accepting a REST client in its constructor
        @throw TOGETHER-CHILD-ERROR the identifier is already registered
    */
    static registerChild(string pathname, Reflection::Class cls) {
        AutoLock al(child_lock);
        if (childMap{pathname} || TogetherSchema::findActionSet(pathname)) {
            throw "TOGETHER-CHILD-ERROR", sprintf("child %y already registered", pathname);
        }
        childMap{pathname} = cls;
    }

    #! Returns the summaries of the registered child providers
    /** @return the built-in and additional child summaries */
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        list<hash<DataProviderSummaryInfo>> rv = ();
        foreach string name in (getChildProviderNamesImpl()) {
            push rv, getChildProvider(name).getSummaryInfo();
        }
        return rv;
    }

    private *list<string> getChildProviderNamesImpl() {
        AutoLock al(child_lock);
        hash<string, bool> names;
        foreach RestSchemaActionSet set in (TogetherSchema::getActionSets()) {
            map names{$1} = True, set.getActionNames();
        }
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
            if (TogetherSchema::findActionSet(name)) {
                return getSchemaProvider(name);
            }
            return;
        }
        return cls.newObject(rest);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
