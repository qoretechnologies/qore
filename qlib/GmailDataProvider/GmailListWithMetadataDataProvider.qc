# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GmailDataProvider module definition

/** GmailListWithMetadataDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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
#! Data provider for searching for Gmail messages and returning each match with its metadata
/** Gmail's \c users/messages/list method returns only message and thread IDs, so a consumer that needs the
    subject or sender of each match has to retrieve every message separately.  This provider does that in one
    action: it lists the matching messages and then retrieves each one with \c users/messages/get in the
    requested \c format, returning the retrieved messages in place of the bare IDs

    @since GmailDataProvider 1.2
*/
public class GmailListWithMetadataDataProvider inherits GmailApiDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-with-metadata",
            "desc": "Gmail search with metadata data provider",
            "type": "GmailListWithMetadataDataProvider",
            "supports_request": True,
            "mutates_state": False,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = GmailListWithMetadataRequestDataType;

        #! Gmail message list resource action
        const ResourceAction = "users/messages/list";

        #! Gmail message get resource action used to retrieve each listed message
        const GetResourceAction = "users/messages/get";

        #! The message format retrieved when the request does not name one
        const DefaultFormat = "metadata";

        #! The headers retrieved when the request does not name any
        const DefaultMetadataHeaders = ("Subject", "From", "To", "Date");

        #! The request keys that are not part of the Gmail list request
        const RetrievalKeys = ("format", "metadata_headers");

        #! The URI of the account's messages resource
        const MessagesUri = "gmail/v1/users/me/messages";
    }

    private {
        #! The response type; created on demand because it is built from the Discovery API document
        static GmailListWithMetadataResponseDataType response_type;

        #! The type of each retrieved message
        AbstractDataProviderType message_type;
    }

    #! Creates the object from a REST connection
    constructor(GoogleRestClient::GoogleRestClientIo rest) : GmailApiDataProviderBase(rest, ResourceAction) {
        message_type = GoogleDataProviderBase::getResponseTypeForSchema(GoogleDiscoveryGmailApiName,
            GetResourceAction);
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Google data %s provider for `%s`", ProviderInfo.name, rest.getSafeUrl());
    }

    #! Returns the response type
    /** @return the response type: the Gmail list response with each listed message replaced by the retrieved
        message
    */
    static AbstractDataProviderType getResponseType() {
        return response_type ?? (response_type = new GmailListWithMetadataResponseDataType());
    }

    #! Makes a request and returns the response
    /** @param req the request info
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request

        @note one \c users/messages/list request is made, and then one \c users/messages/get request per
        listed message; a page of \c maxResults messages therefore costs \c maxResults + 1 requests.  The
        request URIs are built here rather than from the Discovery API request types, because those types
        describe Gmail's repeated query parameters (\c labelIds, \c metadataHeaders) as single strings
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string format = req.format ?? DefaultFormat;
        softlist<string> metadata_headers = req.metadata_headers ?? DefaultMetadataHeaders;

        hash<auto> rv = restGet(MessagesUri + GmailListWithMetadataDataProvider::getQuery(req - RetrievalKeys))
            ?? {};
        list<hash<auto>> messages = ();
        foreach hash<auto> minfo in (rv.messages) {
            # "auto!" so that the string-only literal is not narrowed to hash<string, string>, which would
            # reject the header list
            hash<auto!> get_args = {
                "format": format,
            };
            # Gmail ignores the header list unless the metadata format is requested
            if (format == DefaultFormat) {
                get_args.metadataHeaders = metadata_headers;
            }
            string uri = sprintf("%s/%s%s", MessagesUri, encode_url(minfo.id, True),
                GmailListWithMetadataDataProvider::getQuery(get_args));
            messages += message_type.acceptsValue(restGet(uri));
        }
        # "+" rather than a key assignment, so a response hash narrowed to another value type is not an error
        return getResponseTypeImpl().acceptsValue(rv + {"messages": messages});
    }

    #! Makes a GET request and returns the response body
    /** @param uri the request URI

        @return the response body

        @note any error response is included in the exception as with @ref GoogleApiDataProvider requests
    */
    private auto restGet(string uri) {
        try {
            return rest.restGet(uri).body;
        } catch (hash<ExceptionInfo> ex) {
            rethrow ex.err, ex.desc, ex.arg + {
                "request-uri": ex.arg.request_path,
                "response-code": ex.arg.status_code,
                "response-body": ex.arg.response_body_raw,
            };
        }
    }

    #! Returns the query string for the given arguments
    /** @param args the query arguments; a list value is repeated once per element, as Gmail expects for
        \c labelIds and \c metadataHeaders, and a boolean is sent as \c true or \c false

        @return the query string with a leading \c "?", or an empty string if there are no arguments
    */
    private static string getQuery(hash<auto> args) {
        list<string> query = ();
        foreach hash<auto> i in (args.pairIterator()) {
            foreach auto val in (i.value) {
                string str = val.typeCode() == NT_BOOLEAN ? (val ? "true" : "false") : val.toString();
                query += sprintf("%s=%s", i.key, encode_url(str, True));
            }
        }
        return query ? "?" + query.join("&") : "";
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
        return GmailListWithMetadataDataProvider::getResponseType();
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Message formats supported when retrieving listed messages
/** @since GmailDataProvider 1.2
*/
public const GmailListWithMetadataFormatList = (
    <AllowedValueInfo>{
        "display_name": "Metadata",
        "short_desc": "Message IDs, labels, snippet, and the requested headers only",
        "desc": "Message IDs, labels, snippet, and the headers named in `metadata_headers` only; no body",
        "value": "metadata",
    },
    <AllowedValueInfo>{
        "display_name": "Full",
        "short_desc": "The complete message including all headers and the body",
        "desc": "The complete message including all headers and the MIME body parts; attachment data is not "
            "included and must be retrieved with `get-attachment`",
        "value": "full",
    },
);

#! Gmail search with metadata request type
/** @since GmailDataProvider 1.2
*/
public class GmailListWithMetadataRequestDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "q": {
                "display_name": "Search Criteria",
                "type": StringType,
                "short_desc": "The gmail search criteria for retrieving the messages",
                "desc": "The gmail search criteria for retrieving the messages; ex: `in:inbox to:me@gmail.com "
                    "subject:hello is:unread`",
                "example_value": "in:inbox to:me@gmail.com subject:hello is:unread",
                "required": True,
                "preselected": True,
            },
            "maxResults": {
                "display_name": "Max Results",
                "type": IntOrNothingType,
                "short_desc": "The maximum number of results to return",
                "desc": "The maximum number of results to return; each message returned costs one additional "
                    "API request",
                "example_value": 20,
                "preselected": True,
            },
            "includeSpamTrash": {
                "display_name": "Include Spam and Trash?",
                "type": BoolOrNothingType,
                "short_desc": "Include the spam and trash folders in the search?",
                "desc": "Include the spam and trash folders in the search?",
                "example_value": False,
            },
            "labelIds": {
                "display_name": "Labels",
                "type": new SoftListDataType(StringType, True),
                "short_desc": "Only return messages with labels that match all of the specified label IDs",
                "desc": "Only return messages with labels that match all of the specified label IDs. Messages in "
                    "a thread might have labels that other messages in the same thread don't have",
                "example_value": ("INBOX", "UNREAD"),
                "element_ref_data": GmailApiDataProviderBase::ReferenceDataLabels,
            },
            "pageToken": {
                "display_name": "Page Token",
                "type": StringOrNothingType,
                "short_desc": "Page token to retrieve a specific page of results in the list",
                "desc": "Page token to retrieve a specific page of results in the list",
                "example_value": "07142542234962330268",
            },
            "format": {
                "display_name": "Message Format",
                "type": StringType,
                "short_desc": "How much of each matching message to retrieve",
                "desc": "How much of each matching message to retrieve",
                "default_value": GmailListWithMetadataDataProvider::DefaultFormat,
                "allowed_values": GmailListWithMetadataFormatList,
            },
            "metadata_headers": {
                "display_name": "Metadata Headers",
                "type": new SoftListDataType(StringType, True),
                "short_desc": "The message headers to retrieve for each matching message",
                "desc": "The message headers to retrieve for each matching message; ignored unless `format` is "
                    "`metadata`",
                "default_value": GmailListWithMetadataDataProvider::DefaultMetadataHeaders,
                "example_value": ("Subject", "From", "To", "Date", "Message-ID"),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Gmail search with metadata request type
/** @since GmailDataProvider 1.2
*/
public const GmailListWithMetadataRequestDataType = new GmailListWithMetadataRequestDataType();

#! Gmail search with metadata response type
/** The Gmail \c users/messages/list response with each listed message replaced by the message retrieved with
    \c users/messages/get

    @note Built from the Discovery API document, so it cannot be created before the document is available

    @since GmailDataProvider 1.2
*/
public class GmailListWithMetadataResponseDataType inherits HashDataType {
    #! Creates the object
    constructor() : HashDataType("GmailListWithMetadataResponse") {
        addField(new QoreDataField({
            "name": "messages",
            "display_name": "Messages",
            "short_desc": "The matching messages, each retrieved in the requested format",
            "desc": "The matching messages, each retrieved in the requested format; with the `metadata` format, "
                "the requested headers are in `payload.headers`",
            "type": new ListDataType("GmailMessageList", GoogleDataProviderBase::getResponseTypeForSchema(
                GoogleDiscoveryGmailApiName, GmailListWithMetadataDataProvider::GetResourceAction), True),
        }));
        addField(new QoreDataField({
            "name": "nextPageToken",
            "display_name": "Next Page Token",
            "short_desc": "Token to retrieve the next page of results in the list",
            "desc": "Token to retrieve the next page of results in the list; pass it as `pageToken` to retrieve "
                "the next page",
            "type": AbstractDataProviderTypeMap."*string",
        }));
        addField(new QoreDataField({
            "name": "resultSizeEstimate",
            "display_name": "Result Size Estimate",
            "short_desc": "Estimated total number of results",
            "desc": "Estimated total number of results",
            "type": AbstractDataProviderTypeMap."*int",
        }));
    }
}
}
