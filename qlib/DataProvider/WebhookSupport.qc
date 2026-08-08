# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DataProvider webhook support

/** WebhookSupport.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DataProvider module
public namespace DataProvider {

#! Static webhook API callbacks
/** This class provides a static API for data providers to create and delete webhooks.
    The webhook infrastructure must be registered by the runtime environment
    before data providers can use these methods.

    @par Webhook Creation Callback
    The creation callback takes a hash with the following keys:
    - \c method (string): the HTTP method for the webhook (e.g., "POST")
    - \c callback (code): a callback taking the webhook payload to generate events
    - \c auth (*int): optional authentication requirement
    - \c perms (*list&lt;string&gt;): optional list of permissions

    Returns a hash with:
    - \c url (string): the public URL for the webhook
    - \c ctx (auto): context object for webhook deletion

    @par Webhook Deletion Callback
    The deletion callback takes a hash with \c url and \c ctx keys (as returned by the create callback)
*/
public class DataProviderWebhook {
    public {
        #! Webhook creation callback
        static *code webhookCreate;

        #! Webhook deletion callback
        static *code webhookDelete;
    }

    #! Register webhook creation and deletion APIs
    /** Called by the runtime to provide webhook infrastructure

        @param create a callback for creating webhooks; takes a hash with webhook config,
               returns a hash with \c url and \c ctx keys
        @param destroy a callback for deleting webhooks; takes the hash returned by \a create
    */
    static registerWebhookApis(code create, code destroy) {
        if (webhookCreate) {
            throw "WEBHOOK-API-ERROR", "Webhook APIs are already registered";
        }
        webhookCreate = create;
        webhookDelete = destroy;
    }

    #! Check if webhook infrastructure is available
    /** @return True if webhook APIs have been registered, False otherwise
    */
    static bool hasWebhookSupport() {
        return exists webhookCreate;
    }

    #! Create a webhook and return the public URL for it
    /** @param config a hash with the following keys:
        - \c method (string): the HTTP method for the webhook (e.g., "POST")
        - \c callback (code): a callback taking the webhook payload to generate events
        - \c auth (*int): optional authentication requirement (0 = none, 1 = required)
        - \c perms (*list&lt;string&gt;): optional list of permissions when auth is required

        @return a hash with:
        - \c url (string): the public URL for the webhook
        - \c ctx (auto): context object needed for webhook deletion

        @throw WEBHOOK-API-ERROR if webhook infrastructure is not available
    */
    static hash<auto> createWebhook(hash<auto> config) {
        if (!webhookCreate) {
            throw "WEBHOOK-API-ERROR", "Cannot create a webhook; no webhook infrastructure is available";
        }
        return DataProviderWebhook::webhookCreate(config);
    }

    #! Delete a webhook
    /** @param info the hash returned by createWebhook() containing \c url and \c ctx keys

        @throw WEBHOOK-API-ERROR if webhook infrastructure is not available
    */
    static deleteWebhook(hash<auto> info) {
        if (!webhookDelete) {
            throw "WEBHOOK-API-ERROR", "Cannot delete a webhook; no webhook infrastructure is available";
        }
        DataProviderWebhook::webhookDelete(info);
    }
}

#! Describes a webhook signature to verify
/** @see @ref DataProvider::WebhookSignature "WebhookSignature"

    @since %DataProvider 3.8
*/
public hashdecl WebhookSignatureInfo {
    #! The shared secret the sender signed with
    string secret;

    #! The exact bytes that were signed
    /** This must be the payload as it arrived.  A body that has been parsed and re-serialized is not the
        same sequence of bytes -- key order, whitespace and number formatting all differ -- so a signature
        computed over it will not match, and a verifier that quietly accepted the mismatch would be no
        verifier at all.
    */
    data payload;

    #! The signatures to accept, any one of which is sufficient
    /** More than one is normal while a secret is being rotated: the sender signs with both the old and the
        new secret until every receiver has been updated.
    */
    softlist<string> signatures;

    #! The digest algorithm
    string algorithm = "sha256";

    #! How the signatures are encoded: \c "hex" or \c "base64"
    string encoding = "hex";

    #! The time the payload was signed, if the scheme carries one
    *date timestamp;

    #! How old a signed payload may be
    /** Ignored when @ref timestamp is not set.  Without this check a signature stays valid forever, so an
        attacker who captures one delivery can replay it indefinitely.
    */
    *date tolerance;
}

#! Verifies signatures on incoming webhook payloads
/** A webhook endpoint is a URL anyone on the internet can post to.  Without a signature check it will
    accept whatever it is sent, which for a payments or messaging integration means accepting forged
    events — a fabricated "payment succeeded" is enough to ship goods that were never paid for.

    Most services sign the same way, differing only in how the signature is presented, so the parts worth
    sharing are gathered here: computing the digest, comparing it without leaking timing, and rejecting a
    payload old enough to be a replay.  What each service puts in its header is parsed by its own provider.

    @par Example:
    @code{.py}
if (!WebhookSignature::verify(<WebhookSignatureInfo>{
    "secret": webhook_secret,
    "payload": sprintf("%s.%s", timestamp, raw_body),
    "signatures": signatures,
    "timestamp": signed_at,
    "tolerance": 5m,
})) {
    log(LoggerLevel::ERROR, "rejecting webhook delivery: signature verification failed");
    return;
}
    @endcode

    @since %DataProvider 3.8
*/
public class WebhookSignature {
    #! Verifies a signature
    /** @param info the signature to verify

        @return @ref True if one of the given signatures matches and the payload is not too old

        @throw WEBHOOK-SIGNATURE-ERROR the encoding is not supported
    */
    static bool verify(hash<WebhookSignatureInfo> info) {
        if (!info.secret.val() || !info.signatures) {
            return False;
        }
        if (exists info.timestamp && exists info.tolerance) {
            # a signature with no freshness check stays valid forever, so a captured delivery can be
            # replayed; the comparison is absolute in both directions, because a timestamp far in the
            # future is no more trustworthy than one far in the past
            date now = now_us();
            date age = now - info.timestamp;
            if (age < 0s) {
                age = -age;
            }
            if (age > info.tolerance) {
                return False;
            }
        }
        string expected = WebhookSignature::sign(info.secret, info.payload, info.algorithm, info.encoding);
        foreach string sig in (info.signatures) {
            if (WebhookSignature::secureCompare(expected, sig)) {
                return True;
            }
        }
        return False;
    }

    #! Returns the signature for a payload
    /** @param secret the shared secret
        @param payload the exact bytes to sign
        @param algorithm the digest algorithm
        @param encoding how to encode the digest: \c "hex" or \c "base64"

        @return the encoded signature

        @throw WEBHOOK-SIGNATURE-ERROR the encoding is not supported
    */
    static string sign(string secret, data payload, string algorithm = "sha256", string encoding = "hex") {
        binary digest = hmac(algorithm, payload, secret);
        switch (encoding) {
            case "hex":
                return digest.toHex().lwr();
            case "base64":
                return make_base64_string(digest);
        }
        throw "WEBHOOK-SIGNATURE-ERROR", sprintf("signature encoding %y is not supported; supported "
            "encodings: \"hex\", \"base64\"", encoding);
    }

    #! Compares two signatures without leaking where they first differ
    /** @param a the first value
        @param b the second value

        @return @ref True if the values are equal

        @note Comparing signatures with \c "==" returns as soon as two bytes differ, so how long the
        comparison takes reveals how much of a guess was correct, and an attacker can rebuild a valid
        signature one byte at a time.  Every byte is compared here regardless of the result.
    */
    static bool secureCompare(string a, string b) {
        binary ba = a.toBinary();
        binary bb = b.toBinary();
        # the lengths themselves are not secret, but returning early on a length mismatch would still be a
        # separate timing signal, so a mismatch is folded into the accumulator instead
        int diff = ba.size() ^ bb.size();
        int len = ba.size() < bb.size() ? ba.size() : bb.size();
        for (int i = 0; i < len; ++i) {
            diff |= ba[i] ^ bb[i];
        }
        return diff == 0;
    }

    #! Parses a comma-separated key/value signature header
    /** Several services present a signature as a list of \c "key=value" pairs, where one key carries the
        time the payload was signed and another carries the signature itself, repeated when more than one
        secret is currently valid (\c "t=1614556800,v1=5257a8...,v1=a1b2c3...").

        @param header the header value

        @return the values for each key, in the order they appeared

        @par Example:
        @code{.py}
hash<string, list<string>> parts = WebhookSignature::parseKeyValueHeader(hdr);
*string timestamp = parts.t[0];
    @endcode
    */
    static hash<string, list<string>> parseKeyValueHeader(string header) {
        hash<string, list<string>> rv = {};
        foreach string part in (header.split(",")) {
            part = trim(part);
            int pos = part.find("=");
            if (pos <= 0) {
                continue;
            }
            rv{part.substr(0, pos)} += part.substr(pos + 1);
        }
        return rv;
    }
}

} # namespace DataProvider
