# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore StripeDataProvider module definition

/*  StripeBaseEventDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the StripeDataProvider module
public namespace StripeDataProvider {
#! Base class for Stripe webhook event data providers
/** Registers a webhook endpoint with Stripe when observers are ready, verifies the signature on every
    delivery, and notifies observers for the one event type the subclass watches.

    @section stripeevent_verification Signature Verification

    A Stripe webhook endpoint is a URL anyone can post to.  Without verification it would accept a
    fabricated \c charge.succeeded — enough to make an integration ship goods that were never paid for —
    or a fabricated \c invoice.payment_failed, enough to suspend a paying customer's account.  So a
    delivery whose signature cannot be verified is **dropped**, and one that arrives before the endpoint
    has been created is dropped too; there is no configuration that turns this off.

    Stripe sends a \c Stripe-Signature header holding the time it signed at and one or more HMAC-SHA256
    signatures over \c "{timestamp}.{body}", with more than one present while a secret is being rotated.
    The timestamp is checked against @ref ReplayTolerance so that a captured delivery cannot be replayed
    indefinitely.

    @note The signature covers the **exact bytes** Stripe sent.  A body that has been parsed into a hash
    and re-serialized is a different sequence of bytes, so a delivery that arrives already parsed cannot
    be verified and is dropped with an error saying so, rather than being accepted unchecked.

    @section stripeevent_version The Pinned API Version

    The webhook endpoint is created with an explicit \c api_version, so Stripe formats event payloads to
    the same release the actions are generated from.  Without it Stripe uses the account's default
    version, and the payload shape could differ from what this module documents.
*/
public class StripeBaseEventDataProvider inherits AbstractDataProvider, DataProvider::DelayedObservable {
    public {
        #! How old a signed delivery may be before it is treated as a replay
        /** Stripe's own libraries default to five minutes; the value has to allow for clock skew between
            Stripe and this host without leaving a captured delivery valid indefinitely.
        */
        const ReplayTolerance = 5m;

        #! The header carrying the signature
        const SignatureHeader = "stripe-signature";

        #! The key in the signature header holding the time the payload was signed
        const SignatureTimestampKey = "t";

        #! The key in the signature header holding an HMAC-SHA256 signature
        const SignatureSchemeKey = "v1";

        #! The Stripe API path for managing webhook endpoints
        const WebhookEndpointPath = "/v1/webhook_endpoints";
    }

    private {
        #! The REST client used for API calls
        *StripeRestClient::StripeRestClientIo rest;

        #! Webhook info returned from DataProviderWebhook::createWebhook()
        *hash<auto> webhook_info;

        #! The ID of the webhook endpoint created in Stripe
        *string endpoint_id;

        #! The signing secret Stripe returned when the endpoint was created
        *string webhook_secret;
    }

    #! Creates the object with a REST client
    constructor(*StripeRestClient::StripeRestClientIo rest) {
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

    #! Returns the Stripe event type this provider watches
    /** ex: \c "charge.succeeded"
    */
    abstract string getStripeEventType();

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
        registerStripeEndpoint(webhook_info.url);
        log(LoggerLevel::INFO, "%s: webhook endpoint created at %y for Stripe event %y", self.className(),
            webhook_info.url, getStripeEventType());
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
                hash<auto> resp = rest.restGet(sprintf("/v1/events?type=%s&limit=1", getStripeEventType()));
                if (resp.body.data) {
                    return resp.body.data[0];
                }
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::DEBUG, "%s: could not retrieve a recent %y event: %s", self.className(),
                    getStripeEventType(), get_exception_string(ex));
            }
        }
        return getStaticExampleEvent();
    }

    #! Returns a static example event used when no real one can be retrieved
    private hash<auto> getStaticExampleEvent() {
        return {
            "id": "evt_00000000000000",
            "object": "event",
            "api_version": StripeSchema::Provenance.api_version,
            "created": 1709294400,
            "livemode": False,
            "type": getStripeEventType(),
            "data": {
                "object": getExampleObject(),
            },
        };
    }

    #! Returns the example object carried by this event type
    abstract hash<auto> getExampleObject();

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{
            "type": self.className(),
            "supports_observable": True,
        };
    }

    #! Creates the webhook endpoint in Stripe
    /** Pinned to the API version the actions are generated from, so that event payloads are formatted to
        the release this module documents rather than to whatever the account currently defaults to.
    */
    private registerStripeEndpoint(string url) {
        if (!rest) {
            throw "STRIPE-WEBHOOK-ERROR", sprintf("cannot register a webhook endpoint for %y: no REST client "
                "is available", getStripeEventType());
        }
        on_error rethrow $1.err, sprintf("%s (while registering a Stripe webhook endpoint for %y)", $1.desc,
            getStripeEventType());
        hash<auto> resp = rest.restPost(WebhookEndpointPath, {
            "url": url,
            "enabled_events": (getStripeEventType(),),
            "api_version": StripeSchema::Provenance.api_version,
            "description": sprintf("%s %s", StripeDataProvider::AppName, getEventId()),
        });
        endpoint_id = resp.body.id;
        webhook_secret = resp.body.secret;
        if (!webhook_secret.val()) {
            # without the signing secret nothing that arrives can be verified, and an endpoint that cannot
            # verify is worse than no endpoint at all
            throw "STRIPE-WEBHOOK-ERROR", sprintf("Stripe returned no signing secret for webhook endpoint "
                "%y; deliveries could not be verified", endpoint_id);
        }
    }

    #! Stops the webhook and removes the endpoint from Stripe
    private stopEventsIntern() {
        if (endpoint_id.val() && rest) {
            try {
                rest.restDel(WebhookEndpointPath + "/" + endpoint_id);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::WARN, "%s: could not delete Stripe webhook endpoint %y: %s", self.className(),
                    endpoint_id, get_exception_string(ex));
            }
            remove endpoint_id;
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
        # an endpoint is created per event type, but Stripe can be configured to send more, and a delivery
        # is not trustworthy input just because it was signed
        if (event.type != getStripeEventType()) {
            log(LoggerLevel::DEBUG, "%s: ignoring event type %y (watching %y)", self.className(), event.type,
                getStripeEventType());
            return;
        }
        log(LoggerLevel::DEBUG, "%s: received %s event %y", self.className(), event.type, event.id);
        notifyObservers(getEventId(), event);
    }

    #! Verifies a delivery's signature and returns the parsed event, or nothing if it must be dropped
    private *hash<auto> verifyAndParse(hash<auto> request) {
        if (!webhook_secret.val()) {
            log(LoggerLevel::ERROR, "%s: dropping a delivery received before the webhook endpoint was "
                "registered; it cannot be verified", self.className());
            return;
        }

        # the signature covers the exact bytes Stripe sent, so a body that has already been parsed cannot
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

        hash<string, list<string>> parts = DataProvider::WebhookSignature::parseKeyValueHeader(sig_header);
        *string timestamp = parts{SignatureTimestampKey}[0];
        *list<string> signatures = parts{SignatureSchemeKey};
        if (!timestamp.val() || !signatures) {
            log(LoggerLevel::ERROR, "%s: dropping a delivery whose %s header has no %y and %y values: %y",
                self.className(), SignatureHeader, SignatureTimestampKey, SignatureSchemeKey, sig_header);
            return;
        }

        if (!DataProvider::WebhookSignature::verify(<DataProvider::WebhookSignatureInfo>{
            "secret": webhook_secret,
            # Stripe signs the timestamp and the body joined with a "."
            "payload": timestamp + "." + body.toString(),
            "signatures": signatures,
            "timestamp": gmtime(timestamp.toInt()),
            "tolerance": ReplayTolerance,
        })) {
            log(LoggerLevel::ERROR, "%s: dropping a delivery whose signature did not verify", self.className());
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
