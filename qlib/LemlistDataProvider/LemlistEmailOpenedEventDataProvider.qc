# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LemlistDataProvider module definition

/*  LemlistEmailOpenedEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LemlistDataProvider module
public namespace LemlistDataProvider {

#! Lemlist email opened event data provider
public class LemlistEmailOpenedEventDataProvider inherits LemlistBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "email-opened",
            "type": "LemlistEmailOpenedEventDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_observable": True,
        };

        #! Constructor options
        const ConstructorOptions = LemlistDataProviderBase::BaseConstructorOptions + LocalOptions;
    }

    private {
        #! Event data type
        static LemlistEmailOpenedEventDataType eventDataType();
    }

    #! Creates the object with a REST client
    constructor(LemlistRestClient::LemlistRestClient rest, *hash<auto> options)
            : LemlistBaseEventDataProvider(rest, options) {
    }

    #! Returns the data provider name
    string getName() {
        return EVENT_EMAIL_OPENED;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Triggers when a lead opens a Lemlist email";
    }

    #! Returns the event type
    string getEventType() {
        return EVENT_EMAIL_OPENED;
    }

    #! Returns the Lemlist webhook event type
    string getWebhookEventType() {
        return WEBHOOK_EMAILS_OPENED;
    }

    #! Returns the event data type
    AbstractDataProviderType getEventDataType() {
        return eventDataType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace LemlistDataProvider
