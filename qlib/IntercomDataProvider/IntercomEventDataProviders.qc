# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore IntercomDataProvider module definition

/*  IntercomEventDataProviders.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the IntercomDataProvider module
public namespace IntercomDataProvider {
#! The default polling interval for an Intercom event, in seconds
public const IntercomDefaultPollInterval = 300;

#! The shortest polling interval an Intercom event accepts, in seconds
/** Intercom's rate limit is per workspace and shared with everything else the credential does, so a
    subscription that polled every few seconds would spend the workspace's budget on its own.
*/
public const IntercomMinPollInterval = 60;

#! How many records one polling page reads
public const IntercomPollPageSize = 150;

#! How many pages one polling cycle reads before stopping
public const IntercomMaxPollPages = 10;

#! The record field Intercom positions a newly created object by
public const IntercomCreatedField = "created_at";

#! Base class for Intercom polling watch data providers
/** Intercom publishes a webhook API, and webhook triggers would be better than polling - but subscribing
    to one needs a topic configured against the app in Intercom's own developer hub rather than anything a
    connection can do, and it changes delivery semantics.  So these poll, exactly as the application this
    module replaces did; moving to webhooks is a separate, reviewable decision.

    They poll better, though, in two ways.  That application asked for everything created in the last
    thirty minutes on every cycle and dropped what it had already seen from a bounded in-memory set, so a
    restart re-delivered whatever was still inside the window and a stall longer than the window lost
    records outright.  These position the feed by \c created_at through
    @ref DataProvider::AbstractTimestampWatchDataProvider "AbstractTimestampWatchDataProvider" and send
    that cursor upstream as a search filter, so a cycle reads only what is genuinely new and a restart
    resumes from the durable checkpoint.

    @note The watch base is inherited **first**, deliberately: both bases implement
    @ref DataProvider::DelayedObservable::observersReady() "observersReady()", %Qore resolves a method to
    the first branch that declares it, and
    @ref DataProvider::AbstractDataProvider "AbstractDataProvider"'s implementation reports the operation as
    unsupported.

    @section intercomevent_timestamps Seconds Since the Epoch, and No "At Or After"

    Intercom writes \c created_at as an integer count of seconds since the epoch, so the cursor is
    converted in both directions rather than compared as a date.

    Its search operators are \c "=", \c "!=", \c "IN", \c "NIN", \c "<", \c ">", \c "~", \c "!~", \c "^"
    and \c "$" - there is **no** \c ">=".  The cursor needs "at or after", because a record sharing the
    cursor's timestamp that has not been delivered yet still has to be, so the filter asks for
    \c "> (cursor - 1 second)".  That is exact rather than approximate: the values are whole seconds, so
    one less than the cursor excludes exactly the instants before it.
*/
public class IntercomWatchDataProviderBase inherits DataProvider::AbstractTimestampWatchDataProvider,
        DataProvider::AbstractDataProvider {
    public {
        #! Constructor options shared by every Intercom event provider
        const WatchConstructorOptions = {
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "How often to poll, in seconds",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "How often to poll Intercom, in seconds (minimum " + IntercomMinPollInterval
                    + ").\n\nIntercom rate-limits per workspace, and that budget is shared with "
                    "everything else the credential does, which is what the minimum protects.",
                "default_value": IntercomDefaultPollInterval,
            },
            "since": <DataProviderOptionInfo>{
                "display_name": "Since",
                "short_desc": "Deliver records created at or after this time",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Deliver records created at or after this time.\n\nWithout it the cursor starts "
                    "at the moment the subscription starts, so nothing already in the workspace is "
                    "delivered",
            },
        };
    }

    private {
        #! The REST client used for API calls
        *IntercomRestClient::IntercomRestClientIo rest;
    }

    #! Creates the object
    constructor(*IntercomRestClient::IntercomRestClientIo rest, *hash<auto> options)
            : AbstractTimestampWatchDataProvider(max(options.poll_interval ?? IntercomDefaultPollInterval,
                IntercomMinPollInterval), options.since) {
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

    #! An Intercom record is positioned by when it was created
    private string getTimestampField() {
        return IntercomCreatedField;
    }

    #! Returns the record's position in the feed, converting Intercom's epoch seconds
    /** @note This must not throw: an unusable timestamp skips one record, where an exception would abort
        the whole cycle and stall every other record with it.
    */
    private *date getWatchRecordTimestamp(hash<auto> record) {
        auto val = record{IntercomCreatedField};
        switch (val.typeCode()) {
            case NT_INT:
                return IntercomWatchDataProviderBase::fromEpochSeconds(val);
            case NT_FLOAT:
            case NT_NUMBER:
                return IntercomWatchDataProviderBase::fromEpochSeconds(val.toInt());
            case NT_DATE:
                return val.absolute() ? val : NOTHING;
        }
    }

    #! Returns the record field holding the record's stable identity
    /** Intercom really does call it \c "id" on both a contact and a conversation, so this agrees with the
        default - it is declared rather than inherited so that a reader can see it was checked.  The cursor
        needs it to break a tie between records sharing a timestamp, and Intercom's timestamps are whole
        seconds, so ties are common rather than exotic.
    */
    private string getIdField() {
        return "id";
    }

    #! Returns the UTC timestamp for a count of seconds since the epoch
    static date fromEpochSeconds(int secs) {
        return gmtime(secs, 0);
    }

    #! Reads an Intercom search collection from the given cursor
    /** @param path the search endpoint, ex: \c "/contacts/search"
        @param envelope the name of the array in the response envelope, ex: \c "data"
        @param query the search query
        @param per_page how many records to ask for in one page
        @param max_pages how many pages to read before stopping

        @return the records read
    */
    private list<hash<auto>> searchRecords(string path, string envelope, hash<auto> query,
            int per_page = IntercomPollPageSize, int max_pages = IntercomMaxPollPages) {
        if (!rest) {
            throw "INTERCOM-EVENT-ERROR", "no REST client is available; this data provider was created "
                "without one";
        }
        list<hash<auto>> rv = ();
        *string starting_after;
        int page = 0;
        while (++page <= max_pages) {
            if (isStopping()) {
                break;
            }
            hash<auto> pagination = {"per_page": per_page};
            if (starting_after.val()) {
                pagination.starting_after = starting_after;
            }
            hash<RestClient::RestResponseInfo> resp = rest.restPost(path, {
                "query": query,
                "pagination": pagination,
            });
            auto records = resp.body{envelope};
            if (records.typeCode() != NT_LIST || !records) {
                break;
            }
            foreach auto rec in (records) {
                if (rec.typeCode() == NT_HASH) {
                    rv += rec;
                }
            }
            # "pages" is absent once the collection is exhausted, which is what says there is no next page
            starting_after = resp.body.pages.next.starting_after;
            if (!starting_after.val()) {
                break;
            }
        }
        return rv;
    }

    #! Returns the filter selecting records created at or after the cursor
    /** Intercom has no \c ">=" operator, so this asks for \c "> (cursor - 1 second)"; the values are whole
        seconds, so that is exactly "at or after".
    */
    static hash<auto> getCreatedSinceFilter(date since) {
        return {
            "field": IntercomCreatedField,
            "operator": ">",
            "value": get_epoch_seconds(since) - 1,
        };
    }

    #! Returns a real record as an example of what the event carries
    /** Real data first, because a generated example tells a user much less about the shape they will
        actually receive.  Any failure - no connection, no access, an empty workspace - falls through to
        the inherited behaviour, which builds an example from the declared type: an example is a
        convenience and must not be able to stop a subscription being configured.
    */
    private auto getExampleEventDataImpl(string event_id) {
        if (rest) {
            try {
                # one record from one page: an example needs a real shape, not a real collection
                list<hash<auto>> records = searchRecords(getSearchPath(), getEventEnvelope(),
                    IntercomWatchDataProviderBase::getCreatedSinceFilter(1970-01-01Z), 1, 1);
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

    #! Returns the record type the search action this provider polls returns
    /** Taken from the action set the application ships, so that the event and the action a workflow reads
        it with cannot describe the same record differently.
    */
    private AbstractDataProviderType getEventRecordType() {
        RestSchemaActionSet aset = IntercomSchema::getActionSet();
        *RestSchemaActionOperation op = aset.getOperation(getEventSourceAction());
        if (op) {
            *AbstractDataProviderType t = op.getResponseType();
            if (t) {
                # an Intercom search answers an envelope with one array in it, named after what it holds
                *hash<string, AbstractDataField> fields = t.getFields();
                *AbstractDataField f = fields{getEventEnvelope()};
                if (f) {
                    *AbstractDataProviderType vt = f.getType();
                    if (vt && vt.getBaseTypeCode() == NT_LIST) {
                        *AbstractDataProviderType et = vt.getElementType();
                        if (et) {
                            return et;
                        }
                    }
                }
                return t;
            }
        }
        return AbstractDataProviderType::get(AutoHashType);
    }

    #! Returns the search endpoint this event polls
    abstract private string getSearchPath();

    #! Returns the name of the array in the response envelope
    abstract private string getEventEnvelope();

    #! Returns the action whose records this provider reports
    abstract private string getEventSourceAction();
}

#! Raised for each new contact
public class IntercomNewContactDataProvider inherits IntercomWatchDataProviderBase {
    public {
        #! The event name
        const EventName = "new-contact";

        #! The event display name
        const DisplayName = "New Contact";

        #! The event short description
        const ShortDesc = "Fires when a contact is created";

        #! The event description
        const Desc = "Fires for each contact created in the workspace.\n\n"
            "`role` narrows the feed to users (a contact who has signed up) or to leads (one who has "
            "not); left unset, both are reported. The event carries the contact record Intercom "
            "returns.\n\n"
            "A contact that is edited later does **not** fire this again: the feed is positioned by "
            "`created_at`.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EventName,
            "type": "IntercomNewContactDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = WatchConstructorOptions + {
            "role": <DataProviderOptionInfo>{
                "display_name": "Role",
                "short_desc": "Report only contacts with this role",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Report only contacts with this role.\n\n"
                    "A `user` has signed up and a `lead` has not. Leave this unset to report both, which "
                    "is what the application this module replaces called \"Contact\".",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "user",
                        "display_name": "User",
                        "desc": "A contact who has signed up",
                    },
                    <AllowedValueInfo>{
                        "value": "lead",
                        "display_name": "Lead",
                        "desc": "A contact who has not signed up",
                    },
                ),
            },
        };
    }

    private {
        #! The role this subscription reports, if any
        *string role;
    }

    #! Creates the object
    constructor(*IntercomRestClient::IntercomRestClientIo rest, *hash<auto> options)
            : IntercomWatchDataProviderBase(rest, options) {
        if (options.role.val()) {
            role = options.role;
        }
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

    #! Returns the search endpoint this event polls
    private string getSearchPath() {
        return "/contacts/search";
    }

    #! Returns the name of the array in the response envelope
    private string getEventEnvelope() {
        return "data";
    }

    #! Returns the action whose records this provider reports
    private string getEventSourceAction() {
        return "SearchContacts";
    }

    #! Returns the configuration that defines which scan this provider follows
    /** \c role selects **which** contacts the feed returns, so it defines the scan: resuming a cursor
        taken with one role against another would silently skip every contact created before the change.
    */
    private *hash<auto> getWatchStreamIdentityImpl() {
        return AbstractTimestampWatchDataProvider::getWatchStreamIdentityImpl() + {
            "role": role,
        };
    }

    #! Fetches the contacts created since the cursor
    private list<hash<auto>> fetchRecords(date since) {
        hash<auto> created = IntercomWatchDataProviderBase::getCreatedSinceFilter(since);
        hash<auto> query = role.val()
            ? {
                "operator": "AND",
                "value": (created, {"field": "role", "operator": "=", "value": role}),
            }
            : created;
        return searchRecords(getSearchPath(), getEventEnvelope(), query);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Raised for each new conversation
public class IntercomNewConversationDataProvider inherits IntercomWatchDataProviderBase {
    public {
        #! The event name
        const EventName = "new-conversation";

        #! The event display name
        const DisplayName = "New Conversation";

        #! The event short description
        const ShortDesc = "Fires when a conversation is created";

        #! The event description
        const Desc = "Fires for each conversation created in the workspace.\n\n"
            "The event carries the conversation record Intercom returns, which includes its source - who "
            "started it and what they said.\n\n"
            "A reply to an existing conversation does **not** fire this: the feed is positioned by "
            "`created_at`, so a conversation is reported once, when it starts.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EventName,
            "type": "IntercomNewConversationDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = WatchConstructorOptions;
    }

    #! Creates the object
    constructor(*IntercomRestClient::IntercomRestClientIo rest, *hash<auto> options)
            : IntercomWatchDataProviderBase(rest, options) {
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

    #! Returns the search endpoint this event polls
    private string getSearchPath() {
        return "/conversations/search";
    }

    #! Returns the name of the array in the response envelope
    private string getEventEnvelope() {
        return "conversations";
    }

    #! Returns the action whose records this provider reports
    private string getEventSourceAction() {
        return "searchConversations";
    }

    #! Fetches the conversations created since the cursor
    private list<hash<auto>> fetchRecords(date since) {
        return searchRecords(getSearchPath(), getEventEnvelope(),
            IntercomWatchDataProviderBase::getCreatedSinceFilter(since));
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! The container holding the Intercom event providers
public class IntercomEventsDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": IntercomDataProvider::EventsChildName,
            "type": "IntercomEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
        };

        #! The event providers, keyed by event name
        const EventMap = {
            IntercomNewContactDataProvider::EventName: Class::forName("IntercomNewContactDataProvider"),
            IntercomNewConversationDataProvider::EventName:
                Class::forName("IntercomNewConversationDataProvider"),
        };
    }

    private {
        #! The REST client used for API calls
        *IntercomRestClient::IntercomRestClientIo rest;
    }

    #! Creates the object
    constructor(*IntercomRestClient::IntercomRestClientIo rest) {
        self.rest = rest;
    }

    #! Returns the data provider name
    string getName() {
        return IntercomDataProvider::EventsChildName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Intercom polling event providers";
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
