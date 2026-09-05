# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GmailDataProvider module definition

/** GmailCreateDraftDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GmailDataProvider module
public namespace GmailDataProvider {
#! Data provider for creating Gmail drafts
/** Takes the same request as @ref GmailMessageSendDataProvider "send-message" and stores the message as a
    draft instead of transmitting it

    @since GmailDataProvider 1.2
*/
public class GmailCreateDraftDataProvider inherits GmailApiDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-draft",
            "desc": "Gmail create draft data provider",
            "type": "GmailCreateDraftDataProvider",
            "supports_request": True,
            "mutates_state": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = GmailSendMessageRequestDataType;

        #! Gmail draft create resource action
        const ResourceAction = "users/drafts/create";
    }

    #! Creates the object from a REST connection
    constructor(GoogleRestClient::GoogleRestClientIo rest) : GmailApiDataProviderBase(rest, ResourceAction) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Google data %s provider for `%s`", ProviderInfo.name, rest.getSafeUrl());
    }

    #! Makes a request and returns the response
    /** @param req the request info
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> greq = {
            "userId": "me",
            "message": GmailMessageSendDataProvider::makeMessageBody(req),
        };
        return GoogleApiDataProvider::doRequestImpl(greq, request_options);
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the Google request type for the operation
    private AbstractDataProviderType getGoogleRequestType() {
        return GoogleDataProviderBase::getRequestTypeForSchema(GoogleDiscoveryGmailApiName, ResourceAction);
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *AbstractDataProviderType getResponseTypeImpl() {
        return GoogleDataProviderBase::getResponseTypeForSchema(GoogleDiscoveryGmailApiName, ResourceAction);
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
