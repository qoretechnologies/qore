# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ConfluenceDataProvider module definition

/*  ConfluenceEventDataProviders.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the ConfluenceDataProvider module
public namespace ConfluenceDataProvider {
#! The minimum polling interval in seconds
public const ConfluenceMinPollInterval = 15;

#! The default polling interval in seconds
public const ConfluenceDefaultPollInterval = 60;

#! The most pages a single polling cycle will read
/** Confluence offers no "created since" filter on these collections, so a cycle reads newest-first and
    stops once it is past the cursor.  This bounds the first cycle on a site with years of content.
*/
public const ConfluenceMaxPollPages = 5;

#! Base class for Confluence polling watch data providers
/** Confluence Cloud does have webhooks, but registering one needs a Connect or Forge app installed on the
    site — an administrator action that a connection cannot perform — so these poll, exactly as the
    application this module replaces did.

    They poll better, though.  That application kept the identities it had already delivered in a bounded
    in-memory set, so a restart re-delivered whatever was still in the first page.
    @ref DataProvider::AbstractTimestampWatchDataProvider "AbstractTimestampWatchDataProvider" positions
    the feed by \c createdAt instead and keeps only the identities recorded at exactly the cursor to break
    ties, so delivery is at-least-once across restarts when the host supplies checkpoint persistence, and
    the cursor advances only once an event has been raised successfully.

    Every provider sorts the collection newest-first upstream, so the newest records arrive in the first
    page and @ref ConfluenceMaxPollPages bounds how far back a cycle reads.

    @note The watch base is inherited **first**, deliberately: both bases implement
    @ref DataProvider::DelayedObservable::observersReady() "observersReady()", %Qore resolves a method to
    the first branch that declares it, and
    @ref DataProvider::AbstractDataProvider "AbstractDataProvider"'s implementation reports the operation
    as unsupported.
*/
public class ConfluenceWatchDataProviderBase inherits DataProvider::AbstractTimestampWatchDataProvider,
        DataProvider::AbstractDataProvider {
    public {
        #! Constructor options shared by every Confluence event provider
        const WatchConstructorOptions = {
            "space_id": <DataProviderOptionInfo>{
                "display_name": "Space",
                "short_desc": "Only report content in this space",
                "type": AbstractDataProviderTypeMap."*string",
                "desc": "Only report content created in this space; the whole site when unset",
                "preselected": True,
                "ref_data": ConfluenceManifest::RefSpace,
                "allowed_values_creatable": True,
            },
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "How often to poll, in seconds",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "How often to poll Confluence, in seconds (minimum " + ConfluenceMinPollInterval
                    + ")",
                "default_value": ConfluenceDefaultPollInterval,
            },
            "since": <DataProviderOptionInfo>{
                "display_name": "Since",
                "short_desc": "Deliver records created at or after this time",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Deliver records created at or after this time.\n\n"
                    "Without it the cursor starts at the moment the subscription starts, so nothing "
                    "already on the site is delivered",
            },
        };
    }

    private {
        #! The REST client used for API calls
        *ConfluenceRestClient::ConfluenceRestClientIo rest;

        #! Report only content in this space, if set
        *string space_id;
    }

    #! Creates the object
    constructor(*ConfluenceRestClient::ConfluenceRestClientIo rest, *hash<auto> options)
            : AbstractTimestampWatchDataProvider(max(options.poll_interval
                ?? ConfluenceDefaultPollInterval, ConfluenceMinPollInterval), options.since) {
        self.rest = rest;
        if (exists options.space_id && options.space_id.toString().val()) {
            space_id = options.space_id.toString();
        }
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

    #! Returns the configuration that defines which scan this provider follows
    /** The space scopes the feed, so watching a different one — or the whole site — is a different scan
        and the previous cursor must not be resumed against it.
    */
    private *hash<auto> getWatchStreamIdentityImpl() {
        return (AbstractTimestampWatchDataProvider::getWatchStreamIdentityImpl() ?? {}) + {
            "space_id": space_id,
        };
    }

    #! A Confluence record is positioned by when it was created
    private string getTimestampField() {
        return "createdAt";
    }

    #! Fetches records from a Confluence collection, newest first
    /** @param path the request path, relative to the site
        @param params the query arguments, without paging

        @return the records read

        @note Confluence returns \c {results, _links} and offers no "created since" filter here, so this
        reads newest-first and stops at @ref ConfluenceMaxPollPages; the cursor discards whatever has
        already been delivered.
    */
    private list<hash<auto>> getRecords(string path, *hash<auto> params) {
        if (!rest) {
            throw "CONFLUENCE-EVENT-ERROR", "no REST client is available; this data provider was created "
                "without one";
        }
        int limit = ConfluenceRestClient::ConfluenceRestClientBase::MaxLimit;
        list<hash<auto>> rv = ();
        *string cursor;
        int page = 0;
        while (++page <= ConfluenceMaxPollPages) {
            if (isStopping()) {
                break;
            }
            hash<auto!> args = (params ?? {}) + {"limit": limit};
            if (space_id.val()) {
                args."space-id" = space_id;
            }
            if (cursor.val()) {
                args.cursor = cursor;
            }
            list<string> qargs = ();
            foreach hash<auto> i in (args.pairIterator()) {
                if (exists i.value) {
                    qargs += sprintf("%s=%s", i.key, encode_url(i.value.toString(), True));
                }
            }
            hash<RestClient::RestResponseInfo> resp = rest.restGet(
                ConfluenceRestClient::ConfluenceRestClientBase::ApiPrefix + path + "?" + join("&", qargs));
            auto results = resp.body.results;
            if (!exists results) {
                break;
            }
            if (results.typeCode() != NT_LIST) {
                throw "CONFLUENCE-EVENT-ERROR", sprintf("expected a list in \"results\" from %y, but got "
                    "type %y", path, results.type());
            }
            foreach auto rec in (results) {
                if (rec.typeCode() == NT_HASH) {
                    rv += rec;
                }
            }
            # "_links.next" is a relative URL carrying the cursor for the following page; its absence is
            # what says the collection is exhausted
            *string next = resp.body."_links".next;
            if (!next.val()) {
                break;
            }
            cursor = (next =~ x/[?&]cursor=([^&]+)/)[0];
            if (!cursor.val()) {
                break;
            }
            cursor = decode_url(cursor);
        }
        return rv;
    }

    #! Returns the event this provider raises, and the shape of what it carries
    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            getEventName(): <DataProviderMessageInfo>{
                "desc": getDesc() ?? getEventName(),
                "type": getEventRecordType(getEventSourceAction()),
            },
        };
    }

    #! Returns the action whose records this provider reports
    abstract private string getEventSourceAction();

    #! Returns the record type an action's listing returns
    private AbstractDataProviderType getEventRecordType(string action) {
        RestSchemaActionSet aset = ConfluenceSchema::getActionSet();
        *RestSchemaActionOperation op = aset.getOperation(action);
        if (op) {
            *AbstractDataProviderType t = op.getResponseType();
            if (t) {
                # a Confluence collection is an envelope; the records are in "results"
                *hash<string, AbstractDataField> fields = t.getFields();
                if (fields.results) {
                    *AbstractDataProviderType vt = fields.results.getType();
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
}

#! Raised for each new page
public class ConfluenceNewPageDataProvider inherits ConfluenceWatchDataProviderBase {
    public {
        #! The event name
        const EventName = "new_page";

        #! The event display name
        const DisplayName = "New Page";

        #! The event short description
        const ShortDesc = "Fires when a page is created";

        #! The event description
        const Desc = "Fires for each new page, across the site or in one space.\n\n"
            "The event carries the page record Confluence returns: its ID, title, space, author and "
            "status. A page that is edited later does not fire this event again; it reports creation "
            "only.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EventName,
            "type": "ConfluenceNewPageDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = WatchConstructorOptions;
    }

    #! Creates the object
    constructor(*ConfluenceRestClient::ConfluenceRestClientIo rest, *hash<auto> options)
            : ConfluenceWatchDataProviderBase(rest, options) {
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

    #! Returns the action whose records this provider reports
    private string getEventSourceAction() {
        return "getPages";
    }

    #! Fetches the most recently created pages
    private list<hash<auto>> fetchRecords(date since) {
        return getRecords("/pages", {"sort": "-created-date"});
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Raised for each new blog post
public class ConfluenceNewBlogPostDataProvider inherits ConfluenceWatchDataProviderBase {
    public {
        #! The event name
        const EventName = "new_blogpost";

        #! The event display name
        const DisplayName = "New Blog Post";

        #! The event short description
        const ShortDesc = "Fires when a blog post is created";

        #! The event description
        const Desc = "Fires for each new blog post, across the site or in one space.\n\n"
            "The event carries the blog post record Confluence returns: its ID, title, space, author and "
            "status.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EventName,
            "type": "ConfluenceNewBlogPostDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = WatchConstructorOptions;
    }

    #! Creates the object
    constructor(*ConfluenceRestClient::ConfluenceRestClientIo rest, *hash<auto> options)
            : ConfluenceWatchDataProviderBase(rest, options) {
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

    #! Returns the action whose records this provider reports
    private string getEventSourceAction() {
        return "getBlogPosts";
    }

    #! Fetches the most recently created blog posts
    private list<hash<auto>> fetchRecords(date since) {
        return getRecords("/blogposts", {"sort": "-created-date"});
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Raised for each new attachment
public class ConfluenceNewAttachmentDataProvider inherits ConfluenceWatchDataProviderBase {
    public {
        #! The event name
        const EventName = "new_attachment";

        #! The event display name
        const DisplayName = "New Attachment";

        #! The event short description
        const ShortDesc = "Fires when a file is attached to a page or blog post";

        #! The event description
        const Desc = "Fires for each new attachment.\n\n"
            "The event carries the attachment record Confluence returns: its ID, file name, media type, "
            "size and the content it is attached to. **List Attachments** reads the same records, which "
            "the application this module replaces could not do at all.";

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EventName,
            "type": "ConfluenceNewAttachmentDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        /** An attachment listing is not scoped by space, so the space option is not offered here.
        */
        const ConstructorOptions = WatchConstructorOptions - "space_id";
    }

    #! Creates the object
    constructor(*ConfluenceRestClient::ConfluenceRestClientIo rest, *hash<auto> options)
            : ConfluenceWatchDataProviderBase(rest, options - "space_id") {
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

    #! Returns the action whose records this provider reports
    private string getEventSourceAction() {
        return "getAttachments";
    }

    #! Fetches the most recently created attachments
    private list<hash<auto>> fetchRecords(date since) {
        return getRecords("/attachments", {"sort": "-created-date"});
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! The container holding the Confluence event providers
public class ConfluenceEventsDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": ConfluenceDataProvider::EventsChildName,
            "type": "ConfluenceEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
        };

        #! The event providers, keyed by event name
        const EventMap = {
            ConfluenceNewPageDataProvider::EventName: Class::forName("ConfluenceNewPageDataProvider"),
            ConfluenceNewBlogPostDataProvider::EventName:
                Class::forName("ConfluenceNewBlogPostDataProvider"),
            ConfluenceNewAttachmentDataProvider::EventName:
                Class::forName("ConfluenceNewAttachmentDataProvider"),
        };
    }

    private {
        #! The REST client used for API calls
        *ConfluenceRestClient::ConfluenceRestClientIo rest;
    }

    #! Creates the object
    constructor(*ConfluenceRestClient::ConfluenceRestClientIo rest) {
        self.rest = rest;
    }

    #! Returns the data provider name
    string getName() {
        return ConfluenceDataProvider::EventsChildName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Confluence event providers";
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
