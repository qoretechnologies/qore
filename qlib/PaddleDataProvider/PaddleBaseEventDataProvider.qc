# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore PaddleDataProvider module definition

/*  PaddleBaseEventDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the PaddleDataProvider module
public namespace PaddleDataProvider {
#! Base class for Paddle webhook event data providers
/** Registers a notification destination with Paddle when observers are ready, verifies the signature on
    every delivery, and notifies observers for the one event type the subclass watches.

    @section paddleevent_polling Why Webhooks and Not Polling

    The application this module replaces watched Paddle by listing entities ordered by creation time and
    remembering the newest ID seen.  That cannot see anything created and removed between two polls, cannot
    see an update at all, and costs a request per interval whether or not anything happened.  A webhook
    delivery is the event itself, so nothing is inferred and nothing is missed.

    @section paddleevent_verification Signature Verification

    A webhook destination is a URL anyone can post to.  Without verification it would accept a fabricated
    \c transaction.completed — enough to make an integration provision a paid plan nobody paid for — or a
    fabricated \c subscription.canceled, enough to cut off a paying customer.  So a delivery whose signature
    cannot be verified is **dropped**, and one that arrives before the destination has been created is
    dropped too; there is no configuration that turns this off.

    Paddle sends a \c Paddle-Signature header holding the time it signed at and an HMAC-SHA256 signature
    over \c "{timestamp}:{body}", as \c "ts=1671552777;h1=eb4d0d…" — semicolon-separated, where Stripe uses
    a comma, and joined with a colon where Stripe uses a full stop.  The timestamp is checked against
    @ref ReplayTolerance so that a captured delivery cannot be replayed indefinitely.

    @note The signature covers the **exact bytes** Paddle sent.  A body that has been parsed into a hash and
    re-serialized is a different sequence of bytes, so a delivery that arrives already parsed cannot be
    verified and is dropped with an error saying so, rather than being accepted unchecked.

    @section paddleevent_destinations Notification Destinations

    One destination is created per subscribed event type and deleted when the subscription stops.  Paddle
    caps how many destinations an account may have, so an account that subscribes to a great many events at
    once can reach that cap; Paddle reports it, and the error is propagated rather than swallowed.
*/
public class PaddleBaseEventDataProvider inherits AbstractDataProvider, DataProvider::DelayedObservable {
    public {
        #! How old a signed delivery may be before it is treated as a replay
        /** Paddle's own libraries default to five seconds, which is too tight to survive ordinary clock
            skew between Paddle and this host; five minutes matches the tolerance used for other webhook
            sources here and still bounds how long a captured delivery stays usable.
        */
        const ReplayTolerance = 5m;

        #! The header carrying the signature
        const SignatureHeader = "paddle-signature";

        #! The character separating one key/value pair from the next in the signature header
        const SignatureSeparator = ";";

        #! The character joining the timestamp to the body in the signed payload
        const SignaturePayloadSeparator = ":";

        #! The key in the signature header holding the time the payload was signed
        const SignatureTimestampKey = "ts";

        #! The key in the signature header holding an HMAC-SHA256 signature
        const SignatureSchemeKey = "h1";

        #! The Paddle API path for managing notification destinations
        const NotificationSettingPath = "/notification-settings";

        #! The notification destination type for a webhook endpoint
        const NotificationTypeUrl = "url";
    }

    private {
        #! The REST client used for API calls
        *PaddleRestClient::PaddleRestClientIo rest;

        #! Webhook info returned from DataProviderWebhook::createWebhook()
        *hash<auto> webhook_info;

        #! The ID of the notification destination created in Paddle
        *string notification_setting_id;

        #! The signing secret Paddle returned when the destination was created
        *string webhook_secret;
    }

    #! Creates the object with a REST client
    constructor(*PaddleRestClient::PaddleRestClientIo rest) {
        self.rest = rest;
    }

    #! Stops observing and deletes the object
    destructor() {
        stopEventsIntern();
    }

    #! Returns the data provider name
    string getName() {
        return getEventId();
    }

    #! Returns the Paddle event type this provider watches
    /** ex: \c "transaction.completed"
    */
    abstract string getPaddleEventType();

    #! Returns the data provider event ID
    abstract string getEventId();

    #! Returns a description of what the event means
    abstract string getEventDesc();

    #! Called when all observers have been added to the object
    observersReady() {
        webhook_info = DataProvider::DataProviderWebhook::createWebhook({
            "method": "POST",
            "callback": \handleWebhookEvent(),
        });
        # if Paddle will not accept the destination, give the local URL back rather than leaving one
        # allocated that nothing will ever post to
        {
            on_error {
                try {
                    DataProvider::DataProviderWebhook::deleteWebhook(webhook_info);
                } catch (hash<ExceptionInfo> ex) {
                    log(LoggerLevel::WARN, "%s: could not release the webhook URL %y after registration "
                        "failed: %s", self.className(), webhook_info.url, get_exception_string(ex));
                }
                remove webhook_info;
            }
            registerNotificationSetting(webhook_info.url);
        }
        log(LoggerLevel::INFO, "%s: notification destination created at %y for Paddle event %y",
            self.className(), webhook_info.url, getPaddleEventType());
    }

    #! Called to stop generating events
    stopEvents() {
        stopEventsIntern();
    }

    #! Returns a hash of all supported event types
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            getEventId(): <DataProviderMessageInfo>{
                "desc": getEventDesc(),
                "type": AbstractDataProviderTypeMap."hash",
            },
        };
    }

    #! Returns example data for the given observable event
    /** Retrieves a recent real event of this type where possible, so that what is shown is what this
        account actually receives; falls back to a static example when there is none or the call fails.
    */
    private auto getExampleEventDataImpl(string event_id) {
        if (rest) {
            try {
                hash<auto> resp = rest.restGet(sprintf("/events?event_type=%s&per_page=1",
                    getPaddleEventType()));
                if (resp.body.data) {
                    return resp.body.data[0];
                }
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::DEBUG, "%s: could not retrieve a recent %y event: %s", self.className(),
                    getPaddleEventType(), get_exception_string(ex));
            }
        }
        return getStaticExampleEvent();
    }

    #! Returns a static example event used when no real one can be retrieved
    private hash<auto> getStaticExampleEvent() {
        return {
            "event_id": "evt_01h8b8b8b8b8b8b8b8b8b8b8b8",
            "notification_id": "ntf_01h8b8b8b8b8b8b8b8b8b8b8b8",
            "event_type": getPaddleEventType(),
            "occurred_at": "2026-08-09T12:00:00.000000Z",
            "data": getExampleObject(),
        };
    }

    #! Returns the example entity carried by this event type
    abstract hash<auto> getExampleObject();

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{
            "type": self.className(),
            "supports_observable": True,
        };
    }

    #! Creates the notification destination in Paddle
    private registerNotificationSetting(string url) {
        if (!rest) {
            throw "PADDLE-WEBHOOK-ERROR", sprintf("cannot register a notification destination for %y: no "
                "REST client is available", getPaddleEventType());
        }
        on_error rethrow $1.err, sprintf("%s (while registering a Paddle notification destination for %y)",
            $1.desc, getPaddleEventType());
        hash<auto> resp = rest.restPost(NotificationSettingPath, {
            "description": sprintf("%s %s", PaddleDataProvider::AppName, getEventId()),
            "type": NotificationTypeUrl,
            "destination": url,
            "subscribed_events": (getPaddleEventType(),),
        });
        notification_setting_id = resp.body.data.id;
        webhook_secret = resp.body.data.endpoint_secret_key;
        if (!webhook_secret.val()) {
            # without the signing secret nothing that arrives can be verified, and a destination that
            # cannot verify is worse than no destination at all
            throw "PADDLE-WEBHOOK-ERROR", sprintf("Paddle returned no signing secret for notification "
                "destination %y; deliveries could not be verified", notification_setting_id);
        }
    }

    #! Stops the webhook and removes the notification destination from Paddle
    private stopEventsIntern() {
        if (notification_setting_id.val() && rest) {
            try {
                rest.restDel(NotificationSettingPath + "/" + notification_setting_id);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::WARN, "%s: could not delete Paddle notification destination %y: %s",
                    self.className(), notification_setting_id, get_exception_string(ex));
            }
            remove notification_setting_id;
        }
        remove webhook_secret;
        if (webhook_info) {
            DataProvider::DataProviderWebhook::deleteWebhook(webhook_info);
            remove webhook_info;
        }
    }

    #! Handles an incoming webhook delivery
    private handleWebhookEvent(hash<auto> request) {
        *hash<auto> event = verifyAndParse(request);
        if (!event) {
            return;
        }
        # a destination is created per event type, but one can be configured in the Paddle dashboard to
        # send more, and a delivery is not trustworthy input just because it was signed
        if (event.event_type != getPaddleEventType()) {
            log(LoggerLevel::DEBUG, "%s: ignoring event type %y (watching %y)", self.className(),
                event.event_type, getPaddleEventType());
            return;
        }
        log(LoggerLevel::DEBUG, "%s: received %s event %y", self.className(), event.event_type,
            event.event_id);
        notifyObservers(getEventId(), event);
    }

    #! Verifies a delivery's signature and returns the parsed event, or nothing if it must be dropped
    private *hash<auto> verifyAndParse(hash<auto> request) {
        if (!webhook_secret.val()) {
            log(LoggerLevel::ERROR, "%s: dropping a delivery received before the notification destination "
                "was registered; it cannot be verified", self.className());
            return;
        }

        # the signature covers the exact bytes Paddle sent, so a body that has already been parsed cannot
        # be verified: re-serializing it produces different bytes
        *data body = getRawBody(request);
        if (!exists body) {
            log(LoggerLevel::ERROR, "%s: dropping a delivery whose raw body is not available; the signature "
                "covers the bytes as sent and cannot be checked against a re-serialized body",
                self.className());
            return;
        }

        *string sig_header = getSignatureHeader(request);
        if (!sig_header.val()) {
            log(LoggerLevel::ERROR, "%s: dropping a delivery with no %s header", self.className(),
                SignatureHeader);
            return;
        }

        hash<string, list<string>> parts = DataProvider::WebhookSignature::parseKeyValueHeader(sig_header,
            SignatureSeparator);
        *string timestamp = parts{SignatureTimestampKey}[0];
        *list<string> signatures = parts{SignatureSchemeKey};
        if (!timestamp.val() || !signatures) {
            log(LoggerLevel::ERROR, "%s: dropping a delivery whose %s header has no %y and %y values: %y",
                self.className(), SignatureHeader, SignatureTimestampKey, SignatureSchemeKey, sig_header);
            return;
        }

        if (!DataProvider::WebhookSignature::verify(<DataProvider::WebhookSignatureInfo>{
            "secret": webhook_secret,
            # Paddle signs the timestamp and the body joined with a ":"
            "payload": timestamp + SignaturePayloadSeparator + body.toString(),
            "signatures": signatures,
            "timestamp": gmtime(timestamp.toInt()),
            "tolerance": ReplayTolerance,
        })) {
            log(LoggerLevel::ERROR, "%s: dropping a delivery whose signature did not verify",
                self.className());
            return;
        }

        auto parsed = parse_json(body.toString());
        if (parsed.typeCode() != NT_HASH) {
            log(LoggerLevel::ERROR, "%s: dropping a delivery whose body is not a JSON object (type %y)",
                self.className(), parsed.type());
            return;
        }
        return parsed;
    }

    #! Returns the raw body bytes of a delivery, if available
    private *data getRawBody(hash<auto> request) {
        # the webhook infrastructure may present the unparsed body under either name; a body that arrives
        # already parsed is deliberately not accepted here
        foreach string key in (("body_raw", "body-raw", "raw_body", "body")) {
            auto value = request{key};
            switch (value.typeCode()) {
                case NT_STRING:
                case NT_BINARY:
                    return value;
            }
        }
    }

    #! Returns the signature header of a delivery, if present
    private *string getSignatureHeader(hash<auto> request) {
        *hash<auto> hdr = request.hdr ?? request.headers;
        foreach hash<auto> i in (hdr.pairIterator()) {
            if (i.key.lwr() == SignatureHeader) {
                return i.value.toString();
            }
        }
    }
}
}
