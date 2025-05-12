# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FileDataProvider module definition

/** FileStatDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the FileDataProvider module
public namespace FileDataProvider {
#! The File stat data provider class
public class FileStatDataProvider inherits FileDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "stat",
            "desc": "File stat data provider; returns information about the path given as an argument",
            "type": "FileStatDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new FileStatRequestDataType();

        #! Response type
        const ResponseType = FileStatResponseDataType;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : FileDataProviderBase(options) {
    }

    #! Creates the object from the arguments
    constructor(*string root, *bool lock, *bool readonly) : FileDataProviderBase(root, lock, readonly) {
    }

    #! Returns the data provider name
    string getName() {
        return "stat";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "File stat data provider; returns information about the path given as an argument";
    }

    #! Makes a request and returns the response
    /** @param req the request info
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request

        @throw STAT-ERROR if the path cannot be read
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path = normalize_dir(req.path, root);
        if (root.val() && lock) {
            checkPath(\path);
        }
        # += to ensure that "rv" stays "hash<auto>"
        hash<auto> rv += (req.symlink ? hlstat(path) : hstat(path));

        if (!rv) {
            throw "STAT-ERROR", sprintf("%s: cannot stat(): %s", path, strerror()), errno();
        }
        rv += {
            "name": basename(path),
            "filepath": path,
        };
        return rv;
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
