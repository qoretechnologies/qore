# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MailgunDataProvider module definition

/*  MailgunBaseEventDataProvider.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Base class for Mailgun event data providers
/** This abstract class provides common functionality for all Mailgun webhook-based event providers.

    Mailgun webhooks are registered per-domain and deliver events via HTTP POST with HMAC-SHA256 signatures.
*/
public class MailgunBaseEventDataProvider inherits MailgunDataProviderBase, DataProvider::DelayedObservable {
    public {
        #! Local options (event-specific only - for use when REST client is provided)
        const LocalOptions = {
            "domain": <DataProviderOptionInfo>{
                "display_name": "Domain",
                "short_desc": "The domain to watch for events",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "The Mailgun domain to monitor for events. If not specified, uses the connection's default domain.",
                "ref_data": "domains",
            },
        };
    }

    private {
        #! Domain to watch
        *string domain;

        #! Webhook info returned from DataProviderWebhook::createWebhook()
        *hash<auto> webhook_info;

        #! Mailgun webhook name (the event type)
        *string webhook_name;
    }

    #! Creates the object with a REST client
    constructor(MailgunRestClient::MailgunRestClient rest, *string api_domain, *hash<auto> options)
            : MailgunDataProviderBase(rest, api_domain) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", LocalOptions, options);
        if (copts.domain) {
            domain = copts.domain.value ?? copts.domain;
        } else {
            domain = api_domain;
        }
    }

    #! Stops observing and deletes the object
    destructor() {
        stopEventsIntern();
    }

    #! Returns the event type that this provider handles
    /** Must be overridden by subclasses */
    abstract string getEventType();

    #! Returns the event data type
    /** Must be overridden by subclasses */
    abstract AbstractDataProviderType getEventDataType();

    #! Called when all observers have been added to the object
    observersReady() {
        # Get domain from context if not provided
        if (!domain) {
            *hash<auto> ctx = DataProvider::DataProviderDataContextHelper::getHash();
            # Check both "domain" (LocalOptions) and "api_domain" (BaseConstructorOptions)
            if (ctx.domain) {
                domain = ctx.domain;
            } else if (ctx.api_domain) {
                domain = ctx.api_domain;
            }
        }
        if (!domain) {
            throw "MAILGUN-ERROR", "domain is required to watch for events";
        }

        # Create local webhook endpoint
        webhook_info = DataProvider::DataProviderWebhook::createWebhook({
            "method": "POST",
            "callback": \handleWebhookEvent(),
        });

        # Mailgun webhook names are the event types
        webhook_name = getEventType();

        # Register webhook with Mailgun
        try {
            hash<auto> body = {
                "id": webhook_name,
                "url": webhook_info.url,
            };

            hash<auto> response = doPostFormEncoded(sprintf("%s/domains/%s/webhooks", ApiV3, domain), body);

            log(LoggerLevel::INFO, "MailgunBaseEventDataProvider: registered webhook %y for %s events on domain %s",
                webhook_info.url, webhook_name, domain);
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
        string event_type = getEventType();
        return {
            event_type: <DataProviderMessageInfo>{
                "desc": sprintf("A %s event has occurred", event_type),
                "type": getEventDataType(),
            },
        };
    }

    #! Returns example data for the given observable event
    private auto getExampleEventDataImpl(string event_id) {
        return {
            "event": getEventType(),
            "timestamp": now_us().toFloat(),
            "id": "example-event-id-12345",
            "recipient": "user@example.com",
            "message": {
                "headers": {
                    "message-id": "<example@mail.example.com>",
                    "subject": "Example Email",
                    "from": "sender@example.com",
                    "to": "user@example.com",
                },
            },
            "tags": ("tag1", "tag2"),
        };
    }

    #! Stops the webhook and cleans up
    private stopEventsIntern() {
        # Delete webhook from Mailgun first
        if (webhook_name && domain) {
            try {
                doDelete(sprintf("%s/domains/%s/webhooks/%s", ApiV3, domain, webhook_name));
                log(LoggerLevel::INFO, "MailgunBaseEventDataProvider: deleted Mailgun webhook %s for domain %s",
                    webhook_name, domain);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "MailgunBaseEventDataProvider: error deleting Mailgun webhook: %s",
                    get_exception_string(ex));
            }
            remove webhook_name;
        }

        # Delete local webhook endpoint
        if (webhook_info) {
            DataProvider::DataProviderWebhook::deleteWebhook(webhook_info);
            remove webhook_info;
        }
    }

    #! Handle incoming webhook events from Mailgun
    private handleWebhookEvent(hash<auto> payload) {
        string event_type = getEventType();
        log(LoggerLevel::DEBUG, "handleWebhookEvent: received %s event for domain %s", event_type, domain);

        # Verify webhook signature for security
        if (!verifyWebhookSignature(payload)) {
            log(LoggerLevel::ERROR, "handleWebhookEvent: invalid webhook signature, rejecting event");
            return;
        }

        # Mailgun webhook payload contains event-data
        hash<auto> event_data = payload."event-data" ?? payload;

        # Filter by event type if needed
        string received_type = event_data.event ?? "";
        if (received_type && received_type != event_type) {
            log(LoggerLevel::DEBUG, "handleWebhookEvent: ignoring event type %s (expecting %s)",
                received_type, event_type);
            return;
        }

        # Transform to standard format
        hash<auto> transformed = {
            "event": event_type,
            "timestamp": event_data.timestamp,
            "id": event_data.id,
            "recipient": event_data.recipient,
            "message": event_data.message,
            "tags": event_data.tags,
            "campaigns": event_data.campaigns,
            "user-variables": event_data."user-variables",
        };

        # Add event-specific fields
        transformed += getEventSpecificFields(event_data);

        notifyObservers(event_type, transformed);
    }

    #! Verifies the Mailgun webhook signature
    /** Mailgun signs webhooks using HMAC-SHA256 with the webhook signing key (typically the API key).
        The signature is computed as: HMAC-SHA256(timestamp + token, signing_key)

        @param payload the webhook payload containing the signature block
        @return True if signature is valid, False otherwise
    */
    private bool verifyWebhookSignature(hash<auto> payload) {
        # Get the signature block from the payload
        *hash<auto> sig = payload.signature;
        if (!sig) {
            log(LoggerLevel::WARN, "verifyWebhookSignature: no signature block in payload");
            return False;
        }

        # Extract signature components
        *string timestamp = string(sig.timestamp);
        *string token = sig.token;
        *string signature = sig.signature;

        if (!timestamp || !token || !signature) {
            log(LoggerLevel::WARN, "verifyWebhookSignature: missing signature components");
            return False;
        }

        # Get the webhook signing key
        *string signing_key = getWebhookSigningKey();
        if (!signing_key) {
            log(LoggerLevel::WARN, "verifyWebhookSignature: no webhook signing key configured, skipping verification");
            # Allow through if no signing key is configured (for backwards compatibility)
            return True;
        }

        # Compute expected signature: HMAC-SHA256(timestamp + token, signing_key)
        string data = timestamp + token;
        string expected_signature = hmac("sha256", signing_key, data).toHex().lwr();

        # Compare signatures
        if (expected_signature != signature.lwr()) {
            log(LoggerLevel::WARN, "verifyWebhookSignature: signature mismatch");
            return False;
        }

        log(LoggerLevel::DEBUG, "verifyWebhookSignature: signature verified successfully");
        return True;
    }

    #! Returns event-specific fields from the payload
    /** Can be overridden by subclasses to extract additional fields */
    private hash<auto> getEventSpecificFields(hash<auto> event_data) {
        return {};
    }
}

} # namespace MailgunDataProvider
