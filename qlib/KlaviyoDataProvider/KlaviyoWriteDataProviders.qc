# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KlaviyoDataProvider module definition

/*  KlaviyoWriteDataProviders.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the KlaviyoDataProvider module
public namespace KlaviyoDataProvider {
#! Klaviyo's marker for consent that has been given
public const KlaviyoConsentSubscribed = "SUBSCRIBED";

#! Klaviyo's marker for consent that has been withdrawn
public const KlaviyoConsentUnsubscribed = "UNSUBSCRIBED";

#! Channel selection: the e-mail channel only
public const KlaviyoChannelEmail = "email";

#! Channel selection: the SMS channel only
public const KlaviyoChannelSms = "sms";

#! Channel selection: both channels
public const KlaviyoChannelBoth = "both";

#! SMS consent for marketing messages
public const KlaviyoSmsMarketing = "marketing";

#! SMS consent for transactional messages
public const KlaviyoSmsTransactional = "transactional";

#! SMS consent for both kinds of message
public const KlaviyoSmsBoth = "both";

#! Records that something happened, against a metric and a profile
/** This action is **hand-written rather than generated**.  Klaviyo declares an event body as

    @code{.py}
{"data": {"type": "event", "attributes": {
    "metric":  {"data": {"type": "metric",  "attributes": {"name": …}}},
    "profile": {"data": {"type": "profile", "id": …, "attributes": {"email": …}}},
    "properties": {…}, "time": …, "value": …, "unique_id": …
}}}
    @endcode

    - two whole JSON:API envelopes nested inside the event's own attributes, three levels below the body
    root.  @ref RestSchemaActions::RestSchemaActionInfo::request_unwrap "request_unwrap" lifts the options
    out of **one** envelope; it cannot flatten two more that sit beside each other inside it.

    The application being replaced published a metric name, a profile ID and an e-mail address, and built
    the envelopes itself, and so does this.

    @section klaviyoevent_metric The Metric Is Named, Not Selected

    \c metric is the metric's **name**, not its ID, because Klaviyo defines a metric the first time an event
    names one: recording a \c "Placed Order" event against an account that has never had one creates the
    metric.  The dropdown therefore offers the names already in use and still accepts a value typed in,
    which is what defines a new metric.

    @section klaviyoevent_profile Identifying the Profile

    An event has to say who it happened to, by profile ID or by e-mail address - Klaviyo creates a profile
    from the e-mail address if it does not know it yet.  Supplying neither is rejected by Klaviyo rather
    than here, because it also accepts other identifiers this action does not publish, and failing early on
    a rule the API does not have would be wrong.
*/
public class KlaviyoCreateEventDataProvider inherits KlaviyoHandWrittenDataProviderBase {
    public {
        #! The action ID
        /** The ID the superseded TypeScript application registered, which existing workflows call.
        */
        const ActionName = "create_event";

        #! The request path
        const RequestPath = "/api/events";

        #! The action display name
        const DisplayName = "Create Event";

        #! The action short description
        const ShortDesc = "Creates a new event";

        #! The action description
        const Desc = "Records that something happened, against a metric and a profile.\n\n"
            "`metric` is the **name** of the kind of event - \"Placed Order\", \"Viewed Product\" - and "
            "Klaviyo defines a metric the first time an event names one, so a name that does not exist yet "
            "is how a new metric is created.\n\n"
            "The event has to say who it happened to: give `profile` for someone already known, or "
            "`email`, which Klaviyo creates a profile from if it has to. `customId` makes the event "
            "idempotent - Klaviyo drops a second event with the same one - and `time` records an event "
            "that happened earlier than now.\n\n"
            "This action is hand-written rather than generated, because Klaviyo's own description nests a "
            "metric envelope and a profile envelope inside the event's attributes, which cannot be "
            "published as one flat option shape.";

        #! The request type
        static HashDataType RequestType = KlaviyoCreateEventDataProvider::buildRequestType();
    }

    #! Creates the object from the arguments
    constructor(RestSchemaActionSet action_set, *KlaviyoRestClient::KlaviyoRestClientIo rest)
            : KlaviyoHandWrittenDataProviderBase(action_set, rest) {
    }

    #! Returns the request type
    static HashDataType buildRequestType() {
        HashDataType rv(AutoHashType, ActionName + ": request");
        rv.addQoreFields({
            "metric": {
                "name": "metric",
                "display_name": "Metric",
                "short_desc": "The name of the kind of event",
                "desc": "The name of the kind of event, ex: `Placed Order`.\n\n"
                    "Klaviyo defines a metric the first time an event names one, so a name that is not in "
                    "the list yet creates a new metric.",
                "type": StringType,
                "attr": {
                    "required": True,
                    "preselected": True,
                    "ref_data": KlaviyoManifest::RefMetricName,
                    "supports_custom_values": True,
                },
            },
            "profile": {
                "name": "profile",
                "display_name": "Profile",
                "short_desc": "The profile the event happened to",
                "desc": "The profile the event happened to.\n\nGive this or `email`.",
                "type": StringOrNothingType,
                "attr": {
                    "preselected": True,
                    "ref_data": KlaviyoManifest::RefProfile,
                    "supports_custom_values": True,
                    "required_groups": ("identifier",),
                },
            },
            "email": {
                "name": "email",
                "display_name": "E-mail",
                "short_desc": "The e-mail address the event happened to",
                "desc": "The e-mail address the event happened to.\n\nKlaviyo creates a profile from it if "
                    "it does not know it yet, which is how an event can be recorded for somebody who has "
                    "not been seen before.",
                "type": StringOrNothingType,
                "attr": {
                    "preselected": True,
                    "required_groups": ("identifier",),
                },
            },
            "time": {
                "name": "time",
                "display_name": "Time",
                "short_desc": "When the event happened",
                "desc": "When the event happened.\n\nKlaviyo uses the current time when this is unset, so "
                    "set it when recording something after the fact.",
                "type": DateOrNothingType,
                "attr": {"preselected": True},
            },
            "value": {
                "name": "value",
                "display_name": "Value",
                "short_desc": "The monetary value of the event",
                "desc": "The monetary value of the event, in the account's currency.\n\nThis is what "
                    "Klaviyo's revenue reporting adds up.",
                "type": SoftNumberOrNothingType,
            },
            "customId": {
                "name": "customId",
                "display_name": "Custom ID",
                "short_desc": "Your own unique identifier for this event",
                "desc": "Your own unique identifier for this event.\n\nKlaviyo drops a second event "
                    "carrying an ID it has already seen, which makes a retry safe.",
                "type": StringOrNothingType,
            },
            "customProperties": {
                "name": "customProperties",
                "display_name": "Custom Properties",
                "short_desc": "Your own properties to record with the event",
                "desc": "Your own properties to record with the event, as name/value pairs",
                "type": AutoHashOrNothingType,
            },
        });
        return rv;
    }

    #! Returns the action ID
    string getName() {
        return ActionName;
    }

    #! Returns the display name of the action
    string getDisplayName() {
        return DisplayName;
    }

    #! Returns the short description of the action
    *string getShortDesc() {
        return ShortDesc;
    }

    #! Returns the description of the action
    *string getDesc() {
        return Desc;
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Records the event
    /** Klaviyo answers \c "202 Accepted" with no body, so there is no output contract to publish.
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        KlaviyoRestClient::KlaviyoRestClientIo client = getRestEx();
        hash<auto> args = req;
        string metric = getRequiredOption(args, "metric", "the kind of event being recorded");

        # "auto!" because the attribute values are of several types
        hash<auto!> attributes = {
            # Klaviyo requires "properties", so an event recording nothing beyond the fact that it happened
            # still sends an empty object
            "properties": args.customProperties ?? {},
            "metric": {
                "data": {
                    "type": "metric",
                    "attributes": {"name": metric},
                },
            },
            "profile": {
                "data": {
                    "type": "profile",
                }
                + (args.profile.val() ? {"id": args.profile} : {})
                + {"attributes": args.email.val() ? {"email": args.email} : {}},
            },
        };
        if (exists args.time) {
            attributes.time = KlaviyoWriteUtil::toIso8601(args.time);
        }
        if (exists args.value) {
            attributes.value = args.value;
        }
        if (args.customId.val()) {
            attributes.unique_id = args.customId;
        }

        on_error rethrow $1.err, sprintf("%s (while recording a Klaviyo %y event)", $1.desc, metric);
        client.restPost(RequestPath, {
            "data": {
                "type": "event",
                "attributes": attributes,
            },
        });
    }

    #! Returns the reference data this action's options can offer
    *hash<string, bool> getSupportedReferenceData() {
        return {
            KlaviyoManifest::RefMetricName: True,
            KlaviyoManifest::RefProfile: True,
        };
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{
            "name": ActionName,
            "type": "KlaviyoCreateEventDataProvider",
            "supports_request": True,
            "supports_children": False,
        };
    }
}

#! Helpers shared by the two subscription actions
public class KlaviyoWriteUtil {
    #! Returns an ISO-8601 UTC timestamp for a %Qore date
    /** @note a \c "Z" in a date format string writes the UTC *offset*, not a literal \c "Z", so the value
        is converted to UTC and the zone designator appended explicitly.
    */
    static string toIso8601(date d) {
        TimeZone utc(0);
        return utc.date(d).format("YYYY-MM-DDTHH:mm:SS") + "Z";
    }

    #! Returns the SMS consent block for the given selection and consent marker
    /** @param sms_type \c "marketing", \c "transactional" or \c "both"
        @param consent the consent marker to record

        @return the \c sms member of a subscriptions object
    */
    static hash<auto> getSmsConsent(*string sms_type, string consent) {
        hash<auto> rv = {};
        string want = sms_type ?? KlaviyoSmsMarketing;
        if (want == KlaviyoSmsMarketing || want == KlaviyoSmsBoth) {
            rv.marketing = {"consent": consent};
        }
        if (want == KlaviyoSmsTransactional || want == KlaviyoSmsBoth) {
            rv.transactional = {"consent": consent};
        }
        return rv;
    }

    #! Returns the field describing which kinds of SMS message a consent change covers
    static hash<auto> getSmsTypeField(string desc) {
        return {
            "name": "smsSubscriptionType",
            "display_name": "SMS Subscription Type",
            "short_desc": "Which kinds of SMS message this covers",
            "desc": desc,
            "type": StringOrNothingType,
            "attr": {
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": KlaviyoSmsMarketing,
                        "display_name": "Marketing",
                        "desc": "Promotional messages",
                    },
                    <AllowedValueInfo>{
                        "value": KlaviyoSmsTransactional,
                        "display_name": "Transactional",
                        "desc": "Order updates, delivery notices and the like",
                    },
                    <AllowedValueInfo>{
                        "value": KlaviyoSmsBoth,
                        "display_name": "Marketing & Transactional",
                        "desc": "Both kinds of message",
                    },
                ),
                "depends_on": ("phoneNumber",),
            },
        };
    }
}

#! Records marketing consent for a profile
/** This action is **hand-written rather than generated**.  Klaviyo has no "subscribe this profile"
    endpoint: consent is recorded by creating a **bulk subscription job**, whose body carries a list of
    profiles, each with its own per-channel consent envelope, inside the job's attributes - four levels
    below the body root, with an array in the middle.

    The application being replaced published one profile's identity and a channel selection, and built the
    job itself, and so does this.  A workflow that needs to subscribe many profiles at once can reach the
    endpoint through \c make-api-call.

    @section klaviyosubscribe_consent What Consent Means Here

    Recording consent through the API is only lawful where the profile actually gave it - Klaviyo's own
    terms require that the caller has a record of it.  This action states the consent; it does not collect
    it.

    \c list matters for more than membership: subscribing **through a list** is what makes Klaviyo apply
    that list's double opt-in process, if it has one.  Without a list the consent is recorded against the
    profile directly and no confirmation is sent.

    @section klaviyosubscribe_sms Marketing and Transactional Are Separate
    \c smsSubscriptionType decides which kinds of SMS the consent covers.  Klaviyo treats marketing and
    transactional consent as different permissions, and a profile can hold one without the other; the
    default is marketing, which is what the superseded application sent.
*/
public class KlaviyoSubscribeProfileDataProvider inherits KlaviyoHandWrittenDataProviderBase {
    public {
        #! The action ID
        const ActionName = "subscribe_profile";

        #! The request path
        const RequestPath = "/api/profile-subscription-bulk-create-jobs";

        #! The action display name
        const DisplayName = "Subscribe Profile";

        #! The action short description
        const ShortDesc = "Subscribes a profile to marketing channels";

        #! The action description
        const Desc = "Records marketing consent for a profile, on the e-mail channel, the SMS channel or "
            "both.\n\n"
            "Identify the profile by `profileId`, by `email` or by `phoneNumber`. `list` subscribes the "
            "profile **through a list**, which is what makes Klaviyo apply that list's double opt-in "
            "process; without it the consent is recorded directly and no confirmation is sent.\n\n"
            "Only record consent that was actually given - Klaviyo's terms require the caller to hold a "
            "record of it.\n\n"
            "This action is hand-written rather than generated, because Klaviyo records consent by "
            "creating a bulk subscription job whose body nests a list of per-channel consent envelopes "
            "four levels down.";

        #! The request type
        static HashDataType RequestType = KlaviyoSubscribeProfileDataProvider::buildRequestType();
    }

    #! Creates the object from the arguments
    constructor(RestSchemaActionSet action_set, *KlaviyoRestClient::KlaviyoRestClientIo rest)
            : KlaviyoHandWrittenDataProviderBase(action_set, rest) {
    }

    #! Returns the request type
    static HashDataType buildRequestType() {
        HashDataType rv(AutoHashType, ActionName + ": request");
        rv.addQoreFields({
            "consentToSubscribeToChannel": {
                "name": "consentToSubscribeToChannel",
                "display_name": "Channel",
                "short_desc": "The channel or channels the consent covers",
                "desc": "The channel or channels the consent covers",
                "type": StringType,
                "attr": {
                    "required": True,
                    "preselected": True,
                    "has_dependents": True,
                    "allowed_values": (
                        <AllowedValueInfo>{
                            "value": KlaviyoChannelEmail,
                            "display_name": "E-mail",
                            "desc": "E-mail marketing consent",
                        },
                        <AllowedValueInfo>{
                            "value": KlaviyoChannelSms,
                            "display_name": "SMS",
                            "desc": "SMS consent",
                        },
                        <AllowedValueInfo>{
                            "value": KlaviyoChannelBoth,
                            "display_name": "E-mail & SMS",
                            "desc": "Consent on both channels",
                        },
                    ),
                },
            },
            "profileId": {
                "name": "profileId",
                "display_name": "Profile",
                "short_desc": "The profile giving consent, if it is already known",
                "desc": "The profile giving consent.\n\nGive this, `email` or `phoneNumber`.",
                "type": StringOrNothingType,
                "attr": {
                    "preselected": True,
                    "ref_data": KlaviyoManifest::RefProfile,
                    "supports_custom_values": True,
                    "required_groups": ("subscription",),
                },
            },
            "email": {
                "name": "email",
                "display_name": "E-mail",
                "short_desc": "The e-mail address consenting",
                "desc": "The e-mail address consenting.\n\nRequired for the e-mail channel unless "
                    "`profileId` names a profile that already carries one.",
                "type": StringOrNothingType,
                "attr": {
                    "preselected": True,
                    "required_groups": ("subscription",),
                },
            },
            "phoneNumber": {
                "name": "phoneNumber",
                "display_name": "Phone Number",
                "short_desc": "The phone number consenting, in E.164 format",
                "desc": "The phone number consenting, in E.164 format, ex: `+15005550006`.\n\nRequired for "
                    "the SMS channel unless `profileId` names a profile that already carries one.",
                "type": StringOrNothingType,
                "attr": {
                    "has_dependents": True,
                    "required_groups": ("subscription",),
                },
            },
            "smsSubscriptionType": KlaviyoWriteUtil::getSmsTypeField(
                "Which kinds of SMS message the consent covers.\n\nKlaviyo treats marketing and "
                "transactional consent as different permissions; the default is marketing."),
            "list": {
                "name": "list",
                "display_name": "List",
                "short_desc": "Subscribe through this list",
                "desc": "Subscribe the profile **through this list**, which adds it to the list and "
                    "applies the list's double opt-in process if it has one.\n\nWithout it the consent is "
                    "recorded against the profile directly and no confirmation is sent.",
                "type": StringOrNothingType,
                "attr": {
                    "ref_data": KlaviyoManifest::RefList,
                    "supports_custom_values": True,
                },
            },
        });
        return rv;
    }

    #! Returns the action ID
    string getName() {
        return ActionName;
    }

    #! Returns the display name of the action
    string getDisplayName() {
        return DisplayName;
    }

    #! Returns the short description of the action
    *string getShortDesc() {
        return ShortDesc;
    }

    #! Returns the description of the action
    *string getDesc() {
        return Desc;
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Records the consent
    /** Klaviyo answers \c "202 Accepted" with no body, so there is no output contract to publish.
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        KlaviyoRestClient::KlaviyoRestClientIo client = getRestEx();
        hash<auto> args = req;
        string channel = getRequiredOption(args, "consentToSubscribeToChannel",
            "the channel or channels the consent covers");

        hash<auto> subscriptions = {};
        if (channel == KlaviyoChannelEmail || channel == KlaviyoChannelBoth) {
            subscriptions.email = {"marketing": {"consent": KlaviyoConsentSubscribed}};
        }
        if (channel == KlaviyoChannelSms || channel == KlaviyoChannelBoth) {
            subscriptions.sms = KlaviyoWriteUtil::getSmsConsent(args.smsSubscriptionType,
                KlaviyoConsentSubscribed);
        }
        if (!subscriptions) {
            throw "KLAVIYO-ACTION-ERROR", sprintf("the %y option is %y, which names no channel; it must be "
                "%y, %y or %y", "consentToSubscribeToChannel", channel, KlaviyoChannelEmail,
                KlaviyoChannelSms, KlaviyoChannelBoth);
        }

        hash<auto!> attributes = {"subscriptions": subscriptions};
        if (args.email.val()) {
            attributes.email = args.email;
        }
        if (args.phoneNumber.val()) {
            attributes.phone_number = args.phoneNumber;
        }

        hash<auto!> profile = {"type": "profile"};
        if (args.profileId.val()) {
            profile.id = args.profileId;
        }
        profile.attributes = attributes;

        hash<auto!> data = {
            "type": "profile-subscription-bulk-create-job",
            "attributes": {
                "profiles": {"data": (profile,)},
            },
        };
        if (args.list.val()) {
            data.relationships = {
                "list": {
                    "data": {
                        "type": "list",
                        "id": args.list,
                    },
                },
            };
        }

        on_error rethrow $1.err, sprintf("%s (while recording Klaviyo %s consent)", $1.desc, channel);
        client.restPost(RequestPath, {"data": data});
    }

    #! Returns the reference data this action's options can offer
    *hash<string, bool> getSupportedReferenceData() {
        return {
            KlaviyoManifest::RefProfile: True,
            KlaviyoManifest::RefList: True,
        };
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{
            "name": ActionName,
            "type": "KlaviyoSubscribeProfileDataProvider",
            "supports_request": True,
            "supports_children": False,
        };
    }
}

#! Withdraws marketing consent for a profile
/** The mirror of @ref KlaviyoDataProvider::KlaviyoSubscribeProfileDataProvider
    "KlaviyoSubscribeProfileDataProvider", and hand-written for the same reason: Klaviyo withdraws consent
    by creating a bulk **delete** job whose body has the same shape.

    Which channels are withdrawn follows from what is supplied, exactly as it did in the application being
    replaced: an \c email withdraws e-mail consent, a \c phoneNumber withdraws SMS consent, and both
    withdraw both.  There is no channel option, because there is nothing to say beyond the identifier.

    @note Withdrawing consent is not the same as removing the profile from a list.  Use **Remove Profile
    from List** for membership; this records that the person no longer wants to be marketed to, which
    Klaviyo honours across every list.
*/
public class KlaviyoUnsubscribeProfileDataProvider inherits KlaviyoHandWrittenDataProviderBase {
    public {
        #! The action ID
        const ActionName = "unsubscribe_profile";

        #! The request path
        const RequestPath = "/api/profile-subscription-bulk-delete-jobs";

        #! The action display name
        const DisplayName = "Unsubscribe Profile";

        #! The action short description
        const ShortDesc = "Unsubscribes a profile from marketing channels";

        #! The action description
        const Desc = "Withdraws marketing consent for a profile.\n\n"
            "Which channels are withdrawn follows from what is supplied: an `email` withdraws e-mail "
            "consent, a `phoneNumber` withdraws SMS consent, and both withdraw both. At least one is "
            "required.\n\n"
            "This withdraws **consent**, which Klaviyo honours across every list; use **Remove Profile "
            "from List** to change membership instead.\n\n"
            "This action is hand-written rather than generated, because Klaviyo withdraws consent by "
            "creating a bulk job whose body nests a list of per-channel consent envelopes four levels "
            "down.";

        #! The request type
        static HashDataType RequestType = KlaviyoUnsubscribeProfileDataProvider::buildRequestType();
    }

    #! Creates the object from the arguments
    constructor(RestSchemaActionSet action_set, *KlaviyoRestClient::KlaviyoRestClientIo rest)
            : KlaviyoHandWrittenDataProviderBase(action_set, rest) {
    }

    #! Returns the request type
    static HashDataType buildRequestType() {
        HashDataType rv(AutoHashOrNothingType, ActionName + ": request");
        rv.addQoreFields({
            "email": {
                "name": "email",
                "display_name": "E-mail",
                "short_desc": "The e-mail address to unsubscribe",
                "desc": "The e-mail address to unsubscribe.\n\nSupplying it withdraws e-mail marketing "
                    "consent.",
                "type": StringOrNothingType,
                "attr": {
                    "preselected": True,
                    "required_groups": ("unsubscribe",),
                },
            },
            "phoneNumber": {
                "name": "phoneNumber",
                "display_name": "Phone Number",
                "short_desc": "The phone number to unsubscribe, in E.164 format",
                "desc": "The phone number to unsubscribe, in E.164 format.\n\nSupplying it withdraws SMS "
                    "consent.",
                "type": StringOrNothingType,
                "attr": {
                    "preselected": True,
                    "has_dependents": True,
                    "required_groups": ("unsubscribe",),
                },
            },
            "smsSubscriptionType": KlaviyoWriteUtil::getSmsTypeField(
                "Which kinds of SMS message to withdraw consent for.\n\nOnly meaningful together with "
                "`phoneNumber`; the default is marketing."),
        });
        return rv;
    }

    #! Returns the action ID
    string getName() {
        return ActionName;
    }

    #! Returns the display name of the action
    string getDisplayName() {
        return DisplayName;
    }

    #! Returns the short description of the action
    *string getShortDesc() {
        return ShortDesc;
    }

    #! Returns the description of the action
    *string getDesc() {
        return Desc;
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Withdraws the consent
    /** Klaviyo answers \c "202 Accepted" with no body, so there is no output contract to publish.
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        KlaviyoRestClient::KlaviyoRestClientIo client = getRestEx();
        hash<auto> args = req ?? {};

        hash<auto!> attributes = {};
        hash<auto> subscriptions = {};
        if (args.email.val()) {
            attributes.email = args.email;
            subscriptions.email = {"marketing": {"consent": KlaviyoConsentUnsubscribed}};
        }
        if (args.phoneNumber.val()) {
            attributes.phone_number = args.phoneNumber;
            subscriptions.sms = KlaviyoWriteUtil::getSmsConsent(args.smsSubscriptionType,
                KlaviyoConsentUnsubscribed);
        }
        if (!subscriptions) {
            throw "KLAVIYO-ACTION-ERROR", "either the \"email\" or the \"phoneNumber\" option is required; "
                "they say which channel's consent to withdraw";
        }
        attributes.subscriptions = subscriptions;

        on_error rethrow $1.err, sprintf("%s (while withdrawing Klaviyo consent)", $1.desc);
        client.restPost(RequestPath, {
            "data": {
                "type": "profile-subscription-bulk-delete-job",
                "attributes": {
                    "profiles": {
                        "data": (
                            {
                                "type": "profile",
                                "attributes": attributes,
                            },
                        ),
                    },
                },
            },
        });
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{
            "name": ActionName,
            "type": "KlaviyoUnsubscribeProfileDataProvider",
            "supports_request": True,
            "supports_children": False,
        };
    }
}
}
