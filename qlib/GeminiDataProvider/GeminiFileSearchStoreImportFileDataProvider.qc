# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GeminiDataProvider module definition

/** GeminiFileSearchStoreImportFileDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The Gemini file search store import file data provider
public class GeminiFileSearchStoreImportFileDataProvider inherits GeminiDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "import-file",
            "type": "GeminiFileSearchStoreImportFileDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new GeminiFileSearchStoreImportFileRequestDataType();

        #! Response type
        const ResponseType = new GeminiImportFileResponseDataType();
    }

    private {
        #! The store info
        hash<auto> store_info;
    }

    #! Creates the object from a REST client (standalone)
    constructor(RestClientIo::RestClientIo rest) : GeminiDataProviderBase(rest) {
    }

    #! Creates the object from the arguments
    constructor(RestClientIo::RestClientIo rest, hash<auto> store_info) : GeminiDataProviderBase(rest) {
        self.store_info = store_info;
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # Get store name from request or from parent store context
        string name;
        if (req.fileSearchStore) {
            name = remove req.fileSearchStore;
        } else if (store_info.name) {
            name = store_info.name;
        } else {
            throw "IMPORT-FILE-ERROR", "missing 'fileSearchStore' option";
        }
        if (name !~ /^fileSearchStores\//) {
            name = "fileSearchStores/" + name;
        }
        # Handle file upload: upload first, then import
        if (req."file") {
            hash<auto> file = remove req."file";
            hash<auto> file_info = uploadFile(file.name, file.mime_type, file.content);
            req.fileName = file_info.name;
        } else if (!req.fileName) {
            throw "IMPORT-FILE-ERROR", "either 'file' or 'fileName' must be provided";
        }
        return doRestCommand("POST", name + ":importFile", req).body;
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
