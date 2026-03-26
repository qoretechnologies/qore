# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GeminiDataProvider module definition

/** GeminiCachedContentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GeminiDataProvider module
public namespace GeminiDataProvider {
#! The Gemini cached content data provider class (single cached content)
public class GeminiCachedContentDataProvider inherits GeminiDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "GeminiCachedContentDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
            "supports_children": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = AbstractDataProviderTypeMap."nothing";

        #! Response type
        const ResponseType = new GeminiCachedContentInfoDataType();
    }

    private {
        #! The cached content info
        hash<auto> content_info;

        #! Child data providers
        const ChildMap = {
            "update": Class::forName("GeminiDataProvider::GeminiCachedContentUpdateDataProvider"),
            "delete": Class::forName("GeminiDataProvider::GeminiCachedContentDeleteDataProvider"),
        };
    }

    #! Creates the object from the arguments
    constructor(RestClientIo::RestClientIo rest, hash<auto> content_info) : GeminiDataProviderBase(rest) {
        self.content_info = content_info;
    }

    #! Returns the data provider name
    string getName() {
        return content_info.name =~ /^cachedContents\// ? content_info.name.substr(15) : content_info.name;
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return content_info;
    }

    #! Returns the description of a successful request message, if any
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Return data provider summary info
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    #! Returns a list of child data provider names, if any
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(rest, content_info);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return cast<hash<DataProviderInfo>>({
            "name": getName(),
        }) + ProviderInfo;
    }
}
}
