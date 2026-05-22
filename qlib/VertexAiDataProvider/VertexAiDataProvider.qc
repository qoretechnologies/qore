# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore VertexAiDataProvider root class

/** VertexAiDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the VertexAiDataProvider module
public namespace VertexAiDataProvider {
#! Vertex AI root data provider
public class VertexAiDataProvider inherits VertexAiDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "vertex-ai",
            "type": "VertexAiDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };
    }

    private {
        const ChildMap = {
            "generate-content": Class::forName(
                "VertexAiDataProvider::VertexAiGenerateContentDataProvider"),
            "embed": Class::forName(
                "VertexAiDataProvider::VertexAiEmbedDataProvider"),
        };
    }

    constructor(*hash<auto> options) : VertexAiDataProviderCommon(options) {
    }

    string getName() {
        return ProviderInfo.name;
    }

    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        # forward state via a fresh constructor call
        return cls.newObject(rest, project_id, region, sa_json_path, scopes);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
