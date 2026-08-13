# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KlaviyoDataProvider module definition

/*  KlaviyoRecordTables.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
#! The Klaviyo record-based tables
/** Klaviyo's collections are JSON:API resource collections, and every one of them is filtered with the same
    \c filter query argument.  Six of the seven tables therefore declare
    @ref DataProvider::DataProviderInfo::supports_native_search "supports_native_search": a whole search
    expression is rendered into that argument and Klaviyo does the searching.

    @section klaviyorecord_filter The Filter Syntax

    Klaviyo's filter is a **prefix call** rather than an infix comparison - \c {equals(name,"Newsletter")},
    <tt>greater-than(created,"2026-01-01T00:00:00Z")</tt> - and predicates are joined by a bare comma meaning
    **and**.  There is **no disjunction and no negation at all**, which is why
    @ref KlaviyoRecordTables::Filter "Filter" leaves \c or_op and \c not_op unset: a search containing an
    \c OR or a \c NOT is evaluated here rather than being sent as something Klaviyo would answer differently.
    Sending the arguments of an \c OR joined by commas would answer with their intersection, which is the
    wrong set and would look like a working search.

    A set predicate is spelled \c any and its value is **always a JSON array**: Klaviyo rejects
    <tt>any(email,"x")</tt> as an invalid filter and accepts <tt>any(email,["x"])</tt>.  The application this
    one replaces sent a bare string whenever it had exactly one value, so its single-value filters never
    worked at all.

    @section klaviyorecord_fieldops Each Field Accepts Different Comparisons

    Klaviyo indexes each field for particular comparisons and answers a filter using any other one with an
    **error**, so the matrix is declared per field rather than per table.  A profile's \c id takes \c equals
    and \c any; its \c created takes only \c greater-than and \c less-than - there is no \c greater-or-equal
    on a profile at all, though there is one on a campaign's \c created_at and on an event's \c datetime.
    Every entry below is transcribed from the argument's own description in the pinned schema.

    A predicate the matrix does not allow is not sent: it is evaluated as the records arrive, which is
    slower and is right, where sending it would fail the search outright.

    @section klaviyorecord_campaigns Campaigns Are Scoped, Not Searched

    Klaviyo will not list campaigns at all without being told which channel to look at - \c filter is a
    **required** argument there, and \c messages.channel accepts only \c equals.  The listing action
    therefore publishes the filter as the two options the superseded application offered, \c channel and
    \c name, and there is no raw \c filter argument left for a rendered expression to travel in: one query
    argument cannot carry both the required channel and a whole expression.

    So the campaigns table declares \c channel as a **scope option** - it identifies the collection and is
    the same for every record in a search - and narrows on \c name with a filter rule.  It does not claim
    native search, because it does not have it.

    @section klaviyorecord_metrics Metrics Are Read Only and Unfiltered

    Klaviyo filters metrics on \c integration.name and \c integration.category only, neither of which is a
    column of a metric record - both are properties of its \c integration object - so that table declares no
    filter expression language and narrows nothing.  A metric is also never written through this API: it is
    defined by the first event that names it.  Klaviyo declares no page size on that endpoint either, only a
    cursor.

    @section klaviyorecord_events Events Are Appended, Never Changed

    An event is a record of something that happened, so it is created and never updated or deleted.  It is
    created through **Create Event**, which is hand-written rather than generated - Klaviyo's event body
    nests a metric envelope and a profile envelope inside its attributes - and a table can only name an
    action the manifest exports, so this table reads and searches and does not create.

    @section klaviyorecord_paging Paging Is by Cursor, and the Cap Differs per Endpoint

    Every listing pages by cursor, and Klaviyo answers with a whole **URL** in \c links.next rather than with
    the cursor itself; the response overlay merges those links up, so the next page is at \c next and
    @ref KlaviyoWatchDataProviderBase::getCursorFromLink() "getCursorFromLink()" takes the cursor out of it.

    \c {page[size]} caps differ per endpoint - 10 for lists and segments, 50 for tags, 100 for profiles,
    campaigns and events - and Klaviyo **rejects** a larger value rather than clamping it, so the maximum is
    declared per table.

    @section klaviyorecord_writenames A Column Is Not Always Spelled Like Its Option

    Klaviyo returns a list's \c opt_in_process and the update action publishes \c optInProcess, because that
    is the name the application this one replaces used and preserving it was a commitment of the port.  A
    write sends the columns the write action declares, so each table maps the pairs with
    @ref RestSchemaActions::RestSchemaRecordTableInfo::write_field_map "write_field_map"; without it those
    columns would be dropped and an update would report success having changed nothing.

    @section klaviyorecord_nodelete A Profile Is Not Deleted Through This API

    Klaviyo deletes a profile through a privacy-request flow rather than through the profiles API, so the
    profiles table registers no delete action.  Everything else here that can be deleted, is.
*/
public class KlaviyoRecordTables {
    public {
        #! The action group the record-based actions belong to
        const RecordGroup = "Record-Based";

        #! Klaviyo's filter expression language
        /** @see @ref klaviyorecord_filter
        */
        const Filter = <RestSchemaQuerySyntaxInfo>{
            "name": "Klaviyo filter",
            # every predicate is a prefix call; Klaviyo has no infix form at all
            "binary_ops": {},
            "call_ops": {
                DP_SEARCH_OP_EQ: "equals",
                DP_SEARCH_OP_IN: "any",
                DP_SEARCH_OP_GT: "greater-than",
                DP_SEARCH_OP_GE: "greater-or-equal",
                DP_SEARCH_OP_LT: "less-than",
                DP_SEARCH_OP_LE: "less-or-equal",
                DP_OP_CONTAINS: "contains",
                DP_OP_STARTS_WITH: "starts-with",
                DP_OP_ENDS_WITH: "ends-with",
            },
            # predicates are joined by a bare comma and are never grouped
            "and_sep": ",",
            "parenthesize": False,
            # Klaviyo has neither, and declaring one would send a query it cannot evaluate
            "or_op": NOTHING,
            "not_op": NOTHING,
            "render_value": \KlaviyoRecordTables::renderValue(),
        };

        #! The operators Klaviyo does not mean exactly, so that the predicate is answered here as well
        /** Klaviyo documents neither the case sensitivity nor the collation of its three string predicates,
            so each is sent - it narrows what has to be read - and applied again to what comes back.
        */
        const InexactOps = {
            DP_OP_CONTAINS: True,
            DP_OP_STARTS_WITH: True,
            DP_OP_ENDS_WITH: True,
        };

        #! How a collection paged 100 at a time walks from one page to the next
        const Paging100 = <RestSchemaRecordPagingInfo>{
            "mode": RSRP_CURSOR,
            "page_option": "cursor",
            "size_option": "pageSize",
            "max_size": 100,
            "next_path": ("next",),
            "next_cursor": \KlaviyoWatchDataProviderBase::getCursorFromLink(),
        };

        #! How a collection paged 50 at a time walks from one page to the next
        const Paging50 = <RestSchemaRecordPagingInfo>{
            "mode": RSRP_CURSOR,
            "page_option": "cursor",
            "size_option": "pageSize",
            "max_size": 50,
            "next_path": ("next",),
            "next_cursor": \KlaviyoWatchDataProviderBase::getCursorFromLink(),
        };

        #! How a collection paged 10 at a time walks from one page to the next
        const Paging10 = <RestSchemaRecordPagingInfo>{
            "mode": RSRP_CURSOR,
            "page_option": "cursor",
            "size_option": "pageSize",
            "max_size": 10,
            "next_path": ("next",),
            "next_cursor": \KlaviyoWatchDataProviderBase::getCursorFromLink(),
        };

        #! How a collection with a cursor and no page size at all walks from one page to the next
        const PagingNoSize = <RestSchemaRecordPagingInfo>{
            "mode": RSRP_CURSOR,
            "page_option": "cursor",
            "next_path": ("next",),
            "next_cursor": \KlaviyoWatchDataProviderBase::getCursorFromLink(),
        };

        #! The profiles table
        /** @see @ref klaviyorecord_nodelete
        */
        const ProfilesTable = <RestSchemaRecordTableInfo>{
            "name": "profiles",
            "display_name": "Profiles",
            "short_desc": "Klaviyo profiles as records",
            "desc": "Klaviyo profiles as records; Klaviyo evaluates a search over them itself.\n\n"
                "A profile is not deleted through this API - deletion is a privacy-request flow - so this "
                "table reads, creates and updates only",
            "list_action": "list_profiles",
            "get_action": "get_profile",
            "create_action": "create_profile",
            "update_action": "update_profile",
            "id_field": "id",
            "records_path": ("data",),
            "paging": Paging100,
            "search_options": ("sort", "additionalFields"),
            # Klaviyo returns both of these on every profile and describes neither anywhere in its
            # description; a column the table does not declare cannot be searched at all
            "extra_columns": RestSchemaTableDataProvider::makeRecordType((
                {
                    "name": "anonymous_id",
                    "desc": "the browser identifier Klaviyo assigns a visitor before it knows who they "
                        "are; Klaviyo returns it on every profile and declares it in no schema",
                    "type": "*string",
                },
                {
                    "name": "whatsapp_bsuid",
                    "desc": "the WhatsApp business-scoped user identifier for the profile; Klaviyo returns "
                        "it on every profile and declares it in no schema",
                    "type": "*string",
                },
            )),
            "write_field_map": {
                "first_name": "firstName",
                "last_name": "lastName",
                "phone_number": "phoneNumber",
                "external_id": "externalId",
                "image": "imageUrl",
                "properties": "customProperties",
            },
            "query": <RestSchemaRecordQueryInfo>{
                "option": "filter",
                "syntax": Filter,
                "inexact_ops": InexactOps,
                "field_ops": {
                    "id": {DP_SEARCH_OP_EQ: True, DP_SEARCH_OP_IN: True},
                    "email": {DP_SEARCH_OP_EQ: True, DP_SEARCH_OP_IN: True},
                    "phone_number": {DP_SEARCH_OP_EQ: True, DP_SEARCH_OP_IN: True},
                    "external_id": {DP_SEARCH_OP_EQ: True, DP_SEARCH_OP_IN: True},
                    # Klaviyo has no greater-or-equal or less-or-equal on a profile's timestamps
                    "created": {DP_SEARCH_OP_GT: True, DP_SEARCH_OP_LT: True},
                    "updated": {DP_SEARCH_OP_GT: True, DP_SEARCH_OP_LT: True},
                },
            },
        };

        #! The lists table
        const ListsTable = <RestSchemaRecordTableInfo>{
            "name": "lists",
            "display_name": "Lists",
            "short_desc": "Klaviyo lists as records",
            "desc": "Klaviyo lists as records; Klaviyo evaluates a search over them itself.\n\n"
                "A **list** holds profiles that opted in explicitly, where a **segment** is a rule Klaviyo "
                "evaluates. Deleting a list does not delete the profiles on it",
            "list_action": "list_lists",
            "get_action": "get_list",
            "create_action": "create_list",
            "update_action": "update_list",
            "delete_action": "delete_list",
            "id_field": "id",
            "records_path": ("data",),
            "paging": Paging10,
            "search_options": ("sort",),
            "write_field_map": {"opt_in_process": "optInProcess"},
            "query": <RestSchemaRecordQueryInfo>{
                "option": "filter",
                "syntax": Filter,
                "inexact_ops": InexactOps,
                "field_ops": {
                    "id": {DP_SEARCH_OP_EQ: True, DP_SEARCH_OP_IN: True},
                    "name": {DP_SEARCH_OP_EQ: True, DP_SEARCH_OP_IN: True},
                    # Klaviyo indexes a list's timestamps for "greater-than" and nothing else
                    "created": {DP_SEARCH_OP_GT: True},
                    "updated": {DP_SEARCH_OP_GT: True},
                },
            },
        };

        #! The segments table
        const SegmentsTable = <RestSchemaRecordTableInfo>{
            "name": "segments",
            "display_name": "Segments",
            "short_desc": "Klaviyo segments as records",
            "desc": "Klaviyo segments as records; Klaviyo evaluates a search over them itself.\n\n"
                "A **segment** is a rule Klaviyo evaluates continuously, so its membership changes on its "
                "own. Creating one needs a `definition`, which is the rule; deleting a segment does not "
                "delete the profiles it matched",
            "list_action": "list_segments",
            "get_action": "get_segment",
            "create_action": "create_segment",
            "update_action": "update_segment",
            "delete_action": "delete_segment",
            "id_field": "id",
            "records_path": ("data",),
            "paging": Paging10,
            "search_options": ("sort",),
            "write_field_map": {"is_starred": "isStarred", "is_active": "isActive"},
            "query": <RestSchemaRecordQueryInfo>{
                "option": "filter",
                "syntax": Filter,
                "inexact_ops": InexactOps,
                "field_ops": {
                    "id": {DP_SEARCH_OP_EQ: True, DP_SEARCH_OP_IN: True},
                    "name": {DP_SEARCH_OP_EQ: True, DP_SEARCH_OP_IN: True},
                    "created": {DP_SEARCH_OP_GT: True},
                    "updated": {DP_SEARCH_OP_GT: True},
                    "is_active": {DP_SEARCH_OP_EQ: True, DP_SEARCH_OP_IN: True},
                    "is_starred": {DP_SEARCH_OP_EQ: True},
                },
            },
        };

        #! The tags table
        /** A tag's only attribute is its name, so \c name is the one field Klaviyo will filter on and the
            one value an update changes.
        */
        const TagsTable = <RestSchemaRecordTableInfo>{
            "name": "tags",
            "display_name": "Tags",
            "short_desc": "Klaviyo tags as records",
            "desc": "Klaviyo tags as records; Klaviyo evaluates a search over them itself.\n\n"
                "A tag groups campaigns, flows, lists and segments for reporting. Its name is its only "
                "attribute, so it is the only value a search narrows on and the only one an update changes",
            "list_action": "list_tags",
            "get_action": "get_tag",
            "create_action": "create_tag",
            "update_action": "update_tag",
            "delete_action": "delete_tag",
            "id_field": "id",
            "records_path": ("data",),
            "paging": Paging50,
            "search_options": ("sort",),
            "query": <RestSchemaRecordQueryInfo>{
                "option": "filter",
                "syntax": Filter,
                "inexact_ops": InexactOps,
                "field_ops": {
                    "name": {
                        DP_SEARCH_OP_EQ: True,
                        DP_OP_CONTAINS: True,
                        DP_OP_STARTS_WITH: True,
                        DP_OP_ENDS_WITH: True,
                    },
                },
            },
        };

        #! The campaigns table
        /** @see @ref klaviyorecord_campaigns
        */
        const CampaignsTable = <RestSchemaRecordTableInfo>{
            "name": "campaigns",
            "display_name": "Campaigns",
            "short_desc": "the campaigns on one channel, as records",
            "desc": "The campaigns on one channel, as records.\n\n"
                "**`channel` is required**: Klaviyo does not list campaigns across channels, so a search is "
                "always of one channel's campaigns. A search narrows on the campaign name and the rest of a "
                "search is evaluated as the records arrive - the channel argument is the whole of what "
                "Klaviyo's filter can carry here",
            "list_action": "list_campaigns",
            "get_action": "get_campaign",
            "create_action": "create_campaign",
            "update_action": "update_campaign",
            "delete_action": "delete_campaign",
            "id_field": "id",
            "records_path": ("data",),
            "scope_options": ("channel",),
            "paging": Paging100,
            "search_options": ("sort",),
            "write_field_map": {
                "send_strategy": "sendStrategy",
                "send_options": "sendOptions",
                "tracking_options": "trackingOptions",
            },
            "filters": (
                <RestSchemaRecordFilterInfo>{
                    "field": "name", "operator": DP_OP_CONTAINS, "option": "name",
                },
            ),
        };

        #! The metrics table
        /** @see @ref klaviyorecord_metrics
        */
        const MetricsTable = <RestSchemaRecordTableInfo>{
            "name": "metrics",
            "display_name": "Metrics",
            "short_desc": "the metrics defined in the account, as records",
            "desc": "The metrics defined in the account, as records.\n\n"
                "A **metric** is the kind of an event - \"Placed Order\", \"Viewed Product\" - and Klaviyo "
                "defines one the first time an event names it, so metrics are read only here. Klaviyo "
                "filters them only on the integration that produced them, which is not a column of a "
                "metric, so a search is evaluated as the records arrive",
            "list_action": "list_metrics",
            "get_action": "get_metric",
            "id_field": "id",
            "records_path": ("data",),
            "paging": PagingNoSize,
        };

        #! The events table
        /** @see @ref klaviyorecord_events
        */
        const EventsTable = <RestSchemaRecordTableInfo>{
            "name": "events",
            "display_name": "Events",
            "short_desc": "the events recorded in the account, as records",
            "desc": "The events recorded in the account, as records; Klaviyo evaluates a search over their "
                "timestamps itself.\n\n"
                "An event records something that happened, so it is appended and never changed or removed. "
                "Use **Create Event** to record one: Klaviyo's event body nests a metric and a profile, "
                "which is more than a record carries",
            "list_action": "list_events",
            "get_action": "get_event",
            "id_field": "id",
            "records_path": ("data",),
            "paging": Paging100,
            "search_options": ("sort",),
            "query": <RestSchemaRecordQueryInfo>{
                "option": "filter",
                "syntax": Filter,
                "inexact_ops": InexactOps,
                "field_ops": {
                    "datetime": {
                        DP_SEARCH_OP_GT: True, DP_SEARCH_OP_GE: True,
                        DP_SEARCH_OP_LT: True, DP_SEARCH_OP_LE: True,
                    },
                    "timestamp": {
                        DP_SEARCH_OP_GT: True, DP_SEARCH_OP_GE: True,
                        DP_SEARCH_OP_LT: True, DP_SEARCH_OP_LE: True,
                    },
                },
            },
        };

        #! Every table, in presentation order
        const Tables = (ProfilesTable, ListsTable, SegmentsTable, TagsTable, CampaignsTable, MetricsTable,
            EventsTable);

        #! The IDs the record-based actions are registered under
        const RecordActionIds = <RestSchemaRecordActionIds>{};

        #! UTC, which is the zone Klaviyo compares its timestamps in
        const Utc = new TimeZone("UTC");
    }

    #! Renders one value as Klaviyo's filter syntax spells it
    /** Every literal is JSON, which is what Klaviyo's filter takes: a string is double-quoted and escaped, a
        boolean is a bare keyword and a set is a JSON array.  A set is **always** an array even when it holds
        one value - Klaviyo rejects <tt>any(email,"x")</tt> - and the renderer is handed the whole set, so
        that falls out rather than being a special case.

        A point in time is written as an ISO-8601 instant in UTC, which is how Klaviyo compares its
        timestamps; a value that carries a date and no time of day still has one, and midnight is what the
        caller wrote.
    */
    static string renderValue(auto value, string field) {
        if (value.typeCode() == NT_DATE) {
            return make_json(KlaviyoRecordTables::renderDate(value));
        }
        if (value.typeCode() == NT_LIST) {
            # a set predicate's values are rendered one at a time so that a date inside one is an instant
            # rather than whatever JSON makes of it
            return "[" + ((map KlaviyoRecordTables::renderValue($1, field), value).join(",")) + "]";
        }
        return make_json(value);
    }

    #! Returns a point in time as the ISO-8601 instant in UTC that Klaviyo compares against
    static string renderDate(date value) {
        return Utc.date(value).format("YYYY-MM-DDTHH:mm:SS") + "Z";
    }
}
}
