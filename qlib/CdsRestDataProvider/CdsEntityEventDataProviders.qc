# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CdsRestDataProvider module definition

/*  CdsEntityEventDataProviders.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the CdsRestDataProvider module
public namespace CdsRestDataProvider {
#! The default polling interval for a Dynamics event, in seconds
public const CdsDefaultPollInterval = 300;

#! The shortest polling interval a Dynamics event accepts, in seconds
/** Dataverse charges API calls against a per-user service-protection limit, and a subscription that
    polled every few seconds would spend the allowance on empty answers.
*/
public const CdsMinPollInterval = 60;

#! How many pages one polling cycle reads before it stops and waits for the next
/** A bound is needed because the first cycle of a subscription with an early @c since can match the
    whole table. Stopping mid-history loses nothing: the cursor has advanced over everything already
    dispatched, so the next cycle resumes exactly where this one stopped.
*/
public const CdsMaxPollPages = 10;

#! How many records one page of a polling cycle asks for
public const CdsPollPageSize = 500;

#! The watch condition that fires once per record, when it is created
public const CdsWatchCreated = "created";

#! The watch condition that fires when a record is created and again on every change
public const CdsWatchUpdated = "updated";

#! The Dataverse column that positions a record in the feed, per watch condition
public const CdsConditionTimestampField = {
    CdsWatchCreated: "createdon",
    CdsWatchUpdated: "modifiedon",
};

#! The named Dynamics 365 entity events, keyed by action ID
/** The action IDs are a product commitment and are reproduced here exactly as the application this
    module replaces registered them. They read <tt>new-or-updated-*</tt> while that application's
    files were named <tt>new-*</tt>; the IDs are the truth, and they are also the truth about the
    semantics, because these fire on change as well as on creation — see @ref cdsevent_condition.

    Each entry carries the entity set the event polls, the entity's logical name, and the entity's
    primary key, which Dataverse names after the table rather than calling it @c "id".

    This map is the single source for both the child providers and the registered actions, so an
    event cannot be offered by one and missing from the other.
*/
public const CdsEntityEvents = {
    "new-or-updated-account": {
        "entity_set": "accounts",
        "logical_name": "account",
        "id_field": "accountid",
        "display_name": "New or Updated Account",
        "short_desc": "Fires when an account is created or changed",
        "desc": "Fires for each account created in the organisation, and, when the condition is "
            "`updated`, again on every subsequent change.",
    },
    "new-or-updated-case": {
        "entity_set": "incidents",
        "logical_name": "incident",
        "id_field": "incidentid",
        "display_name": "New or Updated Case",
        "short_desc": "Fires when a case is created or changed",
        "desc": "Fires for each case created in the organisation, and, when the condition is "
            "`updated`, again on every subsequent change.\n\nDataverse calls this table `incident`.",
    },
    "new-or-updated-contact": {
        "entity_set": "contacts",
        "logical_name": "contact",
        "id_field": "contactid",
        "display_name": "New or Updated Contact",
        "short_desc": "Fires when a contact is created or changed",
        "desc": "Fires for each contact created in the organisation, and, when the condition is "
            "`updated`, again on every subsequent change.",
    },
    "new-or-updated-invoice": {
        "entity_set": "invoices",
        "logical_name": "invoice",
        "id_field": "invoiceid",
        "display_name": "New or Updated Invoice",
        "short_desc": "Fires when an invoice is created or changed",
        "desc": "Fires for each invoice created in the organisation, and, when the condition is "
            "`updated`, again on every subsequent change.",
    },
    "new-or-updated-lead": {
        "entity_set": "leads",
        "logical_name": "lead",
        "id_field": "leadid",
        "display_name": "New or Updated Lead",
        "short_desc": "Fires when a lead is created or changed",
        "desc": "Fires for each lead created in the organisation, and, when the condition is "
            "`updated`, again on every subsequent change.",
    },
    "new-or-updated-opportunity": {
        "entity_set": "opportunities",
        "logical_name": "opportunity",
        "id_field": "opportunityid",
        "display_name": "New or Updated Opportunity",
        "short_desc": "Fires when an opportunity is created or changed",
        "desc": "Fires for each opportunity created in the organisation, and, when the condition is "
            "`updated`, again on every subsequent change.",
    },
    "new-or-updated-order": {
        "entity_set": "salesorders",
        "logical_name": "salesorder",
        "id_field": "salesorderid",
        "display_name": "New or Updated Order",
        "short_desc": "Fires when an order is created or changed",
        "desc": "Fires for each order created in the organisation, and, when the condition is "
            "`updated`, again on every subsequent change.\n\nDataverse calls this table "
            "`salesorder`.",
    },
};

#! Base class for Dynamics 365 polling watch data providers
/** Dataverse can push through Service Hooks, but registering one needs a subscription configured
    outside anything a connection can do, so these poll, as the application this module replaces did.

    They poll correctly, though, which that application did not. It asked for
    @code $orderby=<column> asc&$top=50 @endcode with no filter, which is not a moving window at all:
    it is the oldest fifty rows of the table, forever. On any organisation with more than fifty
    records of a type, a newly created or changed row is never inside the page and the trigger never
    fires. These position the feed by @c createdon or @c modifiedon through
    @ref DataProvider::AbstractTimestampWatchDataProvider "AbstractTimestampWatchDataProvider" and
    send that cursor upstream as an OData @c $filter, so a cycle reads only what has changed since
    the last one and the window moves.

    @note The watch base is inherited **first**, deliberately: both bases implement
    @ref DataProvider::DelayedObservable::observersReady() "observersReady()", %Qore resolves a method
    to the first branch that declares it, and
    @ref DataProvider::AbstractDataProvider "AbstractDataProvider"'s implementation reports the
    operation as unsupported.

    @section cdsevent_condition Created Versus Updated

    Every entity event takes a @c condition option, which is the one thing the replaced application
    got right and the reason its action IDs read <tt>new-or-updated-*</tt> rather than
    <tt>new-*</tt>: a single event covers both, chosen when the subscription is created.

    The two conditions are the same mechanism pointed at a different column. @c created positions by
    @c createdon, which never changes, so a record fires exactly once. @c updated positions by
    @c modifiedon, so a record fires when it is created and again on every subsequent change.

    @section cdsevent_idfield Dataverse Names Its Keys After Its Tables

    The cursor needs a stable identity to break a tie between records sharing a timestamp, and its
    default field name is @c "id", which no Dataverse table has: the primary key of @c account is
    @c accountid, of @c incident is @c incidentid, and so on. Every provider here therefore declares
    its key. A missing declaration is not an error at build time and not an error at run time — it is
    an empty feed, which is why it is called out.

    @since CdsRestDataProvider 2.2
*/
public class CdsEntityWatchDataProviderBase inherits DataProvider::AbstractTimestampWatchDataProvider,
        DataProvider::AbstractDataProvider {
    public {
        #! Constructor options shared by every Dynamics entity event
        const WatchConstructorOptions = {
            "condition": <DataProviderOptionInfo>{
                "display_name": "Condition",
                "short_desc": "Whether to fire on creation only, or on every change",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "Whether to fire once per record when it is created (`created`, positioned "
                    "by `createdon`), or when a record is created and again on every subsequent "
                    "change (`updated`, positioned by `modifiedon`).",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": CdsWatchCreated,
                        "display_name": "Created",
                        "desc": "Fire once per record, when it is created",
                    },
                    <AllowedValueInfo>{
                        "value": CdsWatchUpdated,
                        "display_name": "Updated",
                        "desc": "Fire when a record is created and again on every change",
                    },
                ),
                "default_value": CdsWatchCreated,
            },
            "poll_interval": <DataProviderOptionInfo>{
                "display_name": "Poll Interval",
                "short_desc": "How often to poll, in seconds",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "How often to poll Dynamics 365, in seconds (minimum " + CdsMinPollInterval
                    + ").\n\nDataverse charges every call against a per-user service-protection "
                    "limit, which is what the minimum protects.",
                "default_value": CdsDefaultPollInterval,
            },
            "since": <DataProviderOptionInfo>{
                "display_name": "Since",
                "short_desc": "Deliver records positioned at or after this time",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Deliver records positioned at or after this time.\n\nWithout it the cursor "
                    "starts at the moment the subscription starts, so nothing already in the "
                    "organisation is delivered",
            },
        };
    }

    private {
        #! The REST client used for API calls
        *CdsRestClient::CdsRestClient rest;

        #! The watch condition; selects the column the feed is positioned by
        string condition;
    }

    #! Creates the object
    constructor(*CdsRestClient::CdsRestClient rest, *hash<auto> options)
            : AbstractTimestampWatchDataProvider(max(options.poll_interval ?? CdsDefaultPollInterval,
                CdsMinPollInterval), options.since) {
        self.rest = rest;
        condition = options.condition ?? CdsWatchCreated;
        if (!exists CdsConditionTimestampField{condition}) {
            throw "CDS-EVENT-ERROR", sprintf("condition %y is unknown; expecting one of %y", condition,
                keys CdsConditionTimestampField);
        }
    }

    #! Stops the polling thread
    destructor() {
        stopEventsIntern();
    }

    #! Cancels an in-flight REST request during shutdown
    private cancelPollOperation() {
        if (rest) {
            rest.disconnect();
        }
    }

    #! Returns the Dataverse column this event positions a record by
    private string getTimestampField() {
        return CdsConditionTimestampField{condition};
    }

    #! Fetches every record positioned at or after the given timestamp
    /** The filter is what makes this a moving window rather than a fixed one, and it is why
        ascending order is correct here where it was the defect in the replaced application: the
        cursor supplies the lower bound, so the oldest unseen record is the one to dispatch first.
    */
    private list<hash<auto>> fetchRecords(date since) {
        string field = getTimestampField();
        string uri = sprintf("%s?$filter=%s ge %s&$orderby=%s asc", getEntitySetName(), field,
            CdsEntityWatchDataProviderBase::formatODataTimestamp(since), field);

        # Dataverse pages with a Prefer header and @odata.nextLink; $top would cap the result
        # instead of paging it, and would suppress the next link entirely
        hash<auto> hdr = {
            "Prefer": sprintf("odata.maxpagesize=%d", CdsPollPageSize),
        };

        list<hash<auto>> rv = ();
        *string next;
        int pages = 0;

        while (True) {
            hash<auto> info;
            hash<auto> resp;
            try {
                resp = rest.get(next ?? uri, NOTHING, \info, hdr);
            } catch (hash<ExceptionInfo> ex) {
                # ensure that any error response body is included in the exception
                rethrow ex.err, ex.desc, ex.arg + info{"request-uri", "response-code", "response-body"};
            }

            foreach auto rec in (resp.body.value ?? ()) {
                if (rec.typeCode() == NT_HASH) {
                    rv += rec;
                }
            }

            next = resp.body."@odata.nextLink";
            if (!next.val() || ++pages >= CdsMaxPollPages) {
                break;
            }
        }

        return rv;
    }

    #! Renders a timestamp as the literal an OData datetime comparison expects
    /** @c Edm.DateTimeOffset literals are unquoted and carry an explicit zone; this normalises to
        UTC so the comparison cannot depend on the zone the subscription happens to run in.
    */
    static string formatODataTimestamp(date d) {
        TimeZone utc(0);
        return utc.date(d).format("YYYY-MM-DDTHH:mm:SS") + "Z";
    }

    #! Returns a real record from the watched table as an example of what the event carries
    /** Real data first, because a generated example of an OData entity is a hash of type-default
        values and tells a user nothing about the table they picked. Any failure — no connection, no
        access to the table, an empty table — falls through to the inherited behaviour, which builds
        an example from the declared type: an example is a convenience and must not be able to stop
        a subscription being configured.
    */
    private auto getExampleEventDataImpl(string event_id) {
        if (rest) {
            try {
                string field = getTimestampField();
                hash<auto> info;
                # the newest row, which is the one most likely to be populated
                hash<auto> resp = rest.get(sprintf("%s?$orderby=%s desc&$top=1", getEntitySetName(),
                    field), NOTHING, \info);
                *list<auto> rows = resp.body.value;
                if (rows && rows[0].typeCode() == NT_HASH) {
                    return rows[0];
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

    #! Returns the record type of the entity this event watches
    /** Taken from the entity's own description, so that the event and the table a workflow reads it
        with cannot describe the same record differently. Falls back to an untyped hash when the
        description cannot be read, because an event that cannot describe its payload is still
        better than a subscription that will not start.
    */
    private AbstractDataProviderType getEventRecordType() {
        if (!rest) {
            return AbstractDataProviderType::get(AutoHashType);
        }
        try {
            *hash<auto> meta = getEntityMetadata();
            if (meta) {
                ODataEntityDataProvider entity(rest, getEntityLogicalName(), meta);
                # getRecordType() answers with the record's **fields**; an event describes its payload with
                # a type, so the fields are assembled into one
                *hash<string, AbstractDataField> fields = entity.getRecordType();
                if (fields) {
                    HashDataType rv(AutoHashType, getEntityLogicalName());
                    map rv.addField($1), fields.iterator();
                    return rv;
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # the payload shape is a convenience; failing to read it must not stop the subscription
        }
        return AbstractDataProviderType::get(AutoHashType);
    }

    #! Returns the entity's description from the service document, if it can be read
    private *hash<auto> getEntityMetadata() {
        hash<auto> info;
        auto schema = rest.get("$metadata?annotations=true", NOTHING, \info).body."edmx:Edmx"
            ."edmx:DataServices".Schema;
        string logical = getEntityLogicalName();
        foreach auto et in (schema.EntityType) {
            if (et."^attributes^".Name == logical) {
                return et;
            }
        }
    }

    #! Returns the entity set this event polls, ex: @c "accounts"
    abstract private string getEntitySetName();

    #! Returns the entity's logical name, ex: @c "account"
    abstract private string getEntityLogicalName();
}

#! A Dynamics 365 entity event that watches a fixed entity
/** One implementation serves every named event: an entity event differs from another only in which
    table it polls, what its primary key is called, and how it presents itself. That is also what
    makes this better than the application it replaces, which hardcoded seven tables and bolted on a
    custom-entity special case — see @ref CdsCustomEntityWatchDataProvider, which is this same class
    with the table supplied at subscription time.
*/
public class CdsEntityWatchDataProvider inherits CdsEntityWatchDataProviderBase {
    private {
        #! The event raised for each record; also the provider's name
        string event_name;

        #! The entity descriptor, as held in @ref CdsEntityEvents
        hash<auto> info;
    }

    #! Creates the object
    /** @param rest the REST client connection
        @param options the subscription options
        @param event_name the event to raise for each record, which is also this provider's name
        @param info the entity descriptor, as held in @ref CdsEntityEvents
    */
    constructor(*CdsRestClient::CdsRestClient rest, *hash<auto> options, string event_name,
            hash<auto> info) : CdsEntityWatchDataProviderBase(rest, options) {
        self.event_name = event_name;
        self.info = info;
    }

    #! Returns the data provider name
    string getName() {
        return event_name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return info.desc;
    }

    #! Returns the entity set this event polls
    private string getEntitySetName() {
        return info.entity_set;
    }

    #! Returns the entity's logical name
    private string getEntityLogicalName() {
        return info.logical_name;
    }

    #! Returns the record field holding the record's stable identity
    /** Dataverse names the primary key after the table, never @c "id", so this is never the
        inherited default.
    */
    private string getIdField() {
        return info.id_field;
    }

    #! Returns the event name
    private string getEventName() {
        return event_name;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return <DataProviderInfo>{
            "name": event_name,
            "type": "CdsEntityWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }
}

#! A Dynamics 365 entity event that watches an entity named when the subscription is created
/** The generic provider with the table supplied as an option instead of fixed by the registration.

    @section cdscustomevent_names Logical Names Are Not Entity Set Names

    The option is the entity's **logical** name, which is what the Dataverse user interface shows and
    what the application this module replaces asked for — but that application then used it directly
    as the OData path segment and appended @c "id" to it for the primary key. Neither is right:
    @c account is addressed as @c accounts and keyed by @c accountid, and a custom table's set name
    is whatever the publisher chose, not the logical name with an @c s on the end.

    Both are therefore resolved from the organisation's own entity definition rather than derived
    from the option's spelling.
*/
public class CdsCustomEntityWatchDataProvider inherits CdsEntityWatchDataProviderBase {
    public {
        #! The event name
        const EventName = "new-custom-entity";

        #! The event display name
        const DisplayName = "New or Updated Custom Entity";

        #! The event short description
        const ShortDesc = "Fires for each record created or changed in any Dataverse table";

        #! The event description
        const Desc = "Fires for each record created or changed in a Dataverse table named when the "
            "subscription is created.\n\nThe table is given by its logical name, ex: `account`; its "
            "entity set and primary key are read from the organisation's entity definition, so a "
            "custom table whose set name is not simply its logical name pluralised works "
            "unchanged.";

        #! Constructor options
        const ConstructorOptions = WatchConstructorOptions + {
            "entity": <DataProviderOptionInfo>{
                "display_name": "Entity",
                "short_desc": "The logical name of the table to watch",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The logical name of the Dataverse table to watch, ex: `account`.\n\nThis "
                    "is the singular logical name, not the entity set name; the entity set and the "
                    "primary key are resolved from the organisation's entity definition.",
                "required": True,
                "example_value": "account",
            },
        };

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EventName,
            "type": "CdsCustomEntityWatchDataProvider",
            "supports_observable": True,
            "constructor_options": ConstructorOptions,
        };
    }

    private {
        #! The entity's logical name, as given
        *string logical_name;

        #! The entity set resolved from the entity definition
        *string entity_set;

        #! The primary key column resolved from the entity definition
        *string id_field;

        #! Serialises the resolution, which is done once and cached
        Qore::Thread::Mutex lck();
    }

    #! Creates the object
    constructor(*CdsRestClient::CdsRestClient rest, *hash<auto> options)
            : CdsEntityWatchDataProviderBase(rest, options) {
        logical_name = options.entity;
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

    #! Returns the entity's logical name
    private string getEntityLogicalName() {
        return logical_name ?? "";
    }

    #! Returns the entity set this event polls, resolved from the entity definition
    private string getEntitySetName() {
        resolveEntity();
        return entity_set;
    }

    #! Returns the entity's primary key column, resolved from the entity definition
    private string getIdField() {
        resolveEntity();
        return id_field;
    }

    #! Reads the entity set name and primary key from the organisation's entity definition
    private resolveEntity() {
        if (entity_set && id_field) {
            return;
        }

        lck.lock();
        on_exit lck.unlock();

        # check again in the lock
        if (entity_set && id_field) {
            return;
        }

        if (!logical_name.val()) {
            throw "CDS-EVENT-ERROR", "no entity was given for the custom entity event";
        }
        if (!rest) {
            throw "CDS-EVENT-ERROR", "no connection is available to resolve entity " + logical_name;
        }

        hash<auto> info;
        hash<auto> resp;
        string uri = sprintf("EntityDefinitions(LogicalName='%s')?$select=EntitySetName,PrimaryIdAttribute",
            logical_name);
        try {
            resp = rest.get(uri, NOTHING, \info);
        } catch (hash<ExceptionInfo> ex) {
            rethrow ex.err, ex.desc, ex.arg + info{"request-uri", "response-code", "response-body"};
        }

        if (!resp.body.EntitySetName.val()) {
            throw "CDS-EVENT-ERROR", sprintf("entity %y has no entity set name in this organisation; "
                "it cannot be watched", logical_name);
        }
        if (!resp.body.PrimaryIdAttribute.val()) {
            throw "CDS-EVENT-ERROR", sprintf("entity %y declares no primary key; the feed cannot be "
                "positioned without one", logical_name);
        }

        entity_set = resp.body.EntitySetName;
        id_field = resp.body.PrimaryIdAttribute;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! The container holding the Dynamics 365 entity event providers
public class CdsEventsDataProvider inherits DataProvider::AbstractDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": CdsRestDataProvider::EventsChildName,
            "type": "CdsEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
        };

        #! Provider summary info
        static hash<DataProviderSummaryInfo> ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(
            ProviderInfo{AbstractDataProvider::DataProviderSummaryInfoKeys}
        );
    }

    private {
        #! The REST client used for API calls
        *CdsRestClient::CdsRestClient rest;
    }

    #! Creates the object
    constructor(*CdsRestClient::CdsRestClient rest) {
        self.rest = rest;
    }

    #! Returns the data provider name
    string getName() {
        return CdsRestDataProvider::EventsChildName;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Microsoft Dynamics 365 polling event providers";
    }

    #! Returns the event names
    private *list<string> getChildProviderNamesImpl() {
        return (keys CdsEntityEvents) + (CdsCustomEntityWatchDataProvider::EventName,);
    }

    #! Returns the given event provider or @ref nothing if the event is unknown
    /** The child returned here is a **template**: it carries the event's option declarations, and
        the values are supplied when a subscription is created.
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        if (name == CdsCustomEntityWatchDataProvider::EventName) {
            return new CdsCustomEntityWatchDataProvider(rest);
        }
        *hash<auto> info = CdsEntityEvents{name};
        if (!info) {
            return;
        }
        return new CdsEntityWatchDataProvider(rest, NOTHING, name, info);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
