# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GmailDataProvider module definition

/** GmailMessageWatchDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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
#! Gmail email matched event
public const EVENT_EMAIL_MATCHED = "EMAIL-MATCHED";

#! The parent class for Gmail REST APIs
public class GmailMessageWatchDataProvider inherits GmailMessageWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "watch",
            "desc": "Gmail message watch data provider",
            "type": "GmailMessageWatchDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_observable": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        static GmailMessageEventDataType gmailMessageEventDataType;
    }

    #! Creates the object from a REST connection
    constructor(GoogleRestClient::GoogleRestClientIo rest, *hash<auto> options)
            : GmailMessageWatchDataProviderBase(rest, ConstructorOptions, options) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Google data %s provider for `%s`", ProviderInfo.name, rest.getSafeUrl());
    }

    #! Returns the message event data type
    static AbstractDataProviderType getMessageEventType() {
        return gmailMessageEventDataType ?? (gmailMessageEventDataType = new GmailMessageEventDataType());
    }

    #! Returns a hash of all supported event types
    /** @return a hash of all supported event types

        @note only called if the provider supports the observer pattern / event API
    */
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_EMAIL_MATCHED: <DataProviderMessageInfo>{
                "desc": "An email has been matched",
                "type": getMessageEventType(),
            },
        };
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Raise events
    private messageReceived(hash<auto> msg) {
        notifyObservers(EVENT_EMAIL_MATCHED, msg);
    }

    #! Returns example data for the given observable event
    /** @param event_id the event ID of the event

        @return example data for the given observable event

        @throw INVALID-OPERATION the data provider does not support the observer pattern / event API
        @throw INVALID-EVENT the event_id is not valid

        @since DataProvider 3.0
    */
    private auto getExampleEventDataImpl(string event_id) {
        hash<auto> info = setup() + {"test_mode": True};
        if (*hash<auto> msg = pollOnceIntern(\info)) {
            return msg;
        }
        return AbstractDataProvider::getExampleEventDataImpl(event_id);
    }
}

#! Event data type for new gmail events
public class GmailMessageEventDataType inherits HashDataType {
    public {
        const Fields = {
            "Subject": {
                "display_name": "Subject",
                "short_desc": "Email subject",
                "desc": "Email subject",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "From": {
                "display_name": "From Address",
                "short_desc": "From address",
                "desc": "From address",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "To": {
                "display_name": "To Addresses",
                "short_desc": "To addreses",
                "desc": "To addresses",
                "type": new SoftListDataType("*StringList", AbstractDataProviderTypeMap."string", True),
            },
            "Cc": {
                "display_name": "CC Addresses",
                "short_desc": "CC addresses",
                "desc": "CC - Carbon Copy addresses",
                "type": new SoftListDataType("*StringList", AbstractDataProviderTypeMap."string", True),
            },
            "Bcc": {
                "display_name": "BCC Addresses",
                "short_desc": "BCC addresses",
                "desc": "BCC - Blind Carbon Copy addresses",
                "type": new SoftListDataType("*StringList", AbstractDataProviderTypeMap."string", True),
            },
            "Return-Path": {
                "display_name": "Email Return / Bounce Path",
                "short_desc": "This is the address where email servers should send non-delivery notifications",
                "desc": "This is the address where email servers should send non-delivery notifications",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "Date": {
                "display_name": "Client Send Date",
                "short_desc": "The date the client sets in the email that should indicate when the email was sent",
                "desc": "The date the client sets in the email that should indicate when the email was sent",
                "type": AbstractDataProviderTypeMap."date",
            },
            "Delivered-To": {
                "display_name": "Delivered To",
                "short_desc": "The email address the email server delivered the email to",
                "desc": "The email address the email server delivered the email to",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "Message-ID": {
                "display_name": "unique Email ID",
                "short_desc": "A unique ID for this email",
                "desc": "A unique ID for this email",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "References": {
                "display_name": "Email Reference ID",
                "short_desc": "A unique ID for any email this email is in reply to",
                "desc": "A unique ID for any email this email is in reply to",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "Content-Type": {
                "display_name": "Content Type",
                "short_desc": "The MIME content type set on the content transfer encoding",
                "desc": "The MIME content type set on the content transfer encoding",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "Content-Transfer-Encoding": {
                "display_name": "Content Transfer Encoding",
                "short_desc": "The content transfer encoding set on the message or attachment",
                "desc": "The content transfer encoding set on the message or attachment",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "User-Agent": {
                "display_name": "Client Email User Agent",
                "short_desc": "Information about the client application used to send the email",
                "desc": "Information about the client application used to send the email",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };

        const PartFields = Fields{PartHeaders};

        const PartHeaders = (
            "Content-Type",
            "Content-Transfer-Encoding",
        );
    }

    constructor() : HashDataType("GmailWatchMessage") {
        HashDataType t = GoogleDataProviderBase::getInputTypeForSchema(GoogleDiscoveryGmailApiName, "Message");
        *hash<string, AbstractDataField> fields = t.getFields();

        if (QoreDataField payload = fields.payload) {
            HashDataType pt = payload.getType();
            *hash<string, AbstractDataField> pfields = pt.getFields();

            # update type for "parts" fields
            if (QoreDataField parts = pfields.parts) {
                ListDataType part_type = parts.getType();
                HashDataType part_element_type = part_type.getElementType().copy();
                *hash<string, AbstractDataField> pefields = part_element_type.getFields();

                if (QoreDataField headers = pefields.headers) {
                    headers = headers.copy();
                    part_element_type.replaceField(headers);
                    HashDataType header_type();
                    header_type.setDefaultOtherFieldType(AbstractDataProviderTypeMap."string");
                    headers.updateFieldType(header_type);

                    # add direct fields for important values
                    map header_type.addField(new QoreDataField({
                        "name": $1.key,
                    } + $1.value)), PartFields.pairIterator();
                }

                part_type = part_type.copyWithElementType(part_element_type);
                parts.updateFieldType(part_type);
            }

            # update type for "headers" field
            if (QoreDataField headers = pfields.headers) {
                HashDataType header_type();
                header_type.setDefaultOtherFieldType(AbstractDataProviderTypeMap."string");
                headers.updateFieldType(header_type);

                # add direct fields for important values
                map header_type.addField(new QoreDataField({
                    "name": $1.key,
                } + $1.value)), Fields.pairIterator();
            }
        }

        map addField($1), fields.iterator();
        addField(new QoreDataField({
            "name": "attachments",
            "display_name": "Attachments",
            "short_desc": "Message attachments",
            "desc": "Message attachments",
            "type": new ListDataType("AttachmentList", new GmailAttachmentEventDataType(), True),
        }));
    }
}
}
