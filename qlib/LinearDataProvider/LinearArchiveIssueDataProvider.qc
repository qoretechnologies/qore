# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearArchiveIssueDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LinearDataProvider module
public namespace LinearDataProvider {

#! Data provider for archiving Linear issues via request API
public class LinearArchiveIssueDataProvider inherits LinearDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "archive-issue",
            "desc": "Archive (soft delete) a Linear issue",
            "type": "LinearArchiveIssueDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type - the issue ID to archive
        static LinearArchiveIssueRequestDataType RequestType();

        #! Response type - success status
        static LinearArchiveIssueResponseDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(LinearRestClient::LinearRestClient rest) : LinearDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(LinearRestClient::LinearRestConnection conn) : LinearDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "archive-issue";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Archive (soft delete) a Linear issue";
    }

    #! Makes a request and returns the response
    /** @param req the request data (issue ID to archive)
        @param request_options the request options

        @return hash with success status
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.id.val(), "id is required to archive an issue");
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: archiving issue id=%y", req.id));
        hash<auto> data = doGraphQL(ArchiveIssueMutation, {"id": req.id});
        @assert(data.issueArchive, "issueArchive response cannot be null");
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: archive success=%y", data.issueArchive.success));
        return {
            "success": data.issueArchive.success,
        };
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Request type for archiving an issue
public class LinearArchiveIssueRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Issue",
                "short_desc": "The issue to archive",
                "desc": "The ID of the issue to archive",
                "example_value": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for archiving an issue
public class LinearArchiveIssueResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "success": {
                "type": BoolType,
                "display_name": "Success",
                "short_desc": "Whether the archive operation succeeded",
                "desc": "True if the issue was successfully archived",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace LinearDataProvider
