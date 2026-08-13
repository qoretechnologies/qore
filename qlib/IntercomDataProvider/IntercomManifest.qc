# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore IntercomDataProvider module definition

/*  IntercomManifest.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
#! The operations the Intercom application exports, and how they are presented
/** @section intercommanifest_ids Action IDs

    The action IDs are those of the TypeScript application this module replaces, read from its i18n
    catalog rather than derived from a filename or an \c operationId - an action ID is a product commitment,
    and existing workflows call these names.

    Intercom is the unusually comfortable case here: **all 15 IDs are the \c operationId the vendor's own
    description declares**, verbatim, including two that look like mistakes and are not ours to fix:

    |!ID|!Note
    |\c lisDataEvents|the typo is Intercom's, in its own description; it ships as \c lisDataEvents
    |\c SearchContacts|capitalised unlike every one of its peers, again as Intercom spells it

    So ID preservation costs nothing here, and each is recorded as \c operation_id as well, so an upstream
    rename surfaces as drift rather than silently renaming an action.

    @section intercommanifest_added What This Application Adds

    Four read-only listings are exported that the TypeScript application did not.  Every one of them exists
    so that a dropdown has an action to call: a dropdown can only name an action the manifest exports, and
    the application this module replaces filled its dropdowns by calling endpoints it never exported.

    |!Action|!Fills
    |\c listAdmins|the teammate dropdowns - an article's author, a note's or a reply's admin, a message's sender
    |\c ListContacts|the contact dropdowns, and the contact e-mail and external-ID dropdowns with them
    |\c listAllCollections|the parent-collection dropdown on **Create Article**
    |\c listTagsForAContact|the tag dropdown on **Remove Tag from Contact**, which is scoped to the contact

    @section intercommanifest_conversations The Conversation Dropdown Uses the Search

    \c "GET /conversations" is **not** exported, and the conversation dropdown is filled by
    \c searchConversations instead.  Intercom does not grant an app permission to that listing by default
    and answers **403** without it, while the search returns the same conversations to the same credential -
    measured against a live workspace, where the listing failed and the search returned 28.

    A listing exported only to fill a dropdown, which then cannot be read by a normally configured app, is
    a name that does not work; the search is the endpoint that does.

    @section intercommanifest_sections The Section Dropdown Is Withdrawn

    The superseded application offered a **Section** dropdown for an article's parent, backed by
    \c "GET /help_center/sections".  **Intercom removed that endpoint at API version 2.10**: its 2.9
    description declares \c /help_center/sections and \c /help_center/sections/{id}, and no description
    from 2.10 onward declares either.  Sections became nested collections, and Intercom returns what used
    to be a section as a collection.

    So the dropdown is not reproduced, and \c parent_type is narrowed to \c collection - the one value that
    can still be resolved.  This is a withdrawal rather than a gap: the helper it replaces has been calling
    a removed endpoint against every workspace on 2.10 or later, which is all of them by now.

    @section intercommanifest_version The Revision Header

    Every operation in Intercom's description declares an optional \c Intercom-Version header.  It is the
    connection's business rather than each action's - see
    @ref IntercomRestClient::IntercomRestClientBase "IntercomRestClientBase" - so every action drops it with
    @ref RestSchemaActions::RestSchemaFieldOverlayInfo::ignore "ignore" and the client's default headers
    fill it in when the request is built.

    @section intercommanifest_timestamps Timestamps Are Unix Seconds

    Intercom writes every timestamp as an integer count of seconds since the epoch, so the fields that take
    one carry the @ref RestSchemaActions::RestSchemaActionCodecs::UnixSeconds "unix-seconds" codec and are
    presented as %Qore @ref date "date" values.  This is the Stripe case rather than the Paddle one, where
    timestamps are RFC 3339 strings and no codec is wanted.

    @section intercommanifest_paging Paging

    Intercom pages a listing with a cursor: the response carries \c pages.next.starting_after, and passing
    that value back as \c starting_after returns the following page.  \c pages is absent, rather than
    holding an empty cursor, once the collection is exhausted.

    \c "POST /companies/list" is the exception - it takes \c page and \c per_page and reports
    \c pages.total_pages - and \c "GET /tags", \c "GET /admins" and \c "GET /contacts/{contact_id}/tags" are
    not paged at all, returning the whole collection in one response.

    @section intercommanifest_ids_typed One Identifier, Two Types

    Intercom's description types an admin's own \c id as a \c string on the admin object and as an
    \c integer in the bodies that reference it, and does the same for an article's \c parent_id against a
    collection's \c string \c id.  A dropdown filled from the listing therefore supplies a string to an
    integer option.

    The values really are numeric, so those options declare a soft type: the dropdown's string is accepted
    and the integer Intercom's description asks for is what gets sent.  This is a type presentation
    override and not a schema repair - the request contract is unchanged.
*/
public class IntercomManifest {
    public {
        #! Action group: contacts
        const GroupContacts = "Contacts";

        #! Action group: companies
        const GroupCompanies = "Companies";

        #! Action group: conversations
        const GroupConversations = "Conversations";

        #! Action group: tags
        const GroupTags = "Tags";

        #! Action group: help center articles
        const GroupArticles = "Articles";

        #! Action group: data events
        const GroupEvents = "Data Events";

        #! Reference data name: contacts, by Intercom identity
        const RefContact = "intercom-contact";

        #! Reference data name: contacts, by e-mail address
        const RefContactEmail = "intercom-contact-email";

        #! Reference data name: contacts, by the caller's own external identity
        const RefContactExternalId = "intercom-contact-external-id";

        #! Reference data name: teammates
        const RefAdmin = "intercom-admin";

        #! Reference data name: companies, by the caller's own company identity
        const RefCompany = "intercom-company";

        #! Reference data name: conversations
        const RefConversation = "intercom-conversation";

        #! Reference data name: tags
        const RefTag = "intercom-tag";

        #! Reference data name: the tags applied to one contact
        const RefContactTag = "intercom-contact-tag";

        #! Reference data name: help center collections
        const RefCollection = "intercom-collection";

        #! The largest page Intercom returns for a cursor-paged listing
        const MaxPageSize = IntercomRestClient::IntercomRestClientBase::MaxPageSize;

        #! The largest page \c "POST /companies/list" accepts
        /** The one listing Intercom pages by number rather than by cursor is also the one with a smaller
            cap: it answers **"Per Page is too big"** above 60, where the cursor-paged listings take 150.
            The description declares no maximum on either, so this was measured against the API - asking
            for 150 here made the company dropdown fail outright with a 400.
        */
        const MaxCompanyPageSize = 60;

        #! The dropdowns the application offers
        /** Each is produced by executing a listing action from this same manifest, so a dropdown can never
            drift away from an endpoint the application actually exports.

            Intercom names the array in a list response after what it holds rather than always calling it
            \c data - \c admins for teammates, \c conversations for conversations - so each declares its own
            \c records_path.
        */
        const ReferenceData = {
            RefContact: <RestSchemaReferenceDataInfo>{
                "name": RefContact,
                "action": "ListContacts",
                "options": {"per_page": MaxPageSize},
                "records_path": "data",
                "value_field": "id",
                "display_field": "name",
                "desc": "contacts in this workspace, by the identity Intercom gave them",
            },
            RefContactEmail: <RestSchemaReferenceDataInfo>{
                "name": RefContactEmail,
                "action": "ListContacts",
                "options": {"per_page": MaxPageSize},
                "records_path": "data",
                "value_field": "email",
                "display_field": "name",
                "desc": "contacts in this workspace, by e-mail address",
            },
            RefContactExternalId: <RestSchemaReferenceDataInfo>{
                "name": RefContactExternalId,
                "action": "ListContacts",
                "options": {"per_page": MaxPageSize},
                "records_path": "data",
                "value_field": "external_id",
                "display_field": "name",
                "desc": "contacts in this workspace, by the external identity your own system gave them",
            },
            RefAdmin: <RestSchemaReferenceDataInfo>{
                "name": RefAdmin,
                "action": "listAdmins",
                # a teammate listing is not paged and names its array "admins"
                "records_path": "admins",
                "value_field": "id",
                "display_field": "name",
                "desc": "teammates with access to this workspace",
            },
            RefCompany: <RestSchemaReferenceDataInfo>{
                "name": RefCompany,
                "action": "listAllCompanies",
                # not MaxPageSize: this endpoint rejects anything above 60
                "options": {"per_page": MaxCompanyPageSize},
                "records_path": "data",
                # "company_id" is the identity the caller gave the company, which is what the create and
                # update operation matches on; Intercom's own "id" addresses a company in a URL and is not
                # interchangeable with it
                "value_field": "company_id",
                "display_field": "name",
                "desc": "companies in this workspace, by the company ID your own system gave them",
            },
            RefConversation: <RestSchemaReferenceDataInfo>{
                "name": RefConversation,
                # deliberately the search rather than "GET /conversations": that listing needs a permission
                # Intercom does not grant an app by default and answers **403** without it, while the search
                # returns the same conversations to the same credential.  Measured against a live workspace,
                # where the listing failed and this succeeded
                "action": "searchConversations",
                "options": {
                    "query": {"field": "created_at", "operator": ">", "value": 0},
                    "pagination": {"per_page": MaxPageSize},
                },
                "records_path": "conversations",
                "value_field": "id",
                # a conversation often has no title, in which case the ID identifies it
                "display_field": "title",
                "desc": "conversations in this workspace",
            },
            RefTag: <RestSchemaReferenceDataInfo>{
                "name": RefTag,
                "action": "listTags",
                # a tag listing is not paged
                "records_path": "data",
                "value_field": "id",
                "display_field": "name",
                "desc": "tags defined in this workspace",
            },
            RefContactTag: <RestSchemaReferenceDataInfo>{
                "name": RefContactTag,
                "action": "listTagsForAContact",
                # a tag is removed from the contact it is on, so the choice is the tags that contact
                # actually has - not every tag in the workspace, most of which would fail
                "options_from": {"contact_id": "contact_id"},
                "records_path": "data",
                "value_field": "id",
                "display_field": "name",
                "desc": "the tags applied to the selected contact",
            },
            RefCollection: <RestSchemaReferenceDataInfo>{
                "name": RefCollection,
                "action": "listAllCollections",
                "options": {"per_page": MaxPageSize},
                "records_path": "data",
                "value_field": "id",
                "display_field": "name",
                "desc": "help center collections in this workspace",
            },
        };

        #! Overlay dropping the revision header, which the connection owns
        /** Naming the option is what lets a connection that does not send the header be reported as such,
            instead of as a schema type error about a header no action offers.
        */
        const VersionHeaderOverlay = <RestSchemaFieldOverlayInfo>{
            "ignore": True,
            "connection_option": "headers",
        };

        #! Overlay applied to every action, dropping the revision header
        const Common = {
            "Intercom-Version": VersionHeaderOverlay,
        };

        #! Overlay for the contact an action works with
        const ContactOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Contact",
            "short_desc": "The contact this applies to",
            "ref_data": RefContact,
            "supports_custom_values": True,
            "preselected": True,
        };

        #! Overlay for the teammate an action acts as
        /** Intercom types an admin's own \c id as a string and the references to it as integers; the value
            is numeric either way, so a soft type accepts what the dropdown supplies and sends what the
            description asks for.
        */
        const AdminOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Teammate",
            "short_desc": "The teammate to act as",
            "ref_data": RefAdmin,
            "supports_custom_values": True,
            "preselected": True,
            "type": AbstractDataProviderTypeMap."softint",
        };

        #! Overlay for the teammate an action acts as, where the description types it as a string
        const AdminStringOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Teammate",
            "short_desc": "The teammate to act as",
            "ref_data": RefAdmin,
            "supports_custom_values": True,
            "preselected": True,
        };

        #! Overlay for the tag an action works with
        const TagOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Tag",
            "short_desc": "The tag to apply",
            "ref_data": RefTag,
            "supports_custom_values": True,
            "preselected": True,
        };

        #! Overlay for the conversation an action works on
        const ConversationOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Conversation",
            "short_desc": "The conversation this applies to",
            "ref_data": RefConversation,
            "supports_custom_values": True,
            "preselected": True,
        };

        #! Overlay for a timestamp Intercom writes as a count of seconds since the epoch
        const TimestampOverlay = <RestSchemaFieldOverlayInfo>{
            "codec": RestSchemaActionCodecs::UnixSeconds,
        };

        #! Overlay for the cursor of a cursor-paged listing
        const StartingAfterOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Starting After",
            "short_desc": "The cursor for the next page",
            "desc": "The pagination cursor: take `pages.next.starting_after` out of the previous response "
                "and pass it here.\n\n"
                "It is opaque and must not be constructed by hand. `pages` is absent once the collection "
                "is exhausted, which is what says there is no next page.",
        };

        #! Overlay for the page size of a cursor-paged listing
        const PerPageOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Page Size",
            "short_desc": "How many records to return in one page",
            "preselected": True,
            "example_value": MaxPageSize,
        };

        #! Overlay applied to the paging options of a cursor-paged listing
        const CursorPagingOverlay = {
            "per_page": PerPageOverlay,
            "starting_after": StartingAfterOverlay,
        };

        #! The operations the application exports
        const Manifest = (
            # --- Contacts ---
            <RestSchemaActionInfo>{
                "action": "SearchContacts",
                "method": "POST",
                "path": "/contacts/search",
                "operation_id": "SearchContacts",
                "display_name": "Search Contacts",
                "short_desc": "Find contacts matching a query",
                "desc": "Searches the workspace's contacts.\n\n"
                    "`query` is either a single filter - `{field, operator, value}` - or a boolean "
                    "combination, `{operator: \"AND\", value: [ ... ]}`, nested as deeply as needed. "
                    "A field Intercom does not define on a contact is searched by naming it "
                    "`custom_attributes.<name>`.\n\n"
                    "Timestamps in a query are counts of seconds since the epoch, so "
                    "`created_at > 1755000000` reads as \"created after that instant\".\n\n"
                    "The result is paged with `pagination`: pass back the `pages.next.starting_after` "
                    "value from the previous response.",
                "groups": (GroupContacts,),
                "field_order": ("query", "pagination", "sort", "include_merge_history"),
                "fields": Common,
            },
            <RestSchemaActionInfo>{
                "action": "ListContacts",
                "method": "GET",
                "path": "/contacts",
                "operation_id": "ListContacts",
                "display_name": "List Contacts",
                "short_desc": "List the contacts in the workspace",
                "desc": "Lists contacts, newest first.\n\n"
                    "This is exported so that the contact dropdowns have an action to call; **Search "
                    "Contacts** is the one to reach for when the contacts wanted are a subset.\n\n"
                    "Intercom's description declares no paging arguments for this operation even though it "
                    "answers a paged envelope; `per_page` and `starting_after` are added when the schema is "
                    "imported. See @ref IntercomDataProvider::IntercomSchema \"IntercomSchema\".",
                "groups": (GroupContacts,),
                "field_order": ("per_page", "starting_after", "include_merge_history"),
                "fields": Common + CursorPagingOverlay,
            },
            <RestSchemaActionInfo>{
                "action": "createNote",
                "method": "POST",
                "path": "/contacts/{contact_id}/notes",
                "operation_id": "createNote",
                "display_name": "Create Note",
                "short_desc": "Add a note to a contact",
                "desc": "Adds a note to a contact.\n\n"
                    "A note is internal: it is visible to teammates on the contact's record and is never "
                    "shown to the contact. Use **Reply to Conversation** to say something the contact "
                    "sees.\n\n"
                    "Without `admin_id` the note is attributed to no teammate.",
                "groups": (GroupContacts,),
                "field_order": ("contact_id", "body", "admin_id"),
                "fields": Common + {
                    # Intercom's description types this one path variable as an integer and types the
                    # same value as a string on every other operation that takes it - including the two
                    # tag operations on the very next path.  A contact ID is a 24-character hex string,
                    # which this document's own example *for this operation* spells out as
                    # "contact_id": "6762f0ad1bb69f9f2193bb62", so the declaration contradicts the
                    # example beside it.  Left alone the option would reject every real contact ID.
                    "contact_id": ContactOverlay + <RestSchemaFieldOverlayInfo>{
                        "type": AbstractDataProviderTypeMap."softstring",
                    },
                    "admin_id": AdminStringOverlay + <RestSchemaFieldOverlayInfo>{
                        "short_desc": "The teammate the note is attributed to",
                        "preselected": False,
                    },
                    "body": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Note",
                        "short_desc": "The text of the note",
                        "required": True,
                        "preselected": True,
                    },
                },
            },

            # --- Tags ---
            <RestSchemaActionInfo>{
                "action": "listTags",
                "method": "GET",
                "path": "/tags",
                "operation_id": "listTags",
                "display_name": "List Tags",
                "short_desc": "List the tags defined in the workspace",
                "desc": "Lists every tag defined in the workspace.\n\n"
                    "This listing is not paged: Intercom returns the whole set in one response.",
                "groups": (GroupTags,),
                "fields": Common,
            },
            <RestSchemaActionInfo>{
                "action": "listTagsForAContact",
                "method": "GET",
                "path": "/contacts/{contact_id}/tags",
                "operation_id": "listTagsForAContact",
                "display_name": "List Tags on Contact",
                "short_desc": "List the tags applied to one contact",
                "desc": "Lists the tags applied to a contact.\n\n"
                    "This is exported so that **Remove Tag from Contact** can offer the tags that contact "
                    "actually has, rather than every tag in the workspace - almost all of which would fail.\n\n"
                    "This listing is not paged.",
                "groups": (GroupTags,),
                "field_order": ("contact_id",),
                "fields": Common + {
                    "contact_id": ContactOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "attachTagToContact",
                "method": "POST",
                "path": "/contacts/{contact_id}/tags",
                "operation_id": "attachTagToContact",
                "display_name": "Add Tag to Contact",
                "short_desc": "Apply an existing tag to a contact",
                "desc": "Applies a tag to a contact.\n\n"
                    "The tag has to exist already: this attaches one, it does not create one.",
                "groups": (GroupTags,),
                "field_order": ("contact_id", "id"),
                "fields": Common + {
                    "contact_id": ContactOverlay,
                    "id": TagOverlay + <RestSchemaFieldOverlayInfo>{
                        "required": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "detachTagFromContact",
                "method": "DELETE",
                "path": "/contacts/{contact_id}/tags/{tag_id}",
                "operation_id": "detachTagFromContact",
                "display_name": "Remove Tag from Contact",
                "short_desc": "Remove a tag from a contact",
                "desc": "Removes a tag from a contact.\n\n"
                    "The tag itself is untouched; only its application to this contact is removed.\n\n"
                    "The tag dropdown lists the tags the selected contact actually has, so choose the "
                    "contact first.",
                "groups": (GroupTags,),
                "field_order": ("contact_id", "tag_id"),
                "fields": Common + {
                    "contact_id": ContactOverlay + <RestSchemaFieldOverlayInfo>{
                        "has_dependents": True,
                        "on_change": ("refetch",),
                    },
                    "tag_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Tag",
                        "short_desc": "The tag to remove from this contact",
                        "ref_data": RefContactTag,
                        "depends_on": ("contact_id",),
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
            },

            # --- Companies ---
            <RestSchemaActionInfo>{
                "action": "createOrUpdateCompany",
                "method": "POST",
                "path": "/companies",
                "operation_id": "createOrUpdateCompany",
                "display_name": "Create or Update Company",
                "short_desc": "Create a company, or update the one with this company ID",
                "desc": "Creates a company, or updates the existing one with the same `company_id`.\n\n"
                    "`company_id` is **your** identifier for the company, not Intercom's: it is what "
                    "decides whether this creates or updates, and Intercom will not let it be changed "
                    "afterwards. The dropdown therefore lists companies by that identifier.\n\n"
                    "Only the fields sent are changed; the rest are left as they are.",
                "groups": (GroupCompanies,),
                "field_order": ("company_id", "name", "website", "industry", "size", "plan",
                    "monthly_spend", "custom_attributes", "remote_created_at"),
                "fields": Common + {
                    "company_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Company ID",
                        "short_desc": "Your own identifier for the company",
                        "ref_data": RefCompany,
                        "supports_custom_values": True,
                        "required": True,
                        "preselected": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "preselected": True,
                    },
                    "remote_created_at": TimestampOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Created At",
                        "short_desc": "When the company was created in your own system",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "listAllCompanies",
                "method": "POST",
                "path": "/companies/list",
                "operation_id": "listAllCompanies",
                "display_name": "List Companies",
                "short_desc": "List the companies in the workspace",
                "desc": "Lists every company in the workspace.\n\n"
                    "This is the one listing Intercom pages by **page number** rather than by cursor: "
                    "`pages.total_pages` says how many there are, and `page` selects one.\n\n"
                    "It is a `POST` because Intercom made it one; it sends no body.",
                "groups": (GroupCompanies,),
                "field_order": ("page", "per_page", "order"),
                "fields": Common + {
                    "per_page": PerPageOverlay,
                    "page": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page",
                        "short_desc": "Which page to return, starting at 1",
                        "preselected": True,
                    },
                },
            },

            # --- Conversations ---
            <RestSchemaActionInfo>{
                "action": "createConversation",
                "method": "POST",
                "path": "/conversations",
                "operation_id": "createConversation",
                "display_name": "Create Conversation",
                "short_desc": "Start a conversation as a contact",
                "desc": "Starts a conversation **from a contact** - as though the contact had written "
                    "in.\n\n"
                    "This is not the way to message somebody: a conversation created here is attributed to "
                    "the contact, so it appears in the inbox as an incoming message. Use **Create "
                    "Message** to send something from a teammate.",
                "groups": (GroupConversations,),
                "field_order": ("from", "body", "subject", "attachment_urls", "created_at"),
                "fields": Common + {
                    "from": <RestSchemaFieldOverlayInfo>{
                        "display_name": "From",
                        "short_desc": "The contact the conversation comes from",
                        "required": True,
                        "preselected": True,
                        "fields": {
                            "type": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Contact Type",
                                "short_desc": "Whether the conversation comes from a user or a lead",
                                "default_value": "user",
                            },
                            "id": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Contact",
                                "short_desc": "The contact the conversation comes from",
                                "ref_data": RefContact,
                                "supports_custom_values": True,
                            },
                        },
                    },
                    "body": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Message",
                        "short_desc": "The first message in the conversation",
                        "required": True,
                        "preselected": True,
                    },
                    "created_at": TimestampOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "searchConversations",
                "method": "POST",
                "path": "/conversations/search",
                "operation_id": "searchConversations",
                "display_name": "Search Conversations",
                "short_desc": "Find conversations matching a query",
                "desc": "Searches the workspace's conversations.\n\n"
                    "`query` takes the same shape as **Search Contacts**: a single "
                    "`{field, operator, value}` filter, or `{operator: \"AND\", value: [ ... ]}` "
                    "combining several.\n\n"
                    "Timestamps in a query are counts of seconds since the epoch.",
                "groups": (GroupConversations,),
                "field_order": ("query", "pagination", "include_monitors", "include_scorecards"),
                "fields": Common,
            },
            <RestSchemaActionInfo>{
                "action": "createMessage",
                "method": "POST",
                "path": "/messages",
                "operation_id": "createMessage",
                "display_name": "Create Message",
                "short_desc": "Send a message to a contact from a teammate",
                "desc": "Sends a message to a contact **from a teammate**, starting a new conversation.\n\n"
                    "`message_type` selects how it is delivered: `email` needs a `subject`, `in_app` shows "
                    "it in the messenger, and `whatsapp` needs an approved `template`.\n\n"
                    "Use **Reply to Conversation** to add to a conversation that already exists.",
                "groups": (GroupConversations,),
                "field_order": ("message_type", "from", "to", "subject", "body", "template",
                    "created_at"),
                "fields": Common + {
                    "from": <RestSchemaFieldOverlayInfo>{
                        "display_name": "From",
                        "short_desc": "The teammate the message is from",
                        "preselected": True,
                        "fields": {
                            "id": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Teammate",
                                "short_desc": "The teammate the message is from",
                                "ref_data": RefAdmin,
                                "supports_custom_values": True,
                            },
                        },
                    },
                    "to": <RestSchemaFieldOverlayInfo>{
                        "display_name": "To",
                        "short_desc": "The contact to send the message to",
                        "preselected": True,
                    },
                    "body": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Message",
                        "preselected": True,
                    },
                    "created_at": TimestampOverlay,
                },
            },

            # --- Data events ---
            <RestSchemaActionInfo>{
                "action": "createDataEvent",
                "method": "POST",
                "path": "/events",
                "operation_id": "createDataEvent",
                "display_name": "Submit Data Event",
                "short_desc": "Record that a contact did something",
                "desc": "Records a data event against a contact - a signup, a purchase, a feature used.\n\n"
                    "The contact is named in exactly one of three ways: `id` is Intercom's identity for "
                    "them, `user_id` is the external identity your own system gave them, and `email` is "
                    "their address. Supply one.\n\n"
                    "`metadata` carries up to five values describing the event itself.",
                "groups": (GroupEvents,),
                "field_order": ("event_name", "created_at", "id", "user_id", "email", "metadata"),
                "fields": Common + {
                    "event_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Event Name",
                        "short_desc": "The name of the event being recorded",
                        "required": True,
                        "preselected": True,
                    },
                    # Intercom requires this in every one of the request body's three branches, so an
                    # event submitted without it is rejected.  The application this module replaces hid
                    # that by quietly filling in the current time; declaring it required says so at
                    # configuration time instead of failing at the vendor with a 400.
                    "created_at": TimestampOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Occurred At",
                        "short_desc": "When the event happened",
                        "desc": "When the event happened.\n\n"
                            "Intercom requires this. Set it to the current time for an event happening "
                            "now; the application this module replaces did that silently, so a workflow "
                            "migrated from it has to say so explicitly.",
                        "required": True,
                        "preselected": True,
                    },
                    "id": ContactOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Contact",
                        "short_desc": "The contact, by Intercom's identity for them",
                        "required_groups": ("contact",),
                    },
                    "user_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "External ID",
                        "short_desc": "The contact, by the identity your own system gave them",
                        "ref_data": RefContactExternalId,
                        "supports_custom_values": True,
                        "required_groups": ("contact",),
                    },
                    "email": <RestSchemaFieldOverlayInfo>{
                        "display_name": "E-mail",
                        "short_desc": "The contact, by e-mail address",
                        "ref_data": RefContactEmail,
                        "supports_custom_values": True,
                        "required_groups": ("contact",),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "lisDataEvents",
                "method": "GET",
                "path": "/events",
                "operation_id": "lisDataEvents",
                "display_name": "List Data Events",
                "short_desc": "Summarise the data events recorded for one contact",
                "desc": "Returns a summary of the data events recorded for **one contact**.\n\n"
                    "This is not a listing of the workspace's events: `filter` names the contact - by "
                    "`user_id`, `intercom_user_id` or `email` - and `type` must be `user`. What comes back "
                    "is that contact's event summary, each event with its name, first and last occurrence "
                    "and a count.\n\n"
                    "The action ID keeps Intercom's own spelling of `lisDataEvents`.",
                "groups": (GroupEvents,),
                "field_order": ("filter", "type", "summary"),
                "fields": Common + {
                    "type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Type",
                        "short_desc": "Must be \"user\"",
                        "default_value": "user",
                        "allowed_values": ("user",),
                        "preselected": True,
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Contact",
                        "short_desc": "Which contact's events to summarise",
                        "desc": "Names the contact whose events are summarised, in exactly one of three "
                            "ways: `user_id` is the identity your own system gave them, "
                            "`intercom_user_id` is Intercom's, and `email` is their address.",
                        "preselected": True,
                    },
                },
            },

            # --- Articles ---
            <RestSchemaActionInfo>{
                "action": "listArticles",
                "method": "GET",
                "path": "/articles",
                "operation_id": "listArticles",
                "display_name": "List Articles",
                "short_desc": "List the help center articles",
                "desc": "Lists the workspace's help center articles.\n\n"
                    "Intercom's description declares no paging arguments for this operation even though it "
                    "answers a paged envelope; `per_page` and `starting_after` are added when the schema is "
                    "imported. See @ref IntercomDataProvider::IntercomSchema \"IntercomSchema\".",
                "groups": (GroupArticles,),
                "field_order": ("per_page", "starting_after"),
                "fields": Common + CursorPagingOverlay,
            },
            <RestSchemaActionInfo>{
                "action": "listAllCollections",
                "method": "GET",
                "path": "/help_center/collections",
                "operation_id": "listAllCollections",
                "display_name": "List Collections",
                "short_desc": "List the help center collections",
                "desc": "Lists the help center collections articles can be filed under.\n\n"
                    "This is exported so that **Create Article**'s parent dropdown has an action to "
                    "call.\n\n"
                    "Collections nest: a collection with a `parent_id` sits inside another one. What "
                    "earlier API versions called a *section* is returned here as a nested collection - "
                    "Intercom removed sections at version 2.10.",
                "groups": (GroupArticles,),
                "field_order": ("per_page", "starting_after"),
                "fields": Common + CursorPagingOverlay,
            },
            <RestSchemaActionInfo>{
                "action": "createArticle",
                "method": "POST",
                "path": "/articles",
                "operation_id": "createArticle",
                "display_name": "Create Article",
                "short_desc": "Write a help center article",
                "desc": "Creates a help center article.\n\n"
                    "`author_id` has to be a teammate on the help center's workspace. `state` decides "
                    "whether it is published or left as a draft - Intercom defaults it to `draft`.\n\n"
                    "`parent_id` files the article under a collection. Earlier API versions also allowed a "
                    "*section*; Intercom removed sections at version 2.10 and returns what used to be one "
                    "as a nested collection, so `collection` is the only parent type left.",
                "groups": (GroupArticles,),
                "field_order": ("title", "body", "author_id", "state", "parent_type", "parent_id",
                    "translated_content"),
                "fields": Common + {
                    "author_id": AdminOverlay + <RestSchemaFieldOverlayInfo>{
                        "display_name": "Author",
                        "short_desc": "The teammate the article is written by",
                        "required": True,
                    },
                    "title": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Title",
                        "required": True,
                        "preselected": True,
                    },
                    "body": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Body",
                        "short_desc": "The article content, in HTML",
                        "required": True,
                        "preselected": True,
                    },
                    "parent_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Parent Type",
                        "short_desc": "Must be \"collection\"",
                        "desc": "The kind of thing `parent_id` names.\n\n"
                            "Only `collection` remains: Intercom removed help center sections at API "
                            "version 2.10, and returns what used to be a section as a nested collection.",
                        "allowed_values": ("collection",),
                        "default_value": "collection",
                        "has_dependents": True,
                        "on_change": ("refetch",),
                    },
                    "parent_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Collection",
                        "short_desc": "The collection to file the article under",
                        "ref_data": RefCollection,
                        "depends_on": ("parent_type",),
                        "supports_custom_values": True,
                        "type": AbstractDataProviderTypeMap."softint",
                    },
                },
            },

            # --- Teammates ---
            <RestSchemaActionInfo>{
                "action": "listAdmins",
                "method": "GET",
                "path": "/admins",
                "operation_id": "listAdmins",
                "display_name": "List Teammates",
                "short_desc": "List the teammates with access to the workspace",
                "desc": "Lists the teammates who have access to the workspace.\n\n"
                    "This is exported so that the teammate dropdowns have an action to call - an article's "
                    "author, a note's or a reply's teammate, a message's sender.\n\n"
                    "This listing is not paged: Intercom returns them all in one response, in an `admins` "
                    "array.",
                "groups": (GroupContacts,),
                "fields": Common,
            },
        );
    }
}
}
