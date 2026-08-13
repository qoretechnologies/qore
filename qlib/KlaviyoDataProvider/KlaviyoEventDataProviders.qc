# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KlaviyoDataProvider module definition

/*  KlaviyoEventDataProviders.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
#! The default polling interval for a Klaviyo event, in seconds
public const KlaviyoDefaultPollInterval = 300;

#! The shortest polling interval a Klaviyo event accepts, in seconds
/** Klaviyo's rate limits are per account and shared with everything else the credential does, so a
    subscription that polled every few seconds would spend the account's budget on its own.
*/
public const KlaviyoMinPollInterval = 60;

#! How many records one polling page reads
/** Klaviyo caps \c page[size] per endpoint, and 100 is the largest value the endpoints these events poll
    accept.
*/
public const KlaviyoPollPageSize = 100;

#! How many pages one polling cycle reads before stopping
public const KlaviyoMaxPollPages = 10;

#! Base class for Klaviyo polling watch data providers
/** Klaviyo publishes a Webhooks API, and webhook triggers would be better than polling - but a webhook is
    configured against the account in Klaviyo's own console rather than by anything a connection can do, and
    it changes delivery semantics.  So these poll, exactly as the application this module replaces did;
    moving to webhooks is a separate, reviewable decision.

    They poll better, though.  That application asked for the newest page on every cycle, in descending
    order, and dropped what it had already seen from a bounded in-memory set - so a restart re-delivered
    whatever was still in that page, and a burst larger than one page lost records outright.  These position
    the feed by its timestamp field through
    @ref DataProvider::AbstractTimestampWatchDataProvider "AbstractTimestampWatchDataProvider" and send that
    cursor upstream as a \c greater-than filter in **ascending** order, so a cycle reads only what is
    genuinely new, pages through all of it, and a restart resumes from the durable checkpoint.

    @note The watch base is inherited **first**, deliberately: both bases implement
    @ref DataProvider::DelayedObservable::observersReady() "observersReady()", %Qore resolves a method to
    the first branch that declares it, and
    @ref DataProvider::AbstractDataProvider "AbstractDataProvider"'s implementation reports the operation as
    unsupported.

    @section klaviyoevent_shape What a Record Looks Like

    Klaviyo is JSON:API, so a record arrives as \c "{id, type, attributes: {…}}".  The event delivers the
    identity and the attributes together, which is the shape the superseded application delivered and the
    same shape this module's listing actions return - so a workflow reading \c email off an event and off a
    **List Profiles** result reads the same field name in both.
*/
public class KlaviyoWatchDataProviderBase inherits DataProvider::AbstractTimestampWatchDataProvider,
        DataProvider::AbstractDataProvider {
    public {
        #! Constructor options shared by every Klaviyo event provider
        const WatchConstructorOptions = {
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "How often to poll, in seconds",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "How often to poll Klaviyo, in seconds (minimum " + KlaviyoMinPollInterval
                    + ").\n\nKlaviyo rate-limits per account, and that budget is shared with everything "
                    "else the credential does, which is what the minimum protects.",
                "default_value": KlaviyoDefaultPollInterval,
            },
            "since": <DataProviderOptionInfo>{
                "display_name": "Since",
                "short_desc": "Deliver records from this time onwards",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Deliver records from this time onwards.\n\nWithout it the cursor starts at the "
                    "moment the subscription starts, so nothing already in the account is delivered",
            },
        };
    }

    private {
        #! The REST client used for API calls
        *KlaviyoRestClient::KlaviyoRestClientIo rest;
    }

    #! Creates the object
    constructor(*KlaviyoRestClient::KlaviyoRestClientIo rest, *hash<auto> options)
            : AbstractTimestampWatchDataProvider(max(options.poll_interval ?? KlaviyoDefaultPollInterval,
                KlaviyoMinPollInterval), options.since) {
        self.rest = rest;
    }

    #! Stops the polling thread
    destructor() {
        stopEventsIntern();
    }

    #! Cancels an in-flight REST request during shutdown
    private cancelPollOperation() {
        if (rest) {
            rest.close();
        }
    }

    #! Returns the record's position in the feed
    /** @note This must not throw: an unusable timestamp skips one record, where an exception would abort
        the whole cycle and stall every other record with it.
    */
    private *date getWatchRecordTimestamp(hash<auto> record) {
        auto val = record{getTimestampField()};
        switch (val.typeCode()) {
            case NT_DATE:
                return val.absolute() ? val : NOTHING;
            case NT_STRING: {
                # Klaviyo writes every timestamp as an ISO-8601 string; a value it cannot parse skips the
                # record rather than stopping the cycle
                try {
                    date d = date(val);
                    return d.absolute() ? d : NOTHING;
                } catch (hash<ExceptionInfo> ex) {
                    return;
                }
            }
        }
    }

    #! Returns the record field holding the record's stable identity
    /** Klaviyo really does call it \c "id" on a profile, a list membership and an event alike, so this
        agrees with the default - it is declared rather than inherited so that a reader can see it was
        checked.  The cursor needs it to break a tie between records sharing a timestamp.
    */
    private string getIdField() {
        return "id";
    }

    #! Reads a Klaviyo collection from the given cursor, in ascending order
    /** @param path the collection endpoint
        @param filter the filter predicates to send, if any
        @param page_size how many records to ask for in one page
        @param max_pages how many pages to read before stopping

        @return the records read, each with its attributes merged into its identity

        Paging follows Klaviyo's own \c links.next, which is a whole URL: the cursor is taken out of it
        rather than the URL being used directly, because the client addresses the API with its own base URL
        and authentication.
    */
    private list<hash<auto>> readRecords(string path, *string filter,
            int page_size = KlaviyoPollPageSize, int max_pages = KlaviyoMaxPollPages) {
        if (!rest) {
            throw "KLAVIYO-EVENT-ERROR", "no REST client is available; this data provider was created "
                "without one";
        }
        list<hash<auto>> rv = ();
        *string cursor;
        int page = 0;
        while (++page <= max_pages) {
            if (isStopping()) {
                break;
            }
            hash<auto!> query = {"sort": getTimestampField()};
            if (filter.val()) {
                query.filter = filter;
            }
            if (supportsPageSize()) {
                query{"page[size]"} = page_size;
            }
            if (cursor.val()) {
                query{"page[cursor]"} = cursor;
            }
            hash<RestClient::RestResponseInfo> resp = rest.restGet(path + "?"
                + KlaviyoWatchDataProviderBase::buildQueryString(query));
            auto records = resp.body.data;
            if (records.typeCode() != NT_LIST || !records) {
                break;
            }
            foreach auto rec in (records) {
                if (rec.typeCode() == NT_HASH) {
                    rv += KlaviyoWatchDataProviderBase::flattenRecord(rec);
                }
            }
            # "links.next" is absent once the collection is exhausted, which is what says there is no
            # next page
            cursor = KlaviyoWatchDataProviderBase::getCursorFromLink(resp.body.links.next);
            if (!cursor.val()) {
                break;
            }
        }
        return rv;
    }

    #! Returns a JSON:API resource with its attributes merged into its identity
    static hash<auto> flattenRecord(hash<auto> rec) {
        # "auto!" because the attributes need not share a type with the identity beside them
        hash<auto!> rv = rec - "attributes";
        if (rec.attributes.typeCode() == NT_HASH) {
            map rv{$1.key} = $1.value, rec.attributes.pairIterator();
        }
        return rv;
    }

    #! Returns the cursor carried by one of Klaviyo's paging links
    /** Klaviyo answers a whole URL rather than a bare cursor, so the cursor is taken out of its query
        string; a link that carries none means there is no next page.
    */
    static *string getCursorFromLink(auto link) {
        if (link.typeCode() != NT_STRING) {
            return;
        }
        *list<*string> m = (link =~ x/[?&]page(?:%5B|\[)cursor(?:%5D|\])=([^&]+)/i);
        if (!m || !m[0].val()) {
            return;
        }
        return decode_url(m[0]);
    }

    #! Returns a URL query string for the given arguments
    static string buildQueryString(hash<auto> query) {
        list<string> parts = ();
        foreach hash<auto> i in (query.pairIterator()) {
            parts += sprintf("%s=%s", encode_url(i.key, True), encode_url(i.value.toString(), True));
        }
        return parts.join("&");
    }

    #! Returns the filter selecting records positioned after the cursor
    /** Klaviyo's date operators are \c greater-than, \c greater-or-equal, \c less-than, \c less-or-equal
        and \c equals, so this asks for \c greater-or-equal and lets the cursor's own identity tie-break
        drop the record it already delivered.  Asking for \c greater-than would lose a record sharing the
        cursor's timestamp that had not been delivered yet.
    */
    private string getSinceFilter(date since) {
        return sprintf("greater-or-equal(%s,%s)", getTimestampField(),
            KlaviyoWriteUtil::toIso8601(since));
    }

    #! Returns @ref True if the endpoint this event polls declares a page size
    /** \c "GET /api/metrics" declares only a cursor; every endpoint these events poll declares both, so
        this is @ref True by default and exists so that a future event on such an endpoint can say so.
    */
    private bool supportsPageSize() {
        return True;
    }

    #! Returns a real record as an example of what the event carries
    /** Real data first, because a generated example tells a user much less about the shape they will
        actually receive.  Any failure - no connection, no access, an empty account - falls through to the
        inherited behaviour, which builds an example from the declared type: an example is a convenience
        and must not be able to stop a subscription being configured.
    */
    private auto getExampleEventDataImpl(string event_id) {
        if (rest) {
            try {
                # one record from one page: an example needs a real shape, not a real collection
                list<hash<auto>> records = readRecords(getPollPath(), getPollFilter(), 1, 1);
                if (records) {
                    return records[0];
                }
            } catch (hash<ExceptionInfo> ex) {
                # fall through to the generated example
            }
        }
        return AbstractDataProvider::getExampleEventDataImpl(event_id);
    }

    #! Returns the event this provider raises, and the shape of what it carries
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            getEventName(): <DataProviderMessageInfo>{
                "desc": getDesc() ?? getEventName(),
                "type": getEventRecordType(),
            },
        };
    }

    #! Returns the record type the listing action this provider polls returns
    /** Taken from the action set the application ships, so that the event and the action a workflow reads
        it with cannot describe the same record differently - including the JSON:API merge, which both
        apply.
    */
    private AbstractDataProviderType getEventRecordType() {
        RestSchemaActionSet aset = KlaviyoSchema::getActionSet();
        *RestSchemaActionOperation op = aset.getOperation(getEventSourceAction());
        if (op) {
            *AbstractDataProviderType t = op.getResponseType();
            if (t) {
                *hash<string, AbstractDataField> fields = t.getFields();
                *AbstractDataField f = fields.data;
                if (f) {
                    *AbstractDataProviderType vt = f.getType();
                    if (vt && vt.getBaseTypeCode() == NT_LIST) {
                        if (*AbstractDataProviderType et = vt.getElementType()) {
                            return et;
                        }
                    }
                }
            }
        }
        return AbstractDataProviderType::get(AutoHashType);
    }

    #! Fetches the records positioned after the cursor
    private list<hash<auto>> fetchRecords(date since) {
        *string base = getPollFilter();
        string since_filter = getSinceFilter(since);
        return readRecords(getPollPath(), base.val() ? base + "," + since_filter : since_filter);
    }

    #! Returns the collection endpoint this event polls
    abstract private string getPollPath();

    #! Returns any filter predicates this event always sends, beside the cursor
    private *string getPollFilter() {
    }

    #! Returns the action whose records this provider reports
    abstract private string getEventSourceAction();

    #! Returns the record field this feed is positioned by
    abstract private string getTimestampField();
}

#! Raised for each new profile
public class KlaviyoNewProfileDataProvider inherits KlaviyoWatchDataProviderBase {
    public {
        #! The event name
        /** The ID the superseded TypeScript application registered, which existing workflows subscribe to.
        */
        const EventName = "new_profile";

        #! The event display name
        const DisplayName = "New Profile";

        #! The event short description
        const ShortDesc = "Triggers when a new profile is created";

        #! The event description
        const Desc = "Fires for each profile created in the account.\n\n"
            "The event carries the profile with its attributes beside its identity, which is the same "
            "shape **Get Profile** and **List Profiles** return.\n\n"
            "A profile that is edited later does **not** fire this again: the feed is positioned by "
            "`created`.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EventName,
            "type": "KlaviyoNewProfileDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object
    constructor(*KlaviyoRestClient::KlaviyoRestClientIo rest, *hash<auto> options)
            : KlaviyoWatchDataProviderBase(rest, options) {
    }

    #! Returns the data provider name
    string getName() {
        return EventName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return Desc;
    }

    #! Returns the event name
    private string getEventName() {
        return EventName;
    }

    #! Returns the collection endpoint this event polls
    private string getPollPath() {
        return "/api/profiles";
    }

    #! Returns the action whose records this provider reports
    private string getEventSourceAction() {
        return "list_profiles";
    }

    #! Returns the record field this feed is positioned by
    private string getTimestampField() {
        return "created";
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Raised for each event recorded against a metric
public class KlaviyoNewEventDataProvider inherits KlaviyoWatchDataProviderBase {
    public {
        #! The event name
        const EventName = "new_event";

        #! The event display name
        const DisplayName = "New Event";

        #! The event short description
        const ShortDesc = "Triggers when a new event occurs";

        #! The event description
        const Desc = "Fires for each event recorded against the chosen metric.\n\n"
            "`metric` is required and names the metric by **ID** - a feed of every event in the account "
            "would be unusably large, and a workflow reacting to \"Placed Order\" does not want \"Viewed "
            "Product\".\n\n"
            "The event carries the event record with its attributes beside its identity. The feed is "
            "positioned by `datetime`, which is when the event happened rather than when Klaviyo received "
            "it.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EventName,
            "type": "KlaviyoNewEventDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = WatchConstructorOptions + {
            "metric": <DataProviderOptionInfo>{
                "display_name": "Metric",
                "short_desc": "The metric whose events to report",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The metric whose events to report, by ID",
                "required": True,
                "ref_data": KlaviyoManifest::RefMetricId,
                "allowed_values_creatable": True,
            },
        };
    }

    private {
        #! The metric this subscription reports
        string metric;
    }

    #! Creates the object
    constructor(*KlaviyoRestClient::KlaviyoRestClientIo rest, *hash<auto> options)
            : KlaviyoWatchDataProviderBase(rest, options) {
        metric = options.metric ?? "";
    }

    #! Returns the data provider name
    string getName() {
        return EventName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return Desc;
    }

    #! Returns the event name
    private string getEventName() {
        return EventName;
    }

    #! Returns the collection endpoint this event polls
    private string getPollPath() {
        return "/api/events";
    }

    #! Returns the filter narrowing the feed to the chosen metric
    private *string getPollFilter() {
        if (!metric.val()) {
            throw "KLAVIYO-EVENT-ERROR", sprintf("the %y option is required; it names the metric whose "
                "events to report", "metric");
        }
        return sprintf("equals(metric_id,%s)", make_json(metric));
    }

    #! Returns the action whose records this provider reports
    private string getEventSourceAction() {
        return "list_events";
    }

    #! Returns the record field this feed is positioned by
    private string getTimestampField() {
        return "datetime";
    }

    #! Returns the configuration that defines which scan this provider follows
    /** \c metric selects **which** events the feed returns, so it defines the scan: resuming a cursor taken
        against one metric under another would silently skip everything recorded before the change.
    */
    private *hash<auto> getWatchStreamIdentityImpl() {
        return AbstractTimestampWatchDataProvider::getWatchStreamIdentityImpl() + {
            "metric": metric,
        };
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Base class for the two events reporting a profile joining a group
/** A list and a segment are both groups of profiles, both answer the same membership collection, and both
    position it by \c joined_group_at.  The only difference is which endpoint is polled and what the group
    is called, so the two events differ by that and nothing else.
*/
public class KlaviyoGroupMembershipDataProviderBase inherits KlaviyoWatchDataProviderBase {
    private {
        #! The group this subscription watches
        string group;
    }

    #! Creates the object
    constructor(*KlaviyoRestClient::KlaviyoRestClientIo rest, *hash<auto> options)
            : KlaviyoWatchDataProviderBase(rest, options) {
        group = options{getGroupOption()} ?? "";
    }

    #! Returns the collection endpoint this event polls
    private string getPollPath() {
        if (!group.val()) {
            throw "KLAVIYO-EVENT-ERROR", sprintf("the %y option is required; it names the %s whose new "
                "members to report", getGroupOption(), getGroupOption());
        }
        # escaped so that an identifier carrying a reserved character survives as one path segment
        return sprintf(getPathTemplate(), encode_url(group, True));
    }

    #! Returns the record field this feed is positioned by
    /** Klaviyo positions a membership by when the profile joined the group, not by when the profile was
        created - which is what makes this "added to" rather than "created in".
    */
    private string getTimestampField() {
        return "joined_group_at";
    }

    #! Returns the configuration that defines which scan this provider follows
    private *hash<auto> getWatchStreamIdentityImpl() {
        return AbstractTimestampWatchDataProvider::getWatchStreamIdentityImpl() + {
            getGroupOption(): group,
        };
    }

    #! Returns the name of the option naming the group
    abstract private string getGroupOption();

    #! Returns the URI path template, with one \c "%s" for the group's ID
    abstract private string getPathTemplate();
}

#! Raised for each profile added to a list
public class KlaviyoNewListProfileDataProvider inherits KlaviyoGroupMembershipDataProviderBase {
    public {
        #! The event name
        /** The ID the superseded TypeScript application registered.  Its **file** was called
            \c profile-added-to-list; the registered ID is this, and it is the ID existing workflows
            subscribe to.
        */
        const EventName = "new_list_profile";

        #! The event display name
        const DisplayName = "Profile Added to List";

        #! The event short description
        const ShortDesc = "Triggers when a profile is added to a list";

        #! The event description
        const Desc = "Fires for each profile added to the chosen list.\n\n"
            "The feed is positioned by `joined_group_at`, so this fires when the profile **joins the "
            "list** rather than when the profile itself was created - a profile that has existed for a "
            "year fires this the moment it is added.\n\n"
            "The event carries the profile with its attributes beside its identity.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EventName,
            "type": "KlaviyoNewListProfileDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = WatchConstructorOptions + {
            "list": <DataProviderOptionInfo>{
                "display_name": "List",
                "short_desc": "The list whose new members to report",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The list whose new members to report",
                "required": True,
                "ref_data": KlaviyoManifest::RefList,
                "allowed_values_creatable": True,
            },
        };
    }

    #! Creates the object
    constructor(*KlaviyoRestClient::KlaviyoRestClientIo rest, *hash<auto> options)
            : KlaviyoGroupMembershipDataProviderBase(rest, options) {
    }

    #! Returns the data provider name
    string getName() {
        return EventName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return Desc;
    }

    #! Returns the event name
    private string getEventName() {
        return EventName;
    }

    #! Returns the name of the option naming the group
    private string getGroupOption() {
        return "list";
    }

    #! Returns the URI path template
    private string getPathTemplate() {
        return "/api/lists/%s/profiles";
    }

    #! Returns the action whose records this provider reports
    private string getEventSourceAction() {
        return "list_profiles";
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Raised for each profile added to a segment
public class KlaviyoNewSegmentProfileDataProvider inherits KlaviyoGroupMembershipDataProviderBase {
    public {
        #! The event name
        /** The ID the superseded TypeScript application registered.  Its **file** was called
            \c profile-added-to-segment; the registered ID is this.
        */
        const EventName = "new_segment_profile";

        #! The event display name
        const DisplayName = "Profile Added to Segment";

        #! The event short description
        const ShortDesc = "Triggers when a profile is added to a segment";

        #! The event description
        const Desc = "Fires for each profile that enters the chosen segment.\n\n"
            "A segment is a rule Klaviyo evaluates continuously, so a profile enters one by **coming to "
            "match it** - editing a profile, or an event it records, can be what does that. The feed is "
            "positioned by `joined_group_at`.\n\n"
            "The event carries the profile with its attributes beside its identity.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EventName,
            "type": "KlaviyoNewSegmentProfileDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = WatchConstructorOptions + {
            "segment": <DataProviderOptionInfo>{
                "display_name": "Segment",
                "short_desc": "The segment whose new members to report",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The segment whose new members to report",
                "required": True,
                "ref_data": KlaviyoManifest::RefSegment,
                "allowed_values_creatable": True,
            },
        };
    }

    #! Creates the object
    constructor(*KlaviyoRestClient::KlaviyoRestClientIo rest, *hash<auto> options)
            : KlaviyoGroupMembershipDataProviderBase(rest, options) {
    }

    #! Returns the data provider name
    string getName() {
        return EventName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return Desc;
    }

    #! Returns the event name
    private string getEventName() {
        return EventName;
    }

    #! Returns the name of the option naming the group
    private string getGroupOption() {
        return "segment";
    }

    #! Returns the URI path template
    private string getPathTemplate() {
        return "/api/segments/%s/profiles";
    }

    #! Returns the action whose records this provider reports
    private string getEventSourceAction() {
        return "list_profiles";
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! The container holding this application's polling event providers
public class KlaviyoEventsDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": KlaviyoDataProvider::EventsChildName,
            "type": "KlaviyoEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
        };

        #! The event providers, keyed by event name
        const EventMap = {
            KlaviyoNewProfileDataProvider::EventName: Class::forName("KlaviyoNewProfileDataProvider"),
            KlaviyoNewEventDataProvider::EventName: Class::forName("KlaviyoNewEventDataProvider"),
            KlaviyoNewListProfileDataProvider::EventName:
                Class::forName("KlaviyoNewListProfileDataProvider"),
            KlaviyoNewSegmentProfileDataProvider::EventName:
                Class::forName("KlaviyoNewSegmentProfileDataProvider"),
        };
    }

    private {
        #! The REST client used for API calls
        *KlaviyoRestClient::KlaviyoRestClientIo rest;
    }

    #! Creates the object
    constructor(*KlaviyoRestClient::KlaviyoRestClientIo rest) {
        self.rest = rest;
    }

    #! Returns the data provider name
    string getName() {
        return KlaviyoDataProvider::EventsChildName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Klaviyo polling event providers";
    }

    #! Returns the event names
    private *list<string> getChildProviderNamesImpl() {
        return keys EventMap;
    }

    #! Returns the given event provider or @ref nothing if the event is unknown
    /** The child returned here is a **template**: it carries the event's option declarations, and the
        values are supplied when a subscription is created.
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = EventMap{name};
        if (!cls) {
            return;
        }
        return cast<AbstractDataProvider>(cls.newObject(rest));
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
