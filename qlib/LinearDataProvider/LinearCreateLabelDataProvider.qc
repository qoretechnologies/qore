# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearCreateLabelDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Data provider for creating Linear labels via request API
public class LinearCreateLabelDataProvider inherits LinearDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-label",
            "desc": "Create a new label in Linear",
            "type": "LinearCreateLabelDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type - the label data to create
        static LinearLabelCreateDataType RequestType();

        #! Response type - the created label
        static LinearLabelDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(LinearRestClient::LinearRestClient rest) : LinearDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(LinearRestClient::LinearRestConnection conn) : LinearDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "create-label";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new label in Linear";
    }

    #! Makes a request and returns the response
    /** @param req the request data (label fields to create)
        @param request_options the request options

        @return the created label data
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.name.val(), "name is required to create a label");
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: creating label name=%y", req.name));
        hash<auto> data = doGraphQL(CreateLabelMutation, {"input": req});
        @assert(data.issueLabelCreate, "issueLabelCreate response cannot be null");
        if (!data.issueLabelCreate.success) {
            throw "CREATE-ERROR", "Failed to create label";
        }
        @assert(data.issueLabelCreate.issueLabel, "created label cannot be null on success");
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: created label id=%y", data.issueLabelCreate.issueLabel.id));
        return flattenConnections(data.issueLabelCreate.issueLabel);
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
