# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file TableauWebhookEventDataProviderBase.qc Base class for Tableau webhook event data providers

/*  TableauWebhookEventDataProviderBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the TableauDataProvider module
public namespace TableauDataProvider {

#! Base class for Tableau webhook event data providers
public class TableauWebhookEventDataProviderBase inherits TableauDataProviderBase,
        DataProvider::DelayedObservable {
    public {
        #! Constructor options for webhook providers
        const WebhookConstructorOptions = BaseConstructorOptions;
    }

    private {
        #! The event name for this provider
        string event_name;

        #! The Tableau webhook event type string
        string tableau_event_type;

        #! The webhook ID if registered
        *string webhook_id;
    }

    #! Creates the object with a REST client
    constructor(TableauRestClient::TableauRestClient rest, string event_name,
            string tableau_event_type, *hash<auto> options)
            : TableauDataProviderBase(rest) {
        self.event_name = event_name;
        self.tableau_event_type = tableau_event_type;
    }

    #! Called when observers are ready
    /** Webhook registration is handled by the Qorus platform, which provides the webhook URL
        and routes events to this provider
    */
    observersReady() {
        # Webhook registration is handled by the Qorus platform
    }

    #! Stops event generation
    /** Webhook deregistration is handled by the Qorus platform
    */
    stopEvents() {
        # Webhook deregistration is handled by the Qorus platform
    }

    #! Returns a hash of all supported event types
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            event_name: <DataProviderMessageInfo>{
                "desc": sprintf("Fires when a Tableau %s event occurs", tableau_event_type),
                "type": new TableauWebhookEventDataType(),
            },
        };
    }

    #! Returns example event data for documentation/testing
    private auto getExampleEventDataImpl(string event_id) {
        return getExampleEventData();
    }

    #! Returns example webhook event data
    hash<auto> getExampleEventData() {
        return {
            "resource": "DATASOURCE",
            "event_type": tableau_event_type,
            "resource_name": "Example Datasource",
            "resource_luid": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
            "site_luid": "f1e2d3c4-b5a6-7890-abcd-ef1234567890",
            "created_at": now_us().format("YYYY-MM-DDTHH:mm:ssZ"),
        };
    }
}

} # namespace TableauDataProvider
