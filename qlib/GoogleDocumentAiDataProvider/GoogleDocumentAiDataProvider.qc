# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GoogleDocumentAiDataProvider root class definition

/** GoogleDocumentAiDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace GoogleDocumentAiDataProvider {
public class GoogleDocumentAiDataProvider inherits GoogleDocumentAiDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "google-documentai",
            "type": "GoogleDocumentAiDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        static hash<string, Reflection::Class> childMap = {
            "process-document":
                Class::forName("GoogleDocumentAiDataProvider::GoogleDocumentAiProcessDocumentDataProvider"),
        };
    }

    constructor(GoogleDocumentAiRestClient::GoogleDocumentAiRestClientIo rest,
            string project_id, *string region, *string sa_json_path)
            : GoogleDocumentAiDataProviderCommon(rest, project_id, region, sa_json_path) {}
    constructor(*hash<auto> options) : GoogleDocumentAiDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }

    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), childMap.iterator();
    }
    private *list<string> getChildProviderNamesImpl() { return keys childMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = childMap{name};
        return cls ? cls.newObject(rest, project_id, region, sa_json_path) : NOTHING;
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class GoogleDocumentAiDataProviderFactory inherits AbstractDataProviderFactory {
    private {
        static Class cls = new Class("GoogleDocumentAiDataProvider");
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "google-documentai",
            "desc": "Google Document AI data provider factory",
            "children_can_support_apis": True,
        };
    }
    private hash<DataProviderFactoryInfo> getInfoImpl() { return FactoryInfo; }
    private hash<DataProviderInfo> getProviderInfoImpl() {
        return GoogleDocumentAiDataProvider::ProviderInfo;
    }
    private Class getClassImpl() { return cls; }
}
}
