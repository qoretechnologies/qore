# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OllamaDataProvider root class definition

/** OllamaDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace OllamaDataProvider {
public class OllamaDataProvider inherits OllamaDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "ollama",
            "type": "OllamaDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };
    }

    private {
        static hash<string, Reflection::Class> childMap = DefaultChildMap;

        const DefaultChildMap = {
            "chat-completion":
                Class::forName("OllamaDataProvider::OllamaChatCompletionDataProvider"),
            "create-embeddings":
                Class::forName("OllamaDataProvider::OllamaEmbedDataProvider"),
            "list-models":
                Class::forName("OllamaDataProvider::OllamaListModelsDataProvider"),
        };
    }

    constructor(RestClientIo::RestClientIo rest) : OllamaDataProviderCommon(rest) {
    }

    constructor(*hash<auto> options) : OllamaDataProviderCommon(options) {
    }

    string getName() {
        return ProviderInfo.name;
    }

    static registerChild(string pathname, Reflection::Class cls) {
        if (childMap{pathname}) {
            throw "OLLAMA-CHILD-ERROR", sprintf("child %y already registered", pathname);
        }
        childMap{pathname} = cls;
    }

    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), childMap.iterator();
    }

    private *list<string> getChildProviderNamesImpl() {
        return keys childMap;
    }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = childMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(rest);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
