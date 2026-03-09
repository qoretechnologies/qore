# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JotformDataProvider module definition

/*  JotformPaymentEventDataProvider.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the JotformDataProvider module
public namespace JotformDataProvider {

#! JotForm payment event data provider - watches for payment submissions via webhooks
/** Note: JotForm webhooks only fire for payments made through the form UI, not for
    API-created submissions. This is a platform limitation.
*/
public class JotformPaymentEventDataProvider inherits JotformDataProviderBase, DataProvider::DelayedObservable {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "JotformPaymentEventDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_observable": True,
        };

        #! Constructor options (full - includes base options for standalone construction)
        const ConstructorOptions = JotformDataProviderBase::BaseConstructorOptions + LocalOptions;

        #! Local options (event-specific only - for use when REST client is provided)
        const LocalOptions = {
            "formID": <DataProviderOptionInfo>{
                "display_name": "Form",
                "short_desc": "The form to watch for payments",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The ID of the form (with payment integration) to monitor for payments",
            },
        };
    }

    private {
        #! Form ID to watch
        *string formID;

        #! Webhook info returned from DataProviderWebhook::createWebhook()
        *hash<auto> webhook_info;

        #! JotForm webhook ID for cleanup
        *string jotform_webhook_id;

        #! Payment event data type
        static JotformPaymentWebhookEventDataType paymentEventDataType();
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) : JotformDataProviderBase(options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.formID) {
            formID = copts.formID.value ?? copts.formID;
        }
    }

    #! Creates the object with a REST client
    constructor(JotformRestClient::JotformRestClient rest, *hash<auto> options) : JotformDataProviderBase(rest) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", LocalOptions, options);
        if (copts.formID) {
            formID = copts.formID.value ?? copts.formID;
        }
    }

    #! Stops observing and deletes the object
    destructor() {
        stopEventsIntern();
    }

    #! Returns the data provider name
    string getName() {
        return EVENT_NEW_PAYMENT;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "JotForm payment event data provider - triggers when a payment is received on a form";
    }

    #! Called when all observers have been added to the object
    observersReady() {
        # Get formID from context if not provided in constructor
        if (!formID) {
            *hash<auto> ctx = DataProvider::DataProviderDataContextHelper::getHash();
            if (ctx.formID) {
                formID = ctx.formID;
            }
        }
        if (!formID) {
            throw "JOTFORM-ERROR", "formID is required to watch for payments";
        }

        # Create local webhook endpoint
        webhook_info = DataProvider::DataProviderWebhook::createWebhook({
            "method": "POST",
            "callback": \handleWebhookEvent(),
        });

        # Register webhook with JotForm
        # Note: JotForm uses the same webhook endpoint for all events, including payments
        try {
            hash<auto> response = doPost(sprintf("/form/%s/webhooks", formID), {
                "webhookURL": webhook_info.url,
            });

            # JotForm returns the webhook ID in the content
            if (response.content) {
                jotform_webhook_id = response.content;
            }

            log(LoggerLevel::INFO, "JotformPaymentEventDataProvider: registered webhook %y for form %s",
                webhook_info.url, formID);
        } catch (hash<ExceptionInfo> ex) {
            # Clean up local webhook on failure
            if (webhook_info) {
                DataProvider::DataProviderWebhook::deleteWebhook(webhook_info);
                remove webhook_info;
            }
            rethrow;
        }
    }

    #! Called to stop generating events
    stopEvents() {
        stopEventsIntern();
    }

    #! Returns a hash of all supported event types
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_NEW_PAYMENT: <DataProviderMessageInfo>{
                "desc": "A payment has been received on the form",
                "type": paymentEventDataType,
            },
        };
    }

    #! Returns example data for the given observable event
    private auto getExampleEventDataImpl(string event_id) {
        return {
            "submissionID": "5432109876543210987",
            "formID": formID ?? "123456789012345678",
            "formTitle": "Order Form",
            "ip": "192.168.1.100",
            "createdAt": "2025-01-15 10:30:45",
            "paymentType": "stripe",
            "paymentAmount": "99.99",
            "paymentCurrency": "USD",
            "paymentStatus": "COMPLETED",
            "transactionID": "txn_1234567890abcdef",
            "answers": {
                "1": {"name": "product", "answer": "Premium Plan", "type": "control_dropdown"},
                "2": {"name": "quantity", "answer": "1", "type": "control_number"},
            },
            "rawRequest": {},
        };
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Stops the webhook and cleans up
    private stopEventsIntern() {
        # Delete webhook from JotForm first
        if (jotform_webhook_id && formID) {
            try {
                doDelete(sprintf("/form/%s/webhooks/%s", formID, jotform_webhook_id));
                log(LoggerLevel::INFO, "JotformPaymentEventDataProvider: deleted JotForm webhook %s",
                    jotform_webhook_id);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "JotformPaymentEventDataProvider: error deleting JotForm webhook: %s",
                    get_exception_string(ex));
            }
            remove jotform_webhook_id;
        }

        # Delete local webhook endpoint
        if (webhook_info) {
            DataProvider::DataProviderWebhook::deleteWebhook(webhook_info);
            remove webhook_info;
        }
    }

    #! Handle incoming webhook events from JotForm
    private handleWebhookEvent(hash<auto> payload) {
        # Check if this is a payment event (has payment-related fields)
        if (!hasPaymentData(payload)) {
            # Not a payment event, skip it
            return;
        }

        log(LoggerLevel::DEBUG, "handleWebhookEvent: received payment for form %s", formID);

        # Transform the JotForm webhook payload to our event format
        hash<auto> event_data = {
            "submissionID": payload.submissionID ?? payload.submission_id ?? "",
            "formID": payload.formID ?? payload.form_id ?? formID,
            "formTitle": payload.formTitle ?? payload.form_title,
            "ip": payload.ip ?? payload.IP,
            "createdAt": payload.createdAt ?? payload.created_at,
            "paymentType": payload.paymentType ?? payload.payment_type,
            "paymentAmount": payload.paymentAmount ?? payload.payment_amount ?? payload.total,
            "paymentCurrency": payload.paymentCurrency ?? payload.payment_currency ?? "USD",
            "paymentStatus": payload.paymentStatus ?? payload.payment_status ?? "COMPLETED",
            "transactionID": payload.transactionID ?? payload.transaction_id,
            "answers": payload.answers ?? {},
            "rawRequest": payload,
        };

        notifyObservers(EVENT_NEW_PAYMENT, event_data);
    }

    #! Checks if the webhook payload contains payment data
    private bool hasPaymentData(hash<auto> payload) {
        # Check for common payment-related fields
        return (payload.paymentType.val() || payload.payment_type.val() ||
            payload.paymentAmount.val() || payload.payment_amount.val() ||
            payload.total.val() || payload.transactionID.val() ||
            payload.transaction_id.val());
    }
}

} # namespace JotformDataProvider
