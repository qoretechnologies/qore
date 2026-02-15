# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearUpdateIssueDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Data provider for updating Linear issues via request API
public class LinearUpdateIssueDataProvider inherits LinearDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-issue",
            "desc": "Update an existing issue in Linear",
            "type": "LinearUpdateIssueDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type - the issue ID and update data
        static LinearIssueUpdateRequestDataType RequestType();

        #! Response type - the updated issue
        static LinearIssueDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(LinearRestClient::LinearRestClient rest) : LinearDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(LinearRestClient::LinearRestConnection conn) : LinearDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "update-issue";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update an existing issue in Linear";
    }

    #! Makes a request and returns the response
    /** @param req the request data (issue ID and fields to update)
        @param request_options the request options

        @return the updated issue data
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.id.val(), "id is required to update an issue");
        string issueId = req.id;
        # Remove id from the input as it's passed separately
        hash<auto> input = req - "id";
        # Process labelIds to extract plain values if they are objects (from element_ref_data)
        if (input.labelIds) {
            input.labelIds = map $1.value ?? $1, input.labelIds;
        }
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: updating issue id=%y fields=%y", issueId, keys input));
        hash<auto> data = doGraphQL(UpdateIssueMutation, {"id": issueId, "input": input});
        @assert(data.issueUpdate, "issueUpdate response cannot be null");
        if (!data.issueUpdate.success) {
            throw "UPDATE-ERROR", "Failed to update issue";
        }
        @assert(data.issueUpdate.issue, "updated issue cannot be null on success");
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: updated issue id=%y", data.issueUpdate.issue.id));
        return flattenConnections(data.issueUpdate.issue);
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

} # namespace LinearDataProvider
