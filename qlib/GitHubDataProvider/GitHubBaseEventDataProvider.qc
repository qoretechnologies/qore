# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GitHubDataProvider module definition

/*  GitHubBaseEventDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the GitHubDataProvider module
public namespace GitHubDataProvider {
#! Base class for GitHub webhook event data providers
/** Registers a repository webhook with GitHub when observers are ready, verifies the signature on every
    delivery, and notifies observers for the one event the subclass watches.

    @section githubevent_verification Signature Verification

    A webhook endpoint is a URL anyone on the internet can post to.  Without verification it accepts whatever
    it is sent — a fabricated \c "pull request merged" or \c "release published" is enough to make an
    automation deploy code that was never written.

    So this provider creates every hook with a **shared secret**, generated per subscription from
    @ref Qore::get_random_bytes() "get_random_bytes()", and drops any delivery whose
    \c X-Hub-Signature-256 header does not verify against it.  A delivery arriving before the hook has been
    created is dropped too, because there is nothing to verify it against, and there is no option that turns
    the check off.

    **This is the part of the port that is not parity.** The application this module replaces registered its
    hooks with no \c config.secret at all, so GitHub sent no signature and none could be checked: every one
    of its triggers would fire on anything posted to its endpoint by anyone who learned the URL.

    @note The signature covers the **exact bytes** GitHub sent.  A body that has been parsed into a hash and
    re-serialized is a different sequence of bytes — key order, whitespace and number formatting all
    differ — so a delivery that arrives already parsed cannot be verified and is dropped with an error
    saying so, rather than being accepted unchecked.

    @section githubevent_scope The Repository Is an Option

    A GitHub webhook is registered on one repository, so \c owner and \c repo are required options rather
    than connection settings.  They are checked in
    @ref DataProvider::DelayedObservable::observersReady() "observersReady()" rather than in the
    constructor, because the provider is constructed as a **template** when the application's children are
    enumerated — before anyone has chosen a repository — and a constructor that threw then would make the
    event unlistable.

    @section githubevent_actions Which Deliveries Are Raised

    One GitHub event covers a whole family of changes: the \c issues event fires for \c opened, \c edited,
    \c closed, \c labeled and a dozen more.  An event called "New Issue" that fired on all of them would be
    reporting something other than what it says, so a subclass naming an action in
    @ref getEventAction() raises only that one.  Set \c all_actions to receive every delivery of the
    underlying GitHub event instead, which is what the superseded application did.

    GitHub's own \c ping delivery — sent once when a hook is created — is always dropped: it is protocol
    noise, not an event.
*/
public class GitHubBaseEventDataProvider inherits AbstractDataProvider, DataProvider::DelayedObservable {
    public {
        #! The header carrying the HMAC-SHA256 signature
        const SignatureHeader = "x-hub-signature-256";

        #! The prefix GitHub writes before the signature digest
        const SignaturePrefix = "sha256=";

        #! The header carrying the GitHub event name
        const EventHeader = "x-github-event";

        #! The header carrying the unique delivery ID
        const DeliveryHeader = "x-github-delivery";

        #! GitHub's own "the hook was created" delivery, which is never an event
        const PingEvent = "ping";

        #! The number of random bytes in a generated webhook secret
        const SecretBytes = 32;

        #! Constructor options shared by every GitHub event provider
        const EventConstructorOptions = {
            "owner": <DataProviderOptionInfo>{
                "display_name": "Owner",
                "short_desc": "The user or organization that owns the repository",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The account the repository belongs to — a user or an organization",
                "required": True,
                "preselected": True,
                "ref_data": GitHubManifest::RefOwner,
                "allowed_values_creatable": True,
            },
            "repo": <DataProviderOptionInfo>{
                "display_name": "Repository",
                "short_desc": "The name of the repository to watch",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The repository name on its own, **not** `owner/name`",
                "required": True,
                "preselected": True,
                "ref_data": GitHubManifest::RefRepository,
                "allowed_values_creatable": True,
                "depends_on": ("owner",),
            },
            "all_actions": <DataProviderOptionInfo>{
                "display_name": "All Actions",
                "short_desc": "Deliver every change the underlying GitHub event covers",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "One GitHub event covers a family of changes — the `issues` event fires for "
                    "`opened`, `edited`, `closed` and more — and this event normally raises only the one "
                    "that matches its name.\n\n"
                    "Set this to deliver every one of them instead, and read the payload's `action` "
                    "property to tell them apart. This is what the application this module replaces did.",
                "default_value": False,
            },
        };
    }

    private {
        #! The REST client used for API calls
        *GitHubRestClient::GitHubRestClientIo rest;

        #! The account owning the repository being watched
        *string owner;

        #! The repository being watched
        *string repo;

        #! Whether every delivery of the underlying GitHub event is raised
        bool all_actions = False;

        #! Webhook info returned from DataProviderWebhook::createWebhook()
        *hash<auto> webhook_info;

        #! The ID of the hook created on the repository
        *int hook_id;

        #! The secret the hook was created with
        *string webhook_secret;
    }

    #! Creates the object
    /** @param rest the GitHub REST client to use for API calls
        @param options the event options; not validated here, because the provider is constructed as a
            template when the application's children are enumerated
    */
    constructor(*GitHubRestClient::GitHubRestClientIo rest, *hash<auto> options) {
        self.rest = rest;
        if (exists options.owner && options.owner.toString().val()) {
            owner = options.owner.toString();
        }
        if (exists options.repo && options.repo.toString().val()) {
            repo = options.repo.toString();
        }
        if (exists options.all_actions) {
            all_actions = parse_boolean(options.all_actions.toString());
        }
    }

    #! Stops observing and deletes the object
    destructor() {
        stopEventsIntern();
    }

    #! Returns the data provider name
    string getName() {
        return getEventId();
    }

    #! Returns the GitHub event this provider subscribes to
    /** ex: \c "issues"
    */
    abstract string getGitHubEvent();

    #! Returns the payload \c action this event raises, or @ref nothing when the event has none
    /** \c push and \c create carry no \c action property at all, so those return @ref nothing.
    */
    abstract *string getEventAction();

    #! Returns the data provider event ID
    abstract string getEventId();

    #! Returns a description of what the event means
    abstract string getEventDesc();

    #! Returns the type of the payload this event carries
    abstract AbstractDataProviderType getEventType();

    #! Returns @ref True if the delivery is one this event reports
    /** The default accepts a delivery whose \c action matches @ref getEventAction(); an event that needs a
        different rule overrides this.
    */
    private bool acceptsDelivery(hash<auto> event) {
        if (all_actions) {
            return True;
        }
        *string action = getEventAction();
        if (!action.val()) {
            return True;
        }
        return event.action == action;
    }

    #! Called when all observers have been added to the object
    /** The options are checked here rather than in the constructor: this object is built as a template when
        the application's children are enumerated, before anyone has chosen a repository.
    */
    observersReady() {
        if (!owner.val() || !repo.val()) {
            throw "GITHUB-EVENT-ERROR", sprintf("the %y event needs both the \"owner\" and \"repo\" options "
                "to name the repository to watch", getEventId());
        }
        if (!rest) {
            throw "GITHUB-EVENT-ERROR", sprintf("cannot register a webhook for %y: no REST client is "
                "available", getEventId());
        }
        webhook_info = DataProvider::DataProviderWebhook::createWebhook({
            "method": "POST",
            "callback": \handleWebhookEvent(),
        });
        try {
            registerGitHubHook(webhook_info.url);
        } catch (hash<ExceptionInfo> ex) {
            # the local endpoint is live from the moment it is created, so it must not be left behind when
            # GitHub refuses the hook; otherwise a URL nobody verifies stays reachable
            DataProvider::DataProviderWebhook::deleteWebhook(webhook_info);
            remove webhook_info;
            rethrow;
        }
        log(LoggerLevel::INFO, "%s: webhook %d created on %s/%s at %y for GitHub event %y", self.className(),
            hook_id, owner, repo, webhook_info.url, getGitHubEvent());
    }

    #! Called to stop generating events
    stopEvents() {
        stopEventsIntern();
    }

    #! Returns the event this provider raises, and the shape of what it carries
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            getEventId(): <DataProviderMessageInfo>{
                "desc": getEventDesc(),
                "type": getEventType(),
            },
        };
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{
            "name": getEventId(),
            "type": self.className(),
            "supports_observable": True,
            "constructor_options": EventConstructorOptions,
        };
    }

    #! Creates the hook on the repository
    /** The hook is created with a generated secret, so that every delivery carries a signature this
        provider can check.
    */
    private registerGitHubHook(string url) {
        webhook_secret = get_random_bytes(SecretBytes).toHex();
        on_error rethrow $1.err, sprintf("%s (while registering a GitHub webhook on %s/%s for %y)", $1.desc,
            owner, repo, getGitHubEvent());
        hash<auto> resp = rest.restPost(getHooksPath(), {
            "name": "web",
            "active": True,
            "events": (getGitHubEvent(),),
            "config": {
                "url": url,
                "content_type": "json",
                "secret": webhook_secret,
                # GitHub sends TLS certificate verification "on" by default; naming it makes the choice
                # visible rather than implicit
                "insecure_ssl": "0",
            },
        });
        *int id = resp.body.id;
        if (!exists id) {
            remove webhook_secret;
            throw "GITHUB-WEBHOOK-ERROR", sprintf("GitHub returned no hook ID when registering a webhook on "
                "%s/%s for %y; it could not be removed again", owner, repo, getGitHubEvent());
        }
        hook_id = id;
    }

    #! Returns the path of the repository's webhook collection
    private string getHooksPath() {
        return sprintf("/repos/%s/%s/hooks", owner, repo);
    }

    #! Stops the webhook and removes the hook from the repository
    private stopEventsIntern() {
        if (exists hook_id && rest) {
            try {
                rest.restDel(getHooksPath() + "/" + hook_id);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::WARN, "%s: could not delete GitHub webhook %d on %s/%s: %s",
                    self.className(), hook_id, owner, repo, get_exception_string(ex));
            }
            remove hook_id;
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
        *string delivered = getHeader(request, EventHeader);
        if (delivered == PingEvent) {
            log(LoggerLevel::DEBUG, "%s: ignoring GitHub's \"ping\" delivery for hook %y", self.className(),
                hook_id);
            return;
        }
        # a hook is created for one event, but a delivery is not trustworthy input just because it was
        # signed, and a hook can be edited in the GitHub UI to send more
        if (delivered.val() && delivered != getGitHubEvent()) {
            log(LoggerLevel::DEBUG, "%s: ignoring GitHub event %y (watching %y)", self.className(), delivered,
                getGitHubEvent());
            return;
        }
        if (!acceptsDelivery(event)) {
            log(LoggerLevel::DEBUG, "%s: ignoring a %y delivery with action %y (watching %y)",
                self.className(), getGitHubEvent(), event.action, getEventAction());
            return;
        }
        log(LoggerLevel::DEBUG, "%s: received %s delivery %y", self.className(), getGitHubEvent(),
            getHeader(request, DeliveryHeader));
        notifyObservers(getEventId(), event);
    }

    #! Verifies a delivery's signature and returns the parsed payload, or nothing if it must be dropped
    private *hash<auto> verifyAndParse(hash<auto> request) {
        if (!webhook_secret.val()) {
            log(LoggerLevel::ERROR, "%s: dropping a delivery received before the webhook was registered; it "
                "cannot be verified", self.className());
            return;
        }

        # the signature covers the exact bytes GitHub sent, so a body that has already been parsed cannot be
        # verified: re-serializing it produces different bytes
        *data body = getRawBody(request);
        if (!exists body) {
            log(LoggerLevel::ERROR, "%s: dropping a delivery whose raw body is not available; the signature "
                "covers the bytes as sent and cannot be checked against a re-serialized body",
                self.className());
            return;
        }

        *string sig = getHeader(request, SignatureHeader);
        if (!sig.val()) {
            log(LoggerLevel::ERROR, "%s: dropping a delivery with no %s header", self.className(),
                SignatureHeader);
            return;
        }
        if (sig.find(SignaturePrefix) != 0) {
            log(LoggerLevel::ERROR, "%s: dropping a delivery whose %s header is not an %s digest: %y",
                self.className(), SignatureHeader, SignaturePrefix, sig);
            return;
        }

        if (!DataProvider::WebhookSignature::verify(<DataProvider::WebhookSignatureInfo>{
            "secret": webhook_secret,
            "payload": body,
            "signatures": (sig.substr(SignaturePrefix.size()),),
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

    #! Returns a header of a delivery, if present
    private *string getHeader(hash<auto> request, string name) {
        *hash<auto> hdr = request.hdr ?? request.headers;
        foreach hash<auto> i in (hdr.pairIterator()) {
            if (i.key.lwr() == name) {
                return i.value.toString();
            }
        }
    }
}
}
