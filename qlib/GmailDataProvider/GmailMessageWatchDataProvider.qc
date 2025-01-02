# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GmailDataProvider module definition

/** GmailMessageWatchDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
    constructor(GoogleRestClient::GoogleRestClient rest, *hash<auto> options)
            : GmailMessageWatchDataProviderBase(rest, ConstructorOptions, options) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Google data %s provider for `%s`", ProviderInfo.name, rest.getSafeURL());
    }

    #! Returns a hash of all supported event types
    /** @return a hash of all supported event types

        @note only called if the provider supports the observer pattern / event API
    */
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_EMAIL_MATCHED: <DataProviderMessageInfo>{
                "desc": "An email has been matched",
                "type": gmailMessageEventDataType ?? (gmailMessageEventDataType = new GmailMessageEventDataType()),
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
        if (*hash<auto> msg = pollOnce(\info)) {
            return msg;
        }
        return AbstractDataProvider::getExampleEventDataImpl(event_id);
    }
}

#! Event data type for new gmail events
public class GmailMessageEventDataType inherits HashDataType {
    constructor() : HashDataType("GmailWatchMessage") {
        AbstractDataProviderType t = GoogleDataProviderBase::getTypeForSchema(GoogleDiscoveryGmailApiName, "Message");
        map addField($1), t.getFields().iterator();
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
