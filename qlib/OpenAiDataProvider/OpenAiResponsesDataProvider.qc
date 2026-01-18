# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenAiDataProvider module definition

/** OpenAiDataResponsesProvider.qc Copyright 2024 - 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the OpenAiDataProvider module
public namespace OpenAiDataProvider {
#! The OpenAi data provider class
public class OpenAiResponsesDataProvider inherits OpenAiDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "responses",
            "type": "OpenAiResponsesDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        #! Child data providers
        const ChildMap = {
            "create": Class::forName("OpenAiDataProvider::OpenAiResponseCreateDataProvider"),
            "delete": Class::forName("OpenAiDataProvider::OpenAiResponseDeleteDataProvider"),
            "get": Class::forName("OpenAiDataProvider::OpenAiResponseGetDataProvider"),
            "stream": Class::forName("OpenAiDataProvider::OpenAiResponseStreamDataProvider"),
        };
    }

    #! Creates the object from the arguments
    constructor(*RestClient rest) : OpenAiDataProviderCommon(rest) {
    }

    #! Creates the object from constructor options
    constructor(hash<auto> options) : OpenAiDataProviderCommon(options) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Return data provider summary info
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
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
