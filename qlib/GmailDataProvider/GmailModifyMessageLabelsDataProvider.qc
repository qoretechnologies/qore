# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GmailDataProvider module definition

/** GmailModifyMessageLabelsDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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
#! Data provider for adding labels to and removing labels from a Gmail message
/** Wraps the Discovery API \c users/messages/modify method so that the labels can be chosen from reference
    data

    @since GmailDataProvider 1.2
*/
public class GmailModifyMessageLabelsDataProvider inherits GmailApiDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "modify-message-labels",
            "desc": "Gmail modify message labels data provider",
            "type": "GmailModifyMessageLabelsDataProvider",
            "supports_request": True,
            "mutates_state": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = GmailModifyMessageLabelsRequestDataType;

        #! Gmail message modify resource action
        const ResourceAction = "users/messages/modify";
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
        } + req;
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

#! Gmail modify message labels request type
/** @since GmailDataProvider 1.2
*/
public class GmailModifyMessageLabelsRequestDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "id": {
                "display_name": "Message ID",
                "type": StringType,
                "short_desc": "The ID of the message to relabel",
                "desc": "The ID of the message to relabel; watch events carry this value as `message_id`",
                "example_value": "18c6b13f059fc0bd",
                "required": True,
                "preselected": True,
            },
            "addLabelIds": {
                "display_name": "Labels to Add",
                "type": new SoftListDataType(StringType, True),
                "short_desc": "The IDs of the labels to add to the message",
                "desc": "The IDs of the labels to add to the message; a label is identified by its ID (ex: "
                    "`INBOX`, `STARRED`, `Label_12`), not its name",
                "example_value": ("STARRED",),
                "element_ref_data": GmailApiDataProviderBase::ReferenceDataLabels,
                "preselected": True,
            },
            "removeLabelIds": {
                "display_name": "Labels to Remove",
                "type": new SoftListDataType(StringType, True),
                "short_desc": "The IDs of the labels to remove from the message",
                "desc": "The IDs of the labels to remove from the message; a label is identified by its ID (ex: "
                    "`INBOX`, `UNREAD`, `Label_12`), not its name",
                "example_value": ("UNREAD",),
                "element_ref_data": GmailApiDataProviderBase::ReferenceDataLabels,
                "preselected": True,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Gmail modify message labels request type
/** @since GmailDataProvider 1.2
*/
public const GmailModifyMessageLabelsRequestDataType = new GmailModifyMessageLabelsRequestDataType();
}
