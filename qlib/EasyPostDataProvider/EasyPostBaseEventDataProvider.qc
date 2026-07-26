# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Base class for EasyPost webhook event providers
public class EasyPostBaseEventDataProvider inherits EasyPostDataProviderBase, DataProvider::DelayedObservable {
    public {
        #! Local options - none by default; subclasses may add some
        const LocalOptions = {};
    }

    private {
        #! Local webhook info returned from DataProviderWebhook::createWebhook()
        *hash<auto> webhook_info;

        #! EasyPost webhook ID (returned when subscribing)
        *string webhook_id;

        #! Optional shared secret for HMAC signature verification
        *string webhook_secret;
    }

    #! Creates the object with a REST client
    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options) : EasyPostDataProviderBase(rest) {
    }

    #! Stops observing and deletes the object
    destructor() {
        stopEventsIntern();
    }

    #! Returns the event type that this provider handles
    abstract string getEventType();

    #! Returns the EasyPost webhook event description (e.g. `tracker.created`)
    abstract string getWebhookEventType();

    #! Returns the event data type
    abstract AbstractDataProviderType getEventDataType();

    #! Called when all observers have been added to the object
    observersReady() {
        # Create the local webhook endpoint
        webhook_info = DataProvider::DataProviderWebhook::createWebhook({
            "method": "POST",
            "callback": \handleWebhookEvent(),
        });

        try {
            # Generate a random shared secret for HMAC verification
            webhook_secret = sprintf("qore-easypost-%s-%d",
                make_base64_string(get_random_bytes(16)), now_us().getEpochSeconds());

            hash<auto> body = {
                "url": webhook_info.url,
                "webhook_secret": webhook_secret,
                "custom_headers": (),
            };

            auto response = doPostWrapped("webhooks", WrapperKeyWebhook, body);
            if (response.typeCode() == NT_HASH) {
                webhook_id = response.id;
            }

            log(LoggerLevel::INFO, "EasyPostBaseEventDataProvider: registered webhook %y (id=%y) for %s",
                webhook_info.url, webhook_id, getEventType());
        } catch (hash<ExceptionInfo> ex) {
            if (webhook_info) {
                DataProvider::DataProviderWebhook::deleteWebhook(webhook_info);
                remove webhook_info;
            }
            rethrow;
        }
    }

    #! Stops generating events
    stopEvents() {
        stopEventsIntern();
    }

    #! Returns the supported event types
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        string event_type = getEventType();
        return {
            event_type: <DataProviderMessageInfo>{
                "desc": sprintf("EasyPost `%s` webhook event", getWebhookEventType()),
                "type": getEventDataType(),
            },
        };
    }

    #! Returns example event data
    /** Tries to fetch a real event of this type from the EasyPost API; on any failure
        falls back to the static fake event envelope.
    */
    private auto getExampleEventDataImpl(string event_id) {
        if (rest) {
            try {
                auto response = doGet("events", {"page_size": 10});
                *list<auto> evts = response.events ?? (response.typeCode() == NT_LIST ? response : ());
                foreach hash<auto> evt in (evts) {
                    if (evt.description == getWebhookEventType()) {
                        return evt;
                    }
                }
            } catch (hash<ExceptionInfo> ex) {
                # silently fall through to fake data
            }
        }
        return getFakeEventData();
    }

    #! Returns hardcoded fake example event data; subclasses override to populate `result`
    private hash<auto> getFakeEventData() {
        return {
            "id": "evt_example",
            "object": "Event",
            "mode": "production",
            "description": getWebhookEventType(),
            "status": "completed",
            "result": {},
            "user_id": "user_example",
            "created_at": now_us(),
            "updated_at": now_us(),
        };
    }

    #! Stops the webhook
    private stopEventsIntern() {
        if (webhook_id) {
            try {
                doDelete(sprintf("webhooks/%s", webhook_id));
                log(LoggerLevel::INFO, "EasyPostBaseEventDataProvider: deleted EasyPost webhook %y", webhook_id);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "EasyPostBaseEventDataProvider: error deleting EasyPost webhook: %s",
                    get_exception_string(ex));
            }
            remove webhook_id;
        }
        if (webhook_info) {
            DataProvider::DataProviderWebhook::deleteWebhook(webhook_info);
            remove webhook_info;
        }
    }

    #! Verifies the HMAC-SHA-256 signature on a webhook payload
    private bool verifyHmacSignature(string body, *string sig) {
        if (!webhook_secret.val() || !sig.val()) {
            return False;
        }
        # SHA256_hmac() returns the hex digest as a string
        string computed = SHA256_hmac(webhook_secret, body);
        return computed.lwr() == sig.lwr();
    }

    #! Handles an incoming webhook event
    private handleWebhookEvent(hash<auto> request) {
        string event_type = getEventType();
        string body_str;
        try {
            body_str = exists request.body ? string(request.body) : "";
        } catch (hash<ExceptionInfo> ex) {
            body_str = "";
        }

        # HMAC signature verification (enforced when a webhook secret is configured)
        *string sig = request.hdr."x-hmac-signature" ?? request.hdr."X-Hmac-Signature";
        if (webhook_secret.val()) {
            if (!sig.val()) {
                log(LoggerLevel::ERROR, "EasyPostBaseEventDataProvider: missing HMAC signature header for %s; "
                    "ignoring event", event_type);
                return;
            }
            if (!verifyHmacSignature(body_str, sig)) {
                log(LoggerLevel::ERROR, "EasyPostBaseEventDataProvider: HMAC signature mismatch for %s; ignoring event",
                    event_type);
                return;
            }
        }

        # The webhook body is itself an Event object
        hash<auto> payload;
        if (request.body.typeCode() == NT_HASH) {
            payload = request.body;
        } else {
            try {
                auto parsed = parse_json(body_str);
                if (parsed.typeCode() == NT_HASH) {
                    payload = parsed;
                }
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "EasyPostBaseEventDataProvider: cannot parse webhook body: %s", ex.desc);
                return;
            }
            if (!payload) {
                log(LoggerLevel::ERROR, "EasyPostBaseEventDataProvider: cannot parse webhook body");
                return;
            }
        }

        # Filter on description matching this provider's webhook type
        string received_type = payload.description ?? "";
        if (received_type != getWebhookEventType()) {
            log(LoggerLevel::DEBUG, "EasyPostBaseEventDataProvider: ignoring event %y (expected %y)",
                received_type, getWebhookEventType());
            return;
        }

        notifyObservers(event_type, payload);
    }
}

} # namespace EasyPostDataProvider
