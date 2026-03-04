# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GoHighLevelWatchNewMessageDataProvider.qc GoHighLevel watch new message trigger provider

/*  GoHighLevelWatchNewMessageDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

%modern

#! Contains all public definitions in the GoHighLevelDataProvider module
public namespace GoHighLevelDataProvider {

#! GoHighLevel message record type
public class GoHighLevelMessageDataType inherits HashDataType {
    public {
        #! Field definitions for GoHighLevel messages
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Message ID",
                "short_desc": "Unique identifier of the message",
                "desc": "Unique identifier of the message",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The message type",
                "desc": "Message type: `SMS`, `Email`, `WhatsApp`, `FB`, `IG`, `Live_Chat`",
            },
            "body": {
                "type": StringOrNothingType,
                "display_name": "Body",
                "short_desc": "The message body content",
                "desc": "The message body content",
            },
            "direction": {
                "type": StringOrNothingType,
                "display_name": "Direction",
                "short_desc": "The message direction",
                "desc": "The message direction (inbound or outbound)",
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "The message delivery status",
                "desc": "The message delivery status",
            },
            "contactId": {
                "type": StringOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The contact associated with this message",
                "desc": "The contact associated with this message",
            },
            "conversationId": {
                "type": StringOrNothingType,
                "display_name": "Conversation ID",
                "short_desc": "The conversation this message belongs to",
                "desc": "The conversation this message belongs to",
            },
            "dateAdded": {
                "type": StringOrNothingType,
                "display_name": "Date Added",
                "short_desc": "Timestamp when the message was created",
                "desc": "Timestamp when the message was created",
            },
            "messageType": {
                "type": StringOrNothingType,
                "display_name": "Message Type",
                "short_desc": "The type classification of the message",
                "desc": "The type classification of the message",
            },
            "locationId": {
                "type": StringOrNothingType,
                "display_name": "Location ID",
                "short_desc": "The location/sub-account ID",
                "desc": "The location/sub-account ID",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Watch for new inbound messages in GoHighLevel
public class GoHighLevelWatchNewMessageDataProvider inherits GoHighLevelWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_MESSAGE,
            "desc": "Watch for new inbound messages in GoHighLevel",
            "type": "GoHighLevelWatchNewMessageDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(GoHighLevelRestClient::GoHighLevelRestClient rest, *hash<auto> options)
            : GoHighLevelWatchDataProviderBase(rest, options) {
    }

    #! Returns the provider name
    string getName() {
        return EVENT_NEW_MESSAGE;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Watch for new inbound messages in GoHighLevel";
    }

    #! Fetches new messages since the given date
    private list<hash<auto>> fetchRecords(date since) {
        hash<auto> params = {
            "startAfterDate": formatDateTimeGHL(since),
            "limit": DefaultPageSize,
        };
        hash<auto> response = doGet("conversations/search", params);
        return response.conversations ?? ();
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_NEW_MESSAGE;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the data type for message event records
    private AbstractDataProviderType getRecordDataType() {
        return new GoHighLevelMessageDataType();
    }

    #! Returns fake example data for messages
    private hash<auto> getFakeExampleData() {
        return {
            "id": "EXAMPLE_MESSAGE_12345",
            "type": "SMS",
            "body": "Hello, I would like to schedule an appointment.",
            "direction": "inbound",
            "status": "delivered",
            "contactId": "EXAMPLE_CONTACT_12345",
            "conversationId": "EXAMPLE_CONVERSATION_12345",
            "dateAdded": "2026-01-30T10:30:00.000Z",
            "messageType": "TYPE_SMS",
            "locationId": "EXAMPLE_LOCATION_12345",
        };
    }
}

} # namespace GoHighLevelDataProvider
