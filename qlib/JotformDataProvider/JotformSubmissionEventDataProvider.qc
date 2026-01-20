# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JotformDataProvider module definition

/*  JotformSubmissionEventDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! JotForm submission event data provider - watches for new form submissions via webhooks
/** Note: JotForm webhooks only fire for submissions made through the form UI, not for
    API-created submissions. This is a platform limitation.
*/
public class JotformSubmissionEventDataProvider inherits JotformDataProviderBase, DataProvider::DelayedObservable {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "JotformSubmissionEventDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_observable": True,
        };

        #! Constructor options (full - includes base options for standalone construction)
        const ConstructorOptions = JotformDataProviderBase::BaseConstructorOptions + LocalOptions;

        #! Local options (event-specific only - for use when REST client is provided)
        const LocalOptions = {
            "formID": <DataProviderOptionInfo>{
                "display_name": "Form",
                "short_desc": "The form to watch for submissions",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The ID of the form to monitor for new submissions",
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

        #! Submission event data type
        static JotformSubmissionWebhookEventDataType submissionEventDataType();
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
        return EVENT_NEW_SUBMISSION;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "JotForm new submission event data provider - triggers when a form receives a new submission via UI";
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
            throw "JOTFORM-ERROR", "formID is required to watch for submissions";
        }

        # Create local webhook endpoint
        webhook_info = DataProvider::DataProviderWebhook::createWebhook({
            "method": "POST",
            "callback": \handleWebhookEvent(),
        });

        # Register webhook with JotForm
        try {
            hash<auto> response = doPost(sprintf("/form/%s/webhooks", formID), {
                "webhookURL": webhook_info.url,
            });

            # JotForm returns the webhook ID in the content
            if (response.content) {
                jotform_webhook_id = response.content;
            }

            log(LoggerLevel::INFO, "JotformSubmissionEventDataProvider: registered webhook %y for form %s",
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
            EVENT_NEW_SUBMISSION: <DataProviderMessageInfo>{
                "desc": "A new submission has been received on the form",
                "type": submissionEventDataType,
            },
        };
    }

    #! Returns example data for the given observable event
    private auto getExampleEventDataImpl(string event_id) {
        return {
            "submissionID": "5432109876543210987",
            "formID": formID ?? "123456789012345678",
            "formTitle": "Contact Form",
            "ip": "192.168.1.100",
            "createdAt": "2025-01-15 10:30:45",
            "answers": {
                "1": {"name": "fullName", "answer": "John Doe", "type": "control_fullname"},
                "2": {"name": "email", "answer": "john@example.com", "type": "control_email"},
                "3": {"name": "message", "answer": "Hello, this is a test submission.", "type": "control_textarea"},
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
                log(LoggerLevel::INFO, "JotformSubmissionEventDataProvider: deleted JotForm webhook %s",
                    jotform_webhook_id);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "JotformSubmissionEventDataProvider: error deleting JotForm webhook: %s",
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
        log(LoggerLevel::DEBUG, "handleWebhookEvent: received submission for form %s", formID);

        # Transform the JotForm webhook payload to our event format
        hash<auto> event_data = {
            "submissionID": payload.submissionID ?? payload.submission_id ?? "",
            "formID": payload.formID ?? payload.form_id ?? formID,
            "formTitle": payload.formTitle ?? payload.form_title,
            "ip": payload.ip ?? payload.IP,
            "createdAt": payload.createdAt ?? payload.created_at,
            "answers": payload.answers ?? payload,
            "rawRequest": payload,
        };

        notifyObservers(EVENT_NEW_SUBMISSION, event_data);
    }
}

} # namespace JotformDataProvider
