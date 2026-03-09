# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MailgunDataProvider module definition

/*  MailgunTemporaryFailEventDataProvider.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the MailgunDataProvider module
public namespace MailgunDataProvider {

#! Mailgun temporary failure event data provider - watches for soft bounce events
public class MailgunTemporaryFailEventDataProvider inherits MailgunBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "MailgunTemporaryFailEventDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_observable": True,
        };

        #! Constructor options
        const ConstructorOptions = MailgunDataProviderBase::BaseConstructorOptions + LocalOptions;
    }

    private {
        #! Event data type
        static MailgunTemporaryFailEventDataType eventDataType();
    }

    #! Creates the object with a REST client
    constructor(MailgunRestClient::MailgunRestClient rest, *string api_domain, *hash<auto> options)
            : MailgunBaseEventDataProvider(rest, api_domain, options) {
    }

    #! Returns the data provider name
    string getName() {
        return EVENT_TEMPORARY_FAIL;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Mailgun temporary failure event data provider - triggers on soft bounces (temporary delivery failures)";
    }

    #! Returns the event type
    string getEventType() {
        return EVENT_TEMPORARY_FAIL;
    }

    #! Returns the event data type
    AbstractDataProviderType getEventDataType() {
        return eventDataType;
    }

    #! Returns event-specific fields
    private hash<auto> getEventSpecificFields(hash<auto> event_data) {
        return {
            "severity": event_data.severity ?? "temporary",
            "reason": event_data.reason,
            "delivery-status": event_data."delivery-status",
            "envelope": event_data.envelope,
        };
    }

    #! Returns example data for the given observable event
    private auto getExampleEventDataImpl(string event_id) {
        return MailgunBaseEventDataProvider::getExampleEventDataImpl(event_id) + {
            "severity": "temporary",
            "reason": "generic",
            "delivery-status": {
                "code": 452,
                "message": "4.2.2 The email account that you tried to reach is over quota",
                "description": "Mailbox full",
                "attempt-no": 1,
            },
            "envelope": {
                "sender": "sender@example.com",
                "transport": "smtp",
                "targets": "full@example.com",
                "sending-ip": "198.51.100.1",
            },
        };
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace MailgunDataProvider
