# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GeminiDataProvider module definition

/** GeminiFileDeleteDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The Gemini file delete data provider
public class GeminiFileDeleteDataProvider inherits GeminiDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "type": "GeminiFileDeleteDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new GeminiFileDeleteRequestDataType();

        #! Response type
        const ResponseType = new GeminiDeleteResponseDataType();
    }

    private {
        #! The file info
        hash<auto> file_info;
    }

    #! Creates the object from a REST client (standalone)
    constructor(RestClientIo::RestClientIo rest) : GeminiDataProviderBase(rest) {
    }

    #! Creates the object from the arguments
    constructor(RestClientIo::RestClientIo rest, hash<auto> file_info) : GeminiDataProviderBase(rest) {
        self.file_info = file_info;
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string name;
        if (req.fileName) {
            name = remove req.fileName;
        } else if (file_info.name) {
            name = file_info.name;
        } else {
            throw "DELETE-FILE-ERROR", "missing 'fileName' option";
        }
        if (name !~ /^files\//) {
            name = "files/" + name;
        }
        doRestCommand("DELETE", name);
        return {"deleted": True, "name": name};
    }

    #! Returns the description of a successful request message, if any
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
