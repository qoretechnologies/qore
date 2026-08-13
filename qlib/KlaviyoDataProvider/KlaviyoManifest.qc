# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KlaviyoDataProvider module definition

/*  KlaviyoManifest.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
#! What this application exports from Klaviyo's description, and how each action is presented
/** The manifest is the application's own list of what it exports.  An operation Klaviyo declares does not
    become a public action until it is named here, and an action's ID never changes because the schema
    renamed an \c operationId.

    @section klaviyomanifest_identity Two Identities Are Preserved, Not One

    Every earlier application on this path was generated from an \c allowed-paths map, so its option names
    were already Klaviyo's - or Xero's, or GitHub's - own.  This one was not.  Klaviyo's application was 21
    hand-written actions over the \c klaviyo-api SDK, with a designed, flat, camelCase option surface, and
    around seventy option names that differ from the wire by case, by nesting, or by both.

    Existing workflows supply option values **by name**, so those names are as much a commitment as the
    action IDs are, and this manifest reproduces both.  Three mechanisms carry the weight:

    |!Mechanism|!What it fixes|!Where
    |@ref RestSchemaActions::RestSchemaActionInfo::request_unwrap "request_unwrap"|Klaviyo is JSON:API, so \
every write body is \c "{data: {type, attributes: {…}}}" and flattening one level would publish a single \
\c data option|the five write actions
    |@ref RestSchemaActions::RestSchemaFieldOverlayInfo::lift "lift"|a postal address arrives as a nested \
\c location object, and the campaign filter is one required string where the application published two \
options|the three profile writes, and \c list_campaigns
    |@ref KlaviyoDataProvider::KlaviyoCodecs "wire codecs"|\c filter and \c sort are compound strings in a \
syntax of Klaviyo's own|the five listings
    |overlay \c name renames|\c page[cursor] is \c cursor, \c phone_number is \c phoneNumber, and so on|\
everywhere

    @section klaviyomanifest_handwritten What Is Not Generated

    Nine of the twenty-one actions build a request body that Klaviyo's description cannot describe as one
    option shape, and they are written by hand:

    |!Actions|!Why
    |the six \c add_* / \c remove_* relationship actions|the body is a JSON:API relationship **array**, \
\c "{data: [{type, id}]}", and the action takes one identifier rather than a list; an array has no \
properties to lift
    |\c create_event|the body nests a metric envelope and a profile envelope inside the event's attributes, \
three levels deep, and the application published a metric name and a profile ID
    |\c subscribe_profile, \c unsubscribe_profile|the body is a bulk job whose \c profiles carry their own \
subscription envelopes per channel, and the application published one profile's e-mail and phone number

    Each keeps its ID and its options.  See
    @ref KlaviyoDataProvider::KlaviyoRelationshipDataProvider "KlaviyoRelationshipDataProvider" and
    @ref KlaviyoDataProvider::KlaviyoCreateEventDataProvider "KlaviyoCreateEventDataProvider".

    @section klaviyomanifest_added What Was Added

    Two read-only listings the superseded application did not export, both because a dropdown needs them:
    \c list_metrics fills the metric picker on **Create Event** and the **New Event** trigger, and
    \c list_events fills the event picker.  Exporting a listing to fill a dropdown is deliberate - a
    dropdown can only name an action the manifest exports.

    @section klaviyomanifest_dropped What Is Deliberately Not Published

    Every listing declares JSON:API's \c include and its sparse-fieldset \c "fields[…]" arguments, and none
    is published.  Both change the **shape of the response** - \c include adds a top-level \c included
    array, a fieldset removes attributes - while the action's output type is derived from the description's
    unmodified response.  Publishing an option that can silently invalidate the declared output type would
    be worse than not having it; \c make-api-call reaches them.
*/
public class KlaviyoManifest {
    public {
        #! Action group: profiles
        const GroupProfiles = "Profiles";

        #! Action group: lists
        const GroupLists = "Lists";

        #! Action group: segments
        const GroupSegments = "Segments";

        #! Action group: tags
        const GroupTags = "Tags";

        #! Action group: campaigns
        const GroupCampaigns = "Campaigns";

        #! Action group: events and metrics
        const GroupEvents = "Events";

        #! Action group: subscriptions
        const GroupSubscriptions = "Subscriptions";

        #! Reference data: profiles, keyed by profile ID
        const RefProfile = "klaviyo-profile";

        #! Reference data: lists, keyed by list ID
        const RefList = "klaviyo-list";

        #! Reference data: segments, keyed by segment ID
        const RefSegment = "klaviyo-segment";

        #! Reference data: tags, keyed by tag ID
        const RefTag = "klaviyo-tag";

        #! Reference data: campaigns, keyed by campaign ID
        const RefCampaign = "klaviyo-campaign";

        #! Reference data: metrics, keyed by metric name
        /** \c create_event names the metric by **name** rather than by ID, because Klaviyo creates a metric
            on first use: an event may name a metric that does not exist yet, and that is how a new one is
            defined.
        */
        const RefMetricName = "klaviyo-metric-name";

        #! Reference data: metrics, keyed by metric ID
        /** The **New Event** trigger filters events by \c metric_id, so it needs the identifier where
            \c create_event needs the name.
        */
        const RefMetricId = "klaviyo-metric-id";

        #! Reference data: events, keyed by event ID
        const RefEvent = "klaviyo-event";

        #! How many records a dropdown reads in one page
        /** Klaviyo caps \c page[size] per endpoint - 10 on lists and segments, 50 on tags, 100 on profiles
            and campaigns - and rejects a larger value rather than clamping it, so this is the largest value
            every listing a dropdown uses accepts.
        */
        const RefPageSize = 10;

        #! The header naming the API revision, which the connection supplies rather than the action
        /** Klaviyo declares this header **required** on 344 of its 345 operations, which is the good case:
            a connection that does not send it is reported naming the option to set rather than failing as a
            schema type error several frames down.

            @see @ref klaviyorestclient_revision
        */
        const RevisionOverlay = <RestSchemaFieldOverlayInfo>{
            "ignore": True,
            "connection_option": "headers",
        };

        #! An argument the action deliberately does not publish
        const IgnoreOverlay = <RestSchemaFieldOverlayInfo>{
            "ignore": True,
        };

        #! The cursor argument, which Klaviyo spells with a JSON:API bracket
        const CursorOverlay = <RestSchemaFieldOverlayInfo>{
            "name": "cursor",
            "display_name": "Cursor",
            "short_desc": "The cursor for the next page",
            "desc": "The cursor for the next page: pass the value from the previous response's "
                "`links.next`.\n\nIt is opaque and must not be constructed by hand; leave it unset to read "
                "the first page.",
        };

        #! The page size argument, which Klaviyo spells with a JSON:API bracket
        const PageSizeOverlay = <RestSchemaFieldOverlayInfo>{
            "name": "pageSize",
            "display_name": "Page Size",
            "short_desc": "How many records to return in one page",
            "desc": "How many records to return in one page.\n\nKlaviyo caps this per endpoint and rejects "
                "a larger value rather than clamping it; the maximum is in the argument's own description.",
        };

        #! The additional-fields argument, which asks for computed blocks a profile omits by default
        const AdditionalFieldsOverlay = <RestSchemaFieldOverlayInfo>{
            "name": "additionalFields",
            "display_name": "Additional Fields",
            "short_desc": "Computed blocks to include on each profile",
            "desc": "Computed blocks to include on each profile, which Klaviyo omits by default because "
                "they are expensive to produce:\n\n"
                "- `subscriptions`: the profile's consent state per channel\n"
                "- `predictive_analytics`: Klaviyo's predicted lifetime value and churn probability",
        };

        #! The overlay naming the list a single-record action works on
        const ListIdOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "List",
            "short_desc": "The list to work on",
            "ref_data": RefList,
            "supports_custom_values": True,
            "preselected": True,
        };

        #! The overlay naming the segment a single-record action works on
        const SegmentIdOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Segment",
            "short_desc": "The segment to work on",
            "ref_data": RefSegment,
            "supports_custom_values": True,
            "preselected": True,
        };

        #! The overlay naming the tag a single-record action works on
        const TagIdOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Tag",
            "short_desc": "The tag to work on",
            "ref_data": RefTag,
            "supports_custom_values": True,
            "preselected": True,
        };

        #! The overlay naming the campaign a single-record action works on
        const CampaignIdOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Campaign",
            "short_desc": "The campaign to work on",
            "ref_data": RefCampaign,
            "supports_custom_values": True,
            "preselected": True,
        };

        #! The overlay for a listing's sort argument
        /** The permitted fields differ per listing and are set per action; the direction does not.

            @see @ref KlaviyoDataProvider::KlaviyoCodecs "KlaviyoCodecs"
        */
        const SortOverlayBase = <RestSchemaFieldOverlayInfo>{
            "codec": KlaviyoCodecs::Sort,
            "display_name": "Sort",
            "short_desc": "The field and direction to sort by",
            "desc": "The field and direction to sort by",
        };

        #! The location object on a profile, published as the options the superseded application offered
        const LocationOverlay = <RestSchemaFieldOverlayInfo>{
            "lift": True,
            "fields": {
                "city": <RestSchemaFieldOverlayInfo>{"display_name": "City"},
                "region": <RestSchemaFieldOverlayInfo>{
                    "display_name": "Region",
                    "short_desc": "The state, province or region",
                },
                "country": <RestSchemaFieldOverlayInfo>{"display_name": "Country"},
                "zip": <RestSchemaFieldOverlayInfo>{
                    "display_name": "Postal Code",
                    "short_desc": "The postal or ZIP code",
                },
                "address1": <RestSchemaFieldOverlayInfo>{"display_name": "Address Line 1"},
                "address2": <RestSchemaFieldOverlayInfo>{"display_name": "Address Line 2"},
                "latitude": <RestSchemaFieldOverlayInfo>{"display_name": "Latitude"},
                "longitude": <RestSchemaFieldOverlayInfo>{"display_name": "Longitude"},
                "timezone": <RestSchemaFieldOverlayInfo>{
                    "display_name": "Time Zone",
                    "short_desc": "The IANA time zone name",
                },
                "ip": <RestSchemaFieldOverlayInfo>{
                    "display_name": "IP Address",
                    "short_desc": "The IP address Klaviyo should locate the profile from",
                },
            },
        };

        #! The profile attribute overlays shared by the three profile write actions
        /** The three take the same attributes and the superseded application gave them the same names, so
            the overlay is shared rather than repeated: a rename that applied to two of the three would be
            a difference nobody meant.
        */
        const ProfileAttributeOverlay = {
            "email": <RestSchemaFieldOverlayInfo>{
                "display_name": "E-mail",
                "short_desc": "The profile's e-mail address",
                "preselected": True,
            },
            "phone_number": <RestSchemaFieldOverlayInfo>{
                "name": "phoneNumber",
                "display_name": "Phone Number",
                "short_desc": "The profile's phone number in E.164 format",
                "desc": "The profile's phone number in E.164 format, ex: `+15005550006`.\n\nKlaviyo "
                    "rejects a number in any other format.",
                "preselected": True,
            },
            "first_name": <RestSchemaFieldOverlayInfo>{
                "name": "firstName",
                "display_name": "First Name",
                "preselected": True,
            },
            "last_name": <RestSchemaFieldOverlayInfo>{
                "name": "lastName",
                "display_name": "Last Name",
                "preselected": True,
            },
            "external_id": <RestSchemaFieldOverlayInfo>{
                "name": "externalId",
                "display_name": "External ID",
                "short_desc": "Your own identifier for this person",
                "desc": "Your own identifier for this person, so that a profile can be found by the ID it "
                    "has in your system rather than by e-mail",
            },
            "image": <RestSchemaFieldOverlayInfo>{
                "name": "imageUrl",
                "display_name": "Image URL",
                "short_desc": "A URL for the profile's avatar",
            },
            "properties": <RestSchemaFieldOverlayInfo>{
                "name": "customProperties",
                "display_name": "Custom Properties",
                "short_desc": "Your own properties to store on the profile",
                "desc": "Your own properties to store on the profile, as name/value pairs.\n\nKlaviyo "
                    "reserves property names beginning with `$`.",
            },
            "title": <RestSchemaFieldOverlayInfo>{
                "display_name": "Title",
                "short_desc": "The person's job title",
            },
            "organization": <RestSchemaFieldOverlayInfo>{
                "display_name": "Organization",
                "short_desc": "The company the person belongs to",
            },
            "locale": <RestSchemaFieldOverlayInfo>{
                "display_name": "Locale",
                "short_desc": "The profile's locale, ex: \"en-US\"",
            },
            "location": LocationOverlay,
        };

        #! The response overlay for an action answering one JSON:API resource
        /** Klaviyo answers \c "{data: {id, type, attributes: {…}}}" for a single resource, so an e-mail
            address is two levels down and a workflow reading one has to know that \c attributes exists at
            all.  The superseded application published the resource flat - identity and attributes
            together - and this reproduces that: the envelope is unwrapped and the attributes are merged
            into the identity beside them.

            \c links and \c relationships are **kept**, unlike in the application being replaced, which
            dropped them.  They are additive, and \c relationships is the only way to see what a resource
            belongs to without a second call.
        */
        const SingleResourceResponse = {
            "attributes": <RestSchemaFieldOverlayInfo>{"merge": True},
        };

        #! The response overlay for an action answering a JSON:API collection
        /** The same merge as @ref SingleResourceResponse, applied to each element of \c data - plus the
            paging links, merged so that the cursor for the next page is \c next at the top level, which is
            where the superseded application put it.
        */
        const CollectionResponse = {
            "data": <RestSchemaFieldOverlayInfo>{
                "fields": SingleResourceResponse,
            },
            "links": <RestSchemaFieldOverlayInfo>{"merge": True},
        };

        #! The option order the three profile write actions present
        const ProfileFieldOrder = (
            "email", "phoneNumber", "firstName", "lastName", "title", "organization",
            "city", "region", "country", "zip", "imageUrl", "externalId", "customProperties",
        );

        #! What this application exports
        const Manifest = (
            #
            # Profiles
            #
            <RestSchemaActionInfo>{
                "action": "list_profiles",
                "method": "GET",
                "path": "/api/profiles",
                "operation_id": "get_profiles",
                "display_name": "List Profiles",
                "short_desc": "Lists profiles",
                "desc": "Lists the profiles in the account, newest first by default.\n\n"
                    "`filter` narrows the listing by e-mail, phone number, external ID or profile ID; each "
                    "takes one or more values and matches any of them. Paging is by cursor: pass the "
                    "previous response's `links.next` back as `cursor`.",
                "groups": (GroupProfiles,),
                "field_order": ("filter", "sort", "additionalFields", "pageSize", "cursor"),
                "response_fields": CollectionResponse,
                "fields": {
                    "page[cursor]": CursorOverlay,
                    "page[size]": PageSizeOverlay,
                    "additional-fields[profile]": AdditionalFieldsOverlay,
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "codec": KlaviyoCodecs::ProfileFilter,
                        "display_name": "Filter",
                        "short_desc": "Narrow the listing to matching profiles",
                        "desc": "Narrow the listing to matching profiles",
                    },
                    "sort": SortOverlayBase + <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "field": <RestSchemaFieldOverlayInfo>{
                                "default_value": "created",
                                "allowed_values": (
                                    <AllowedValueInfo>{"value": "created", "display_name": "Created"},
                                    <AllowedValueInfo>{"value": "updated", "display_name": "Updated"},
                                    <AllowedValueInfo>{"value": "email", "display_name": "E-mail"},
                                    <AllowedValueInfo>{"value": "id", "display_name": "ID"},
                                    <AllowedValueInfo>{
                                        "value": "subscriptions.email.marketing.suppression.timestamp",
                                        "display_name": "E-mail Suppression Timestamp",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "subscriptions.email.marketing.list_suppressions.timestamp",
                                        "display_name": "E-mail List Suppression Timestamp",
                                    },
                                ),
                            },
                        },
                    },
                    "fields[conversation]": IgnoreOverlay,
                    "fields[profile]": IgnoreOverlay,
                    "fields[push-token]": IgnoreOverlay,
                    "include": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "get_profile",
                "method": "GET",
                "path": "/api/profiles/{id}",
                "operation_id": "get_profile",
                "display_name": "Get Profile",
                "short_desc": "Retrieves a specific profile",
                "desc": "Retrieves one profile by its Klaviyo ID",
                "groups": (GroupProfiles,),
                "field_order": ("id", "additionalFields"),
                "response_unwrap": "data",
                "response_fields": SingleResourceResponse,
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Profile",
                        "short_desc": "The profile to retrieve",
                        "ref_data": RefProfile,
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "additional-fields[profile]": AdditionalFieldsOverlay,
                    "fields[conversation]": IgnoreOverlay,
                    "fields[list]": IgnoreOverlay,
                    "fields[profile]": IgnoreOverlay,
                    "fields[push-token]": IgnoreOverlay,
                    "fields[segment]": IgnoreOverlay,
                    "include": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "create_profile",
                "method": "POST",
                "path": "/api/profiles",
                "operation_id": "create_profile",
                "display_name": "Create Profile",
                "short_desc": "Creates a new profile",
                "desc": "Creates a profile.\n\n"
                    "A profile needs at least one identifier - an e-mail address, a phone number or an "
                    "external ID. Klaviyo **rejects** a create whose identifier already belongs to a "
                    "profile; use **Create Or Update Profile** when the person may already be known.",
                "groups": (GroupProfiles,),
                "request_unwrap": ("data", "attributes"),
                "field_order": ProfileFieldOrder,
                "response_unwrap": "data",
                "response_fields": SingleResourceResponse,
                "fields": ProfileAttributeOverlay + {
                    "email": ProfileAttributeOverlay.email + <RestSchemaFieldOverlayInfo>{
                        "required_groups": ("identifier",),
                    },
                    "phone_number": ProfileAttributeOverlay.phone_number + <RestSchemaFieldOverlayInfo>{
                        "required_groups": ("identifier",),
                    },
                    "external_id": ProfileAttributeOverlay.external_id + <RestSchemaFieldOverlayInfo>{
                        "required_groups": ("identifier",),
                    },
                    "additional-fields[profile]": IgnoreOverlay,
                    "fields[profile]": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "create_or_update_profile",
                "method": "POST",
                "path": "/api/profile-import",
                "operation_id": "create_or_update_profile",
                "display_name": "Create Or Update Profile",
                "short_desc": "Creates a new profile or updates an existing one",
                "desc": "Creates a profile, or updates the one already carrying the identifier given.\n\n"
                    "Klaviyo matches on `id` when it is set, and otherwise on `email`, `phoneNumber` or "
                    "`externalId` - so this is the action to use when the person may already be known. "
                    "A custom property supplied here **replaces** the stored value; use **Update Profile** "
                    "with `meta.patch_properties` to append to one instead.",
                "groups": (GroupProfiles,),
                "request_unwrap": ("data", "attributes"),
                "field_order": ("id",) + ProfileFieldOrder,
                # Klaviyo answers 201 when it created the profile and 200 when it updated one, with the
                # same body either way; an action has one output contract, so the update case is the one
                # published - it is the common one, and the shapes do not differ
                "response_code": "200",
                "response_unwrap": "data",
                "response_fields": SingleResourceResponse,
                "fields": ProfileAttributeOverlay + {
                    "body.data.id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Profile",
                        "short_desc": "The profile to update, if it is already known",
                        "desc": "The profile to update.\n\nLeave it unset to match on an identifier "
                            "instead, which is the usual way to use this action.",
                        "ref_data": RefProfile,
                        "supports_custom_values": True,
                    },
                    "body.data.meta": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Patch Operations",
                        "short_desc": "Append to or unset custom properties rather than replacing them",
                        "desc": "Append to or unset custom properties rather than replacing them.\n\n"
                            "`patch_properties.append` adds a value to a list-valued property, `unappend` "
                            "removes one, and `unset` deletes a property outright.",
                    },
                    "_kx": IgnoreOverlay,
                    "additional-fields[profile]": IgnoreOverlay,
                    "fields[profile]": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "update_profile",
                "method": "PATCH",
                "path": "/api/profiles/{id}",
                "operation_id": "update_profile",
                "display_name": "Update Profile",
                "short_desc": "Updates an existing profile",
                "desc": "Updates a profile that already exists, named by its Klaviyo ID.\n\n"
                    "Only the attributes supplied are changed. A custom property supplied here "
                    "**replaces** the stored value; `meta.patch_properties` appends to or unsets one "
                    "instead.",
                "groups": (GroupProfiles,),
                "request_unwrap": ("data", "attributes"),
                "field_order": ("id",) + ProfileFieldOrder,
                "response_unwrap": "data",
                "response_fields": SingleResourceResponse,
                "fields": ProfileAttributeOverlay + {
                    "path.id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Profile",
                        "short_desc": "The profile to update",
                        "ref_data": RefProfile,
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    # Klaviyo requires the profile's identity in the body as well as in the URL, and the
                    # two must always be equal; asking for it twice would only be a way to get it wrong
                    "body.data.id": <RestSchemaFieldOverlayInfo>{
                        "value_from": "id",
                    },
                    "body.data.meta": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Patch Operations",
                        "short_desc": "Append to or unset custom properties rather than replacing them",
                        "desc": "Append to or unset custom properties rather than replacing them.\n\n"
                            "`patch_properties.append` adds a value to a list-valued property, `unappend` "
                            "removes one, and `unset` deletes a property outright.",
                    },
                    "additional-fields[profile]": IgnoreOverlay,
                    "fields[profile]": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
            #
            # Lists
            #
            <RestSchemaActionInfo>{
                "action": "list_lists",
                "method": "GET",
                "path": "/api/lists",
                "operation_id": "get_lists",
                "display_name": "List Lists",
                "short_desc": "Lists all lists",
                "desc": "Lists the account's lists.\n\n"
                    "A **list** holds profiles that opted in explicitly, where a **segment** is a rule "
                    "Klaviyo evaluates. Paging is by cursor.",
                "groups": (GroupLists,),
                "field_order": ("filter", "sort", "cursor"),
                "response_fields": CollectionResponse,
                "fields": {
                    "page[cursor]": CursorOverlay,
                    "page[size]": PageSizeOverlay,
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "codec": KlaviyoCodecs::ListFilter,
                        "display_name": "Filter",
                        "short_desc": "Narrow the listing to matching lists",
                        "desc": "Narrow the listing to matching lists",
                    },
                    "sort": SortOverlayBase + <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "field": <RestSchemaFieldOverlayInfo>{
                                "default_value": "created",
                                "allowed_values": (
                                    <AllowedValueInfo>{"value": "created", "display_name": "Created"},
                                    <AllowedValueInfo>{"value": "updated", "display_name": "Updated"},
                                    <AllowedValueInfo>{"value": "name", "display_name": "Name"},
                                    <AllowedValueInfo>{"value": "id", "display_name": "ID"},
                                ),
                            },
                        },
                    },
                    "fields[flow]": IgnoreOverlay,
                    "fields[list]": IgnoreOverlay,
                    "fields[tag]": IgnoreOverlay,
                    "include": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "create_list",
                "method": "POST",
                "path": "/api/lists",
                "operation_id": "create_list",
                "display_name": "Create List",
                "short_desc": "Creates a new list",
                "desc": "Creates a list.\n\n"
                    "`optInProcess` decides whether a profile added to the list has to confirm by e-mail "
                    "before it is subscribed; Klaviyo uses the account's default when it is unset.",
                "groups": (GroupLists,),
                "request_unwrap": ("data", "attributes"),
                "field_order": ("name", "optInProcess"),
                "response_unwrap": "data",
                "response_fields": SingleResourceResponse,
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "The name of the list",
                        "preselected": True,
                    },
                    "opt_in_process": <RestSchemaFieldOverlayInfo>{
                        "name": "optInProcess",
                        "display_name": "Opt-in Process",
                        "short_desc": "Whether a new subscriber has to confirm by e-mail",
                    },
                    "fields[list]": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "get_list",
                "method": "GET",
                "path": "/api/lists/{id}",
                "operation_id": "get_list",
                "display_name": "Get List",
                "short_desc": "Retrieves a specific list",
                "desc": "Retrieves one list by its Klaviyo ID.\n\n"
                    "`additionalFields` adds the list's profile count, which Klaviyo does not return "
                    "otherwise because counting is expensive.",
                "groups": (GroupLists,),
                "field_order": ("id", "additionalFields"),
                "response_unwrap": "data",
                "response_fields": SingleResourceResponse,
                "fields": {
                    "id": ListIdOverlay,
                    "additional-fields[list]": AdditionalFieldsOverlay,
                    "fields[flow]": IgnoreOverlay,
                    "fields[list]": IgnoreOverlay,
                    "fields[tag]": IgnoreOverlay,
                    "include": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "update_list",
                "method": "PATCH",
                "path": "/api/lists/{id}",
                "operation_id": "update_list",
                "display_name": "Update List",
                "short_desc": "Updates an existing list",
                "desc": "Updates a list that already exists, named by its Klaviyo ID.\n\n"
                    "Only the attributes supplied are changed.",
                "groups": (GroupLists,),
                "request_unwrap": ("data", "attributes"),
                "field_order": ("id", "name", "optInProcess"),
                "response_unwrap": "data",
                "response_fields": SingleResourceResponse,
                "fields": {
                    "path.id": ListIdOverlay,
                    # Klaviyo requires the identity in the body as well as in the URL, and the two must
                    # always be equal; asking for it twice would only be a way to get it wrong
                    "body.data.id": <RestSchemaFieldOverlayInfo>{"value_from": "id"},
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "The new name of the list",
                        "preselected": True,
                    },
                    "opt_in_process": <RestSchemaFieldOverlayInfo>{
                        "name": "optInProcess",
                        "display_name": "Opt-in Process",
                        "short_desc": "Whether a new subscriber has to confirm by e-mail",
                    },
                    "fields[list]": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete_list",
                "method": "DELETE",
                "path": "/api/lists/{id}",
                "operation_id": "delete_list",
                "display_name": "Delete List",
                "short_desc": "Deletes a list",
                "desc": "Deletes a list.\n\n"
                    "The profiles on it are **not** deleted; they simply stop being members. Klaviyo "
                    "answers with no content.",
                "groups": (GroupLists,),
                "fields": {
                    "id": ListIdOverlay,
                    "revision": RevisionOverlay,
                },
            },
            #
            # Segments
            #
            <RestSchemaActionInfo>{
                "action": "list_segments",
                "method": "GET",
                "path": "/api/segments",
                "operation_id": "get_segments",
                "display_name": "List Segments",
                "short_desc": "Lists segments",
                "desc": "Lists the account's segments.\n\n"
                    "A **segment** is a rule Klaviyo evaluates continuously, so its membership changes on "
                    "its own; a **list** holds profiles that opted in explicitly. Paging is by cursor.",
                "groups": (GroupSegments,),
                "field_order": ("filter", "sort", "cursor"),
                "response_fields": CollectionResponse,
                "fields": {
                    "page[cursor]": CursorOverlay,
                    "page[size]": PageSizeOverlay,
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "codec": KlaviyoCodecs::SegmentFilter,
                        "display_name": "Filter",
                        "short_desc": "Narrow the listing to matching segments",
                        "desc": "Narrow the listing to matching segments",
                    },
                    "sort": SortOverlayBase + <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "field": <RestSchemaFieldOverlayInfo>{
                                "default_value": "created",
                                "allowed_values": (
                                    <AllowedValueInfo>{"value": "created", "display_name": "Created"},
                                    <AllowedValueInfo>{"value": "updated", "display_name": "Updated"},
                                    <AllowedValueInfo>{"value": "name", "display_name": "Name"},
                                    <AllowedValueInfo>{"value": "id", "display_name": "ID"},
                                ),
                            },
                        },
                    },
                    "fields[flow]": IgnoreOverlay,
                    "fields[segment]": IgnoreOverlay,
                    "fields[tag]": IgnoreOverlay,
                    "include": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "get_segment",
                "method": "GET",
                "path": "/api/segments/{id}",
                "operation_id": "get_segment",
                "display_name": "Get Segment",
                "short_desc": "Retrieves a specific segment",
                "desc": "Retrieves one segment by its Klaviyo ID.\n\n"
                    "`additionalFields` adds the segment's profile count and its `definition` - the rule "
                    "Klaviyo evaluates - neither of which is returned otherwise.",
                "groups": (GroupSegments,),
                "field_order": ("id", "additionalFields"),
                "response_unwrap": "data",
                "response_fields": SingleResourceResponse,
                "fields": {
                    "id": SegmentIdOverlay,
                    "additional-fields[segment]": AdditionalFieldsOverlay,
                    "fields[flow]": IgnoreOverlay,
                    "fields[segment]": IgnoreOverlay,
                    "fields[tag]": IgnoreOverlay,
                    "include": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "create_segment",
                "method": "POST",
                "path": "/api/segments",
                "operation_id": "create_segment",
                "display_name": "Create Segment",
                "short_desc": "Creates a new segment",
                "desc": "Creates a segment.\n\n"
                    "`definition` is the rule Klaviyo evaluates to decide who is in the segment, and it is "
                    "**required**: a segment with no rule would match nobody. It is Klaviyo's own condition "
                    "structure - a list of condition groups, each a list of conditions - and is documented "
                    "with the segments API rather than here.",
                "groups": (GroupSegments,),
                "request_unwrap": ("data", "attributes"),
                "field_order": ("name", "definition", "isStarred"),
                "response_unwrap": "data",
                "response_fields": SingleResourceResponse,
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "The name of the segment",
                        "preselected": True,
                    },
                    "definition": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Definition",
                        "short_desc": "The rule deciding who is in the segment",
                        "preselected": True,
                    },
                    "is_starred": <RestSchemaFieldOverlayInfo>{
                        "name": "isStarred",
                        "display_name": "Starred",
                        "short_desc": "Whether the segment is starred in the Klaviyo UI",
                    },
                    "fields[segment]": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "update_segment",
                "method": "PATCH",
                "path": "/api/segments/{id}",
                "operation_id": "update_segment",
                "display_name": "Update Segment",
                "short_desc": "Updates an existing segment",
                "desc": "Updates a segment that already exists, named by its Klaviyo ID.\n\n"
                    "Only the attributes supplied are changed. Replacing `definition` re-evaluates the "
                    "membership, which Klaviyo does in the background.",
                "groups": (GroupSegments,),
                "request_unwrap": ("data", "attributes"),
                "field_order": ("id", "name", "definition", "isStarred", "isActive"),
                "response_unwrap": "data",
                "response_fields": SingleResourceResponse,
                "fields": {
                    "path.id": SegmentIdOverlay,
                    "body.data.id": <RestSchemaFieldOverlayInfo>{"value_from": "id"},
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "The new name of the segment",
                        "preselected": True,
                    },
                    "definition": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Definition",
                        "short_desc": "The rule deciding who is in the segment",
                    },
                    "is_starred": <RestSchemaFieldOverlayInfo>{
                        "name": "isStarred",
                        "display_name": "Starred",
                        "short_desc": "Whether the segment is starred in the Klaviyo UI",
                    },
                    "is_active": <RestSchemaFieldOverlayInfo>{
                        "name": "isActive",
                        "display_name": "Active",
                        "short_desc": "Whether Klaviyo keeps the segment's membership up to date",
                    },
                    "fields[segment]": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete_segment",
                "method": "DELETE",
                "path": "/api/segments/{id}",
                "operation_id": "delete_segment",
                "display_name": "Delete Segment",
                "short_desc": "Deletes a segment",
                "desc": "Deletes a segment.\n\n"
                    "The profiles it matched are **not** deleted; the rule simply stops being evaluated. "
                    "Klaviyo answers with no content.",
                "groups": (GroupSegments,),
                "fields": {
                    "id": SegmentIdOverlay,
                    "revision": RevisionOverlay,
                },
            },
            #
            # Tags
            #
            <RestSchemaActionInfo>{
                "action": "list_tags",
                "method": "GET",
                "path": "/api/tags",
                "operation_id": "get_tags",
                "display_name": "List Tags",
                "short_desc": "Lists tags",
                "desc": "Lists the account's tags.\n\n"
                    "A tag groups campaigns, flows, lists and segments for reporting. Paging is by cursor.",
                "groups": (GroupTags,),
                "field_order": ("filter", "sort", "cursor"),
                "response_fields": CollectionResponse,
                "fields": {
                    "page[cursor]": CursorOverlay,
                    "page[size]": PageSizeOverlay,
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "codec": KlaviyoCodecs::TagFilter,
                        "display_name": "Filter",
                        "short_desc": "Narrow the listing to matching tags",
                        "desc": "Narrow the listing to matching tags",
                    },
                    "sort": SortOverlayBase + <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "field": <RestSchemaFieldOverlayInfo>{
                                "default_value": "name",
                                "allowed_values": (
                                    <AllowedValueInfo>{"value": "name", "display_name": "Name"},
                                    <AllowedValueInfo>{"value": "id", "display_name": "ID"},
                                ),
                            },
                        },
                    },
                    "fields[tag-group]": IgnoreOverlay,
                    "fields[tag]": IgnoreOverlay,
                    "include": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "get_tag",
                "method": "GET",
                "path": "/api/tags/{id}",
                "operation_id": "get_tag",
                "display_name": "Get Tag",
                "short_desc": "Retrieves a specific tag",
                "desc": "Retrieves one tag by its Klaviyo ID",
                "groups": (GroupTags,),
                "response_unwrap": "data",
                "response_fields": SingleResourceResponse,
                "fields": {
                    "id": TagIdOverlay,
                    "fields[tag-group]": IgnoreOverlay,
                    "fields[tag]": IgnoreOverlay,
                    "include": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "create_tag",
                "method": "POST",
                "path": "/api/tags",
                "operation_id": "create_tag",
                "display_name": "Create Tag",
                "short_desc": "Creates a new tag",
                "desc": "Creates a tag.\n\n"
                    "A tag groups campaigns, flows, lists and segments for reporting. Klaviyo places a new "
                    "tag in the account's default tag group unless one is named in the relationships, which "
                    "this action does not publish.",
                "groups": (GroupTags,),
                "request_unwrap": ("data", "attributes"),
                "field_order": ("name",),
                "response_unwrap": "data",
                "response_fields": SingleResourceResponse,
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "The name of the tag",
                        "preselected": True,
                    },
                    "fields[tag]": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "update_tag",
                "method": "PATCH",
                "path": "/api/tags/{id}",
                "operation_id": "update_tag",
                "display_name": "Update Tag",
                "short_desc": "Renames an existing tag",
                "desc": "Renames a tag.\n\n"
                    "A tag's name is the only attribute it has, so this action changes the name and nothing "
                    "else. Klaviyo answers with no content.",
                "groups": (GroupTags,),
                "request_unwrap": ("data", "attributes"),
                "field_order": ("id", "name"),
                "fields": {
                    "path.id": TagIdOverlay,
                    "body.data.id": <RestSchemaFieldOverlayInfo>{"value_from": "id"},
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "The new name of the tag",
                        "preselected": True,
                    },
                    "revision": RevisionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete_tag",
                "method": "DELETE",
                "path": "/api/tags/{id}",
                "operation_id": "delete_tag",
                "display_name": "Delete Tag",
                "short_desc": "Deletes a tag",
                "desc": "Deletes a tag.\n\n"
                    "The campaigns, flows, lists and segments it was applied to are **not** deleted; they "
                    "simply lose the tag. Klaviyo answers with no content.",
                "groups": (GroupTags,),
                "fields": {
                    "id": TagIdOverlay,
                    "revision": RevisionOverlay,
                },
            },
            #
            # Campaigns
            #
            <RestSchemaActionInfo>{
                "action": "list_campaigns",
                "method": "GET",
                "path": "/api/campaigns",
                "operation_id": "get_campaigns",
                "display_name": "List Campaigns",
                "short_desc": "Lists campaigns",
                "desc": "Lists campaigns on the given channel.\n\n"
                    "**`channel` is required**: Klaviyo does not list campaigns across channels, so a "
                    "listing is always of one channel's campaigns - or of both, if both are named. "
                    "Paging is by cursor.",
                "groups": (GroupCampaigns,),
                "field_order": ("channel", "name", "sort", "pageSize", "cursor"),
                "response_fields": CollectionResponse,
                "fields": {
                    "page[cursor]": CursorOverlay,
                    "page[size]": PageSizeOverlay,
                    # the campaign filter is published as the two options the superseded application
                    # offered, rather than as the one required string Klaviyo takes
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "codec": KlaviyoCodecs::CampaignFilter,
                        "lift": True,
                    },
                    "sort": SortOverlayBase + <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "field": <RestSchemaFieldOverlayInfo>{
                                "default_value": "created_at",
                                "allowed_values": (
                                    <AllowedValueInfo>{"value": "created_at", "display_name": "Created"},
                                    <AllowedValueInfo>{"value": "updated_at", "display_name": "Updated"},
                                    <AllowedValueInfo>{"value": "scheduled_at", "display_name": "Scheduled"},
                                    <AllowedValueInfo>{"value": "name", "display_name": "Name"},
                                    <AllowedValueInfo>{"value": "id", "display_name": "ID"},
                                ),
                            },
                        },
                    },
                    "fields[campaign-message]": IgnoreOverlay,
                    "fields[campaign]": IgnoreOverlay,
                    "fields[tag]": IgnoreOverlay,
                    "include": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "get_campaign",
                "method": "GET",
                "path": "/api/campaigns/{id}",
                "operation_id": "get_campaign",
                "display_name": "Get Campaign",
                "short_desc": "Retrieves a specific campaign",
                "desc": "Retrieves one campaign by its Klaviyo ID",
                "groups": (GroupCampaigns,),
                "response_unwrap": "data",
                "response_fields": SingleResourceResponse,
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Campaign",
                        "short_desc": "The campaign to retrieve",
                        "ref_data": RefCampaign,
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "fields[campaign-message]": IgnoreOverlay,
                    "fields[campaign]": IgnoreOverlay,
                    "fields[tag]": IgnoreOverlay,
                    "include": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "create_campaign",
                "method": "POST",
                "path": "/api/campaigns",
                "operation_id": "create_campaign",
                "display_name": "Create Campaign",
                "short_desc": "Creates a new campaign",
                "desc": "Creates a campaign as a **draft**; it is not sent until **Send Campaign** creates a "
                    "send job for it.\n\n"
                    "`audiences` names the lists and segments to include and exclude, and `campaignMessages` "
                    "carries the message content per channel; both are **required** and both are Klaviyo's "
                    "own structures, documented with the campaigns API rather than here.",
                "groups": (GroupCampaigns,),
                "request_unwrap": ("data", "attributes"),
                "field_order": ("name", "audiences", "campaignMessages", "sendStrategy", "sendOptions",
                    "trackingOptions"),
                "response_unwrap": "data",
                "response_fields": SingleResourceResponse,
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "The name of the campaign",
                        "preselected": True,
                    },
                    "audiences": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Audiences",
                        "short_desc": "The lists and segments to include and exclude",
                        "preselected": True,
                    },
                    "campaign-messages": <RestSchemaFieldOverlayInfo>{
                        "name": "campaignMessages",
                        "display_name": "Messages",
                        "short_desc": "The message content, per channel",
                        "preselected": True,
                    },
                    "send_strategy": <RestSchemaFieldOverlayInfo>{
                        "name": "sendStrategy",
                        "display_name": "Send Strategy",
                        "short_desc": "When the campaign is sent, if not immediately",
                    },
                    "send_options": <RestSchemaFieldOverlayInfo>{
                        "name": "sendOptions",
                        "display_name": "Send Options",
                        "short_desc": "Whether to use smart sending",
                    },
                    "tracking_options": <RestSchemaFieldOverlayInfo>{
                        "name": "trackingOptions",
                        "display_name": "Tracking Options",
                        "short_desc": "The UTM parameters and tracking flags to apply",
                    },
                    "fields[campaign]": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "update_campaign",
                "method": "PATCH",
                "path": "/api/campaigns/{id}",
                "operation_id": "update_campaign",
                "display_name": "Update Campaign",
                "short_desc": "Updates an existing campaign",
                "desc": "Updates a campaign that already exists, named by its Klaviyo ID.\n\n"
                    "Only the attributes supplied are changed. Klaviyo rejects a change to a campaign that "
                    "has already been sent.",
                "groups": (GroupCampaigns,),
                "request_unwrap": ("data", "attributes"),
                "field_order": ("id", "name", "audiences", "sendStrategy", "sendOptions", "trackingOptions"),
                "response_unwrap": "data",
                "response_fields": SingleResourceResponse,
                "fields": {
                    "path.id": CampaignIdOverlay,
                    "body.data.id": <RestSchemaFieldOverlayInfo>{"value_from": "id"},
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "The new name of the campaign",
                        "preselected": True,
                    },
                    "audiences": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Audiences",
                        "short_desc": "The lists and segments to include and exclude",
                    },
                    "send_strategy": <RestSchemaFieldOverlayInfo>{
                        "name": "sendStrategy",
                        "display_name": "Send Strategy",
                        "short_desc": "When the campaign is sent, if not immediately",
                    },
                    "send_options": <RestSchemaFieldOverlayInfo>{
                        "name": "sendOptions",
                        "display_name": "Send Options",
                        "short_desc": "Whether to use smart sending",
                    },
                    "tracking_options": <RestSchemaFieldOverlayInfo>{
                        "name": "trackingOptions",
                        "display_name": "Tracking Options",
                        "short_desc": "The UTM parameters and tracking flags to apply",
                    },
                    "fields[campaign]": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete_campaign",
                "method": "DELETE",
                "path": "/api/campaigns/{id}",
                "operation_id": "delete_campaign",
                "display_name": "Delete Campaign",
                "short_desc": "Deletes a campaign",
                "desc": "Deletes a campaign.\n\n"
                    "Klaviyo answers with no content. A campaign that has already been sent keeps its "
                    "reporting history.",
                "groups": (GroupCampaigns,),
                "fields": {
                    "id": CampaignIdOverlay,
                    "revision": RevisionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "send_campaign",
                "method": "POST",
                "path": "/api/campaign-send-jobs",
                "operation_id": "send_campaign",
                "display_name": "Send Campaign",
                "short_desc": "Sends a campaign",
                "desc": "Sends a campaign immediately, by creating a send job for it.\n\n"
                    "The campaign must be complete and ready to send; Klaviyo rejects a send job for a "
                    "campaign that is still a draft. The response carries the job, whose status can be "
                    "read back through `make-api-call` on `/api/campaign-send-jobs/{id}`.",
                "groups": (GroupCampaigns,),
                "request_unwrap": "data",
                "response_unwrap": "data",
                "response_fields": SingleResourceResponse,
                "fields": {
                    # the send job's "type" discriminator is filled in automatically: the schema fixes it
                    # to one value, so there is nothing to choose and publishing it would add a required
                    # option every existing call omits
                    "body.id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Campaign",
                        "short_desc": "The campaign to send",
                        "ref_data": RefCampaign,
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "fields[campaign-send-job]": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
            #
            # Events and metrics
            #
            <RestSchemaActionInfo>{
                "action": "list_events",
                "method": "GET",
                "path": "/api/events",
                "operation_id": "get_events",
                "display_name": "List Events",
                "short_desc": "Lists events",
                "desc": "Lists the events recorded in the account, newest first by default.\n\n"
                    "This listing is exported so that the event picker on other actions has values to "
                    "offer; the superseded application had no listing action of its own.",
                "groups": (GroupEvents,),
                "field_order": ("sort", "pageSize", "cursor"),
                "response_fields": CollectionResponse,
                "fields": {
                    "page[cursor]": CursorOverlay,
                    "page[size]": PageSizeOverlay,
                    "sort": SortOverlayBase + <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "field": <RestSchemaFieldOverlayInfo>{
                                "default_value": "datetime",
                                "allowed_values": (
                                    <AllowedValueInfo>{"value": "datetime", "display_name": "Date"},
                                    <AllowedValueInfo>{
                                        "value": "timestamp",
                                        "display_name": "Timestamp",
                                    },
                                ),
                            },
                        },
                    },
                    # published rather than ignored so that the events table can narrow a search Klaviyo
                    # would otherwise answer by walking every event in the account; it takes Klaviyo's own
                    # filter syntax, which the argument's own description spells out
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "short_desc": "Narrow the listing to matching events",
                        "desc": "Narrow the listing to matching events, in Klaviyo's filter syntax.\n\n"
                            "`equals(metric_id,\"…\")` and `equals(profile_id,\"…\")` select one metric or "
                            "one profile, and `datetime` and `timestamp` take the four range comparisons - "
                            "for example "
                            "`greater-or-equal(datetime,\"2026-01-01T00:00:00Z\"),less-than(datetime,\"2026-02-01T00:00:00Z\")`. "
                            "Predicates are joined by commas and mean **and**; Klaviyo has no `or` here.",
                    },
                    "fields[attribution]": IgnoreOverlay,
                    "fields[event]": IgnoreOverlay,
                    "fields[metric]": IgnoreOverlay,
                    "fields[profile]": IgnoreOverlay,
                    "include": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "get_event",
                "method": "GET",
                "path": "/api/events/{id}",
                "operation_id": "get_event",
                "display_name": "Get Event",
                "short_desc": "Retrieves a specific event",
                "desc": "Retrieves one event by its Klaviyo ID",
                "groups": (GroupEvents,),
                "response_unwrap": "data",
                "response_fields": SingleResourceResponse,
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Event",
                        "short_desc": "The event to retrieve",
                        "ref_data": RefEvent,
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "fields[attribution]": IgnoreOverlay,
                    "fields[event]": IgnoreOverlay,
                    "fields[metric]": IgnoreOverlay,
                    "fields[profile]": IgnoreOverlay,
                    "include": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "list_metrics",
                "method": "GET",
                "path": "/api/metrics",
                "operation_id": "get_metrics",
                "display_name": "List Metrics",
                "short_desc": "Lists metrics",
                "desc": "Lists the metrics defined in the account.\n\n"
                    "A **metric** is the kind of an event - \"Placed Order\", \"Viewed Product\" - and "
                    "Klaviyo defines one the first time an event names it. This listing is exported so "
                    "that the metric picker on **Create Event** and the **New Event** trigger has values "
                    "to offer; the superseded application had no listing action of its own.\n\n"
                    "Klaviyo declares no page size on this endpoint, only a cursor.",
                "groups": (GroupEvents,),
                "response_fields": CollectionResponse,
                "fields": {
                    "page[cursor]": CursorOverlay,
                    "filter": IgnoreOverlay,
                    "fields[flow]": IgnoreOverlay,
                    "fields[metric]": IgnoreOverlay,
                    "include": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "get_metric",
                "method": "GET",
                "path": "/api/metrics/{id}",
                "operation_id": "get_metric",
                "display_name": "Get Metric",
                "short_desc": "Retrieves a specific metric",
                "desc": "Retrieves one metric by its Klaviyo ID.\n\n"
                    "A **metric** is the kind of an event - \"Placed Order\", \"Viewed Product\" - and "
                    "Klaviyo defines one the first time an event names it.",
                "groups": (GroupEvents,),
                "response_unwrap": "data",
                "response_fields": SingleResourceResponse,
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Metric",
                        "short_desc": "The metric to retrieve",
                        "ref_data": RefMetricId,
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "fields[flow]": IgnoreOverlay,
                    "fields[metric]": IgnoreOverlay,
                    "include": IgnoreOverlay,
                    "revision": RevisionOverlay,
                },
            },
        );

        #! The dropdowns this application's options offer
        const ReferenceData = {
            RefProfile: <RestSchemaReferenceDataInfo>{
                "name": RefProfile,
                "action": "list_profiles",
                "options": {"pageSize": RefPageSize},
                "records_path": "data",
                "value_field": "id",
                "display_field": "email",
                "desc": "The profiles in the account, newest first",
            },
            RefList: <RestSchemaReferenceDataInfo>{
                "name": RefList,
                "action": "list_lists",
                "records_path": "data",
                "value_field": "id",
                "display_field": "name",
                "desc": "The lists in the account",
            },
            RefSegment: <RestSchemaReferenceDataInfo>{
                "name": RefSegment,
                "action": "list_segments",
                "records_path": "data",
                "value_field": "id",
                "display_field": "name",
                "desc": "The segments in the account",
            },
            RefTag: <RestSchemaReferenceDataInfo>{
                "name": RefTag,
                "action": "list_tags",
                "records_path": "data",
                "value_field": "id",
                "display_field": "name",
                "desc": "The tags in the account",
            },
            RefCampaign: <RestSchemaReferenceDataInfo>{
                "name": RefCampaign,
                "action": "list_campaigns",
                # Klaviyo accepts only "equals" on the message channel, so an account's campaigns are
                # reachable one channel at a time; a picker offering only e-mail campaigns would be missing
                # half the account, so this reads both and merges them - which is what the superseded
                # application did by hand
                "options": {"pageSize": RefPageSize},
                "option_sets": (
                    {"channel": "email"},
                    {"channel": "sms"},
                ),
                "records_path": "data",
                "value_field": "id",
                "display_field": "name",
                "desc": "The account's campaigns, on both channels",
            },
            RefMetricName: <RestSchemaReferenceDataInfo>{
                "name": RefMetricName,
                "action": "list_metrics",
                "records_path": "data",
                "value_field": "name",
                "display_field": "name",
                "desc": "The metrics defined in the account, by name",
            },
            RefMetricId: <RestSchemaReferenceDataInfo>{
                "name": RefMetricId,
                "action": "list_metrics",
                "records_path": "data",
                "value_field": "id",
                "display_field": "name",
                "desc": "The metrics defined in the account, by ID",
            },
            RefEvent: <RestSchemaReferenceDataInfo>{
                "name": RefEvent,
                "action": "list_events",
                "options": {"pageSize": RefPageSize},
                "records_path": "data",
                "value_field": "id",
                "display_field": "datetime",
                "desc": "The events recorded in the account, newest first",
            },
        };
    }
}
}
