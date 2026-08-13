# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore IntercomDataProvider module definition

/*  IntercomRecordTables.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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
#! The Intercom record-based tables
/** Intercom publishes two real search endpoints - one for contacts and one for conversations - and both
    take a **filter tree in the request body** rather than a query string.  Those two tables therefore
    declare @ref DataProvider::DataProviderInfo::supports_native_search "supports_native_search"; the other
    five list a collection and are searched here, as the records arrive.

    @section intercomrecord_tree The Query Is a Tree, Not a String

    A single predicate is <tt>{"field": …, "operator": …, "value": …}</tt> and a combination is
    <tt>{"operator": "AND", "value": [ … ]}</tt>, nested as deeply as needed.  Nothing about that is a
    string, so @ref IntercomRecordTables::Search "Search" renders the whole expression itself with
    @ref RestSchemaActions::RestSchemaQuerySyntaxInfo::render_query "render_query" and returns a hash.

    The operator table is still declared beside the hook and still decides **what is pushed down**, so
    \c supports_native_search, \c supports_pushdown and the residual predicate stay declarative and are not
    hidden inside the renderer.

    @section intercomrecord_ops There Is No >=, No <= and No NOT

    Intercom's own description enumerates the operators it accepts - \c =, \c !=, \c IN, \c NIN, \c <,
    \c >, \c ~, \c !~, \c ^ and \c $ - and there is **no** \c >= and **no** \c <= among them, nor any
    negation of a whole sub-expression.  Mapping \c >= onto \c > would silently drop the boundary records,
    which is the one way a search can be wrong rather than merely slow, so those operators are left out and
    are evaluated here instead.

    \c ~ (contains), \c ^ (starts with) and \c $ (ends with) are declared
    @ref RestSchemaActions::RestSchemaRecordQueryInfo::inexact_ops "inexact_ops": Intercom documents neither
    their case sensitivity nor their collation, so each narrows what has to be read **and** is applied again
    to what comes back.

    @section intercomrecord_matrix Which Field Takes Which Operator Was Measured

    Intercom's description says only "the accepted field that you want to search on" and enumerates none of
    them, so every entry in the two \c field_ops tables below was **measured against a live workspace**, one
    request per field and operator.  What that found is not guessable from the description:

    |!Field kind|!Operators
    |a contact's identity and text - \c id, \c name, \c email, \c phone, \c role, \c external_id|<tt>= != ~ ^ $ IN</tt>
    |a contact's timestamps - \c created_at, \c last_seen_at and the rest|<tt>= < ></tt> and **not** \c !=
    |a contact's booleans - \c has_hard_bounced and the rest|<tt>= !=</tt>
    |a conversation's timestamps|<tt>= != < ></tt> - unlike a contact's, which refuse \c !=
    |a conversation's \c id, \c admin_assignee_id, \c team_assignee_id|<tt>= != < > IN</tt>, because Intercom compares them as numbers

    Fields Intercom refuses outright, though a record carries them, are simply absent: a contact's \c tags,
    \c companies, \c notes, \c custom_attributes and \c workspace_id, and a conversation's \c channel and
    \c contacts.  A predicate on one of those is answered here.

    Intercom will also search a nested value - \c location.country on a contact, \c source.author.email on a
    conversation - but a table searches its **columns**, and neither is one: the record carries \c location
    and \c source as whole objects.  Such a predicate is evaluated here as well.

    @section intercomrecord_time Timestamps Are Counts of Seconds

    Intercom writes and compares every timestamp as an integer count of seconds since the epoch, so the
    renderer converts a %Qore @ref date "date" to one.  A caller may compare against either.

    @section intercomrecord_post The Two Searches Are POST Operations

    Both carry their query **and their paging** in the request body, where every other listing here takes
    its paging as query arguments.  The manifest publishes that paging object's two properties in its place
    with @ref RestSchemaActions::RestSchemaFieldOverlayInfo::lift "lift", so \c per_page and
    \c starting_after are ordinary options and mean the same thing everywhere in this application - and a
    table can name the one option carrying its cursor, which it could not do while the cursor was a property
    of a value.

    @section intercomrecord_company Two Things Are Called a Company ID

    Intercom addresses a company by **its own** identifier in \c "/companies/{company_id}", and
    \c "POST /companies" matches on the \c company_id **the caller** gave it, creating a company when
    nothing matches.  The two are different values under one name.

    So this table updates through \c UpdateCompany, which takes Intercom's identifier in the path.  Using
    \c createOrUpdateCompany for an update would put Intercom's identifier where the caller's belongs and
    **create a second company** rather than change the one matched - a write to the wrong record, which is
    the worst thing a record table can do.

    @section intercomrecord_eventual A Company Write Is Not Visible At Once

    Two behaviours of Intercom's company endpoints surprise a caller who reads back what it just wrote, and
    both were measured against a live workspace rather than inferred:

    - a company created through \c "POST /companies" is **not** in \c "POST /companies/list" immediately;
      that listing is backed by an index that lags the write.  Reading the new company by the identity the
      create returned works at once.
    - \c "DELETE /companies/{company_id}" answers <tt>{"deleted": true}</tt> and
      \c "GET /companies/{company_id}" **still returns the company** afterwards.  The delete is accepted and
      applied; the read-by-identity endpoint continues to answer with it.

    Neither is something this layer can paper over - a table reports what the API did - so a caller that
    needs to observe its own write should trust the response of the write rather than a subsequent read.

    @section intercomrecord_readonly What Cannot Be Written

    A conversation is created and read and is not updated or deleted here: Intercom changes one by replying
    to it or by managing its parts, which is what **Reply To Conversation** does, and neither is an update
    of a record's columns.  A tag is created only by attaching it to something.  A collection, an article's
    author list and the teammate list are read only.
*/
public class IntercomRecordTables {
    public {
        #! The action group the record-based actions belong to
        const RecordGroup = "Record-Based";

        #! Intercom's search filter tree
        /** @see @ref intercomrecord_tree
        */
        const Search = <RestSchemaQuerySyntaxInfo>{
            "name": "Intercom search",
            # declared so that the pushdown decision stays declarative; the tree itself is rendered by the
            # hook below, which spells these same tokens in its "operator" property
            "binary_ops": {
                DP_SEARCH_OP_EQ: "=",
                DP_SEARCH_OP_NE: "!=",
                DP_SEARCH_OP_LT: "<",
                DP_SEARCH_OP_GT: ">",
                DP_SEARCH_OP_IN: "IN",
                DP_OP_CONTAINS: "~",
                DP_OP_STARTS_WITH: "^",
                DP_OP_ENDS_WITH: "$",
            },
            "and_op": "AND",
            "or_op": "OR",
            # Intercom negates a predicate with "!=" and "!~"; it cannot negate a sub-expression
            "not_op": NOTHING,
            "render_query": \IntercomRecordTables::renderQuery(),
        };

        #! The operators Intercom does not mean exactly, so that the predicate is answered here as well
        const InexactOps = {
            DP_OP_CONTAINS: True,
            DP_OP_STARTS_WITH: True,
            DP_OP_ENDS_WITH: True,
        };

        #! The operators a text field accepts, measured live
        const TextOps = {
            DP_SEARCH_OP_EQ: True,
            DP_SEARCH_OP_NE: True,
            DP_SEARCH_OP_IN: True,
            DP_OP_CONTAINS: True,
            DP_OP_STARTS_WITH: True,
            DP_OP_ENDS_WITH: True,
        };

        #! The operators a contact's timestamp accepts, measured live
        /** A contact's timestamps refuse \c "!=", where a conversation's accept it.
        */
        const ContactTimeOps = {
            DP_SEARCH_OP_EQ: True,
            DP_SEARCH_OP_LT: True,
            DP_SEARCH_OP_GT: True,
        };

        #! The operators a conversation's timestamp accepts, measured live
        const ConversationTimeOps = {
            DP_SEARCH_OP_EQ: True,
            DP_SEARCH_OP_NE: True,
            DP_SEARCH_OP_LT: True,
            DP_SEARCH_OP_GT: True,
        };

        #! The operators a boolean accepts, measured live
        const BoolOps = {
            DP_SEARCH_OP_EQ: True,
            DP_SEARCH_OP_NE: True,
        };

        #! The operators a numeric identifier accepts, measured live
        const NumericIdOps = {
            DP_SEARCH_OP_EQ: True,
            DP_SEARCH_OP_NE: True,
            DP_SEARCH_OP_LT: True,
            DP_SEARCH_OP_GT: True,
            DP_SEARCH_OP_IN: True,
        };

        #! How a cursor-paged collection walks from one page to the next
        /** Intercom reports the next cursor at \c pages.next.starting_after, and omits \c pages entirely
            once the collection is exhausted - which is what says there is no next page.
        */
        const CursorPaging = <RestSchemaRecordPagingInfo>{
            "mode": RSRP_CURSOR,
            "page_option": "starting_after",
            "size_option": "per_page",
            "max_size": IntercomManifest::MaxPageSize,
            "next_path": ("pages", "next", "starting_after"),
        };

        #! How the one page-numbered collection walks from one page to the next
        /** \c "POST /companies/list" takes a page number rather than a cursor, and reports
            \c pages.total_pages.

            It also caps its page **lower** than every other listing here and **rejects** a larger value
            rather than clamping it - \c "Per Page is too big" - so the size is declared per table rather
            than taken from the client's maximum.  60 is the largest value the live API accepted.
        */
        const CompanyPageSize = 60;

        #! How the one page-numbered collection walks from one page to the next
        const PagePaging = <RestSchemaRecordPagingInfo>{
            "mode": RSRP_PAGE,
            "page_option": "page",
            "size_option": "per_page",
            "max_size": CompanyPageSize,
        };

        #! A collection Intercom returns whole, with no paging arguments at all
        const NoPaging = <RestSchemaRecordPagingInfo>{"mode": RSRP_NONE};

        #! The contacts table
        /** Contacts are not created here: Intercom's contact create is not exported by this application,
            and a table naming an action the manifest does not export could only ever fail.

            @see @ref intercomrecord_matrix
        */
        const ContactsTable = <RestSchemaRecordTableInfo>{
            "name": "contacts",
            "display_name": "Contacts",
            "short_desc": "Intercom contacts as records",
            "desc": "Intercom contacts as records; Intercom evaluates a search over them itself.\n\n"
                "A contact is read, searched, updated and deleted here. Deleting one is permanent - "
                "archive it instead to keep the record and stop messaging the person",
            "list_action": "SearchContacts",
            "get_action": "ShowContact",
            "update_action": "UpdateContact",
            "delete_action": "DeleteContact",
            "id_field": "id",
            "id_option": "contact_id",
            "records_path": ("data",),
            "paging": CursorPaging,
            "search_options": ("sort",),
            # Intercom returns all ten of these on every contact and describes none of them; a column the
            # table does not declare cannot be searched at all
            "extra_columns": RestSchemaTableDataProvider::makeRecordType((
                {"name": "opted_in_subscription_types", "type": "*hash",
                    "desc": "the subscription types the contact has opted in to, as a list envelope"},
                {"name": "opted_out_subscription_types", "type": "*hash",
                    "desc": "the subscription types the contact has opted out of, as a list envelope"},
                {"name": "utm_campaign", "type": "*string", "desc": "the campaign that brought the contact"},
                {"name": "utm_content", "type": "*string", "desc": "the content that brought the contact"},
                {"name": "utm_medium", "type": "*string", "desc": "the medium that brought the contact"},
                {"name": "utm_source", "type": "*string", "desc": "the source that brought the contact"},
                {"name": "utm_term", "type": "*string", "desc": "the search term that brought the contact"},
                {"name": "referrer", "type": "*string", "desc": "the page that referred the contact"},
                {"name": "sms_consent", "type": "*bool",
                    "desc": "whether the contact has consented to receive SMS"},
                {"name": "unsubscribed_from_sms", "type": "*bool",
                    "desc": "whether the contact has unsubscribed from SMS"},
            )),
            "query": <RestSchemaRecordQueryInfo>{
                "option": "query",
                "syntax": Search,
                "inexact_ops": InexactOps,
                "field_ops": {
                    "id": TextOps,
                    "role": TextOps,
                    "name": TextOps,
                    "email": TextOps,
                    "phone": TextOps,
                    "external_id": TextOps,
                    "email_domain": TextOps,
                    "language_override": TextOps,
                    "browser": TextOps,
                    "browser_language": TextOps,
                    "os": TextOps,
                    "avatar": TextOps,
                    "android_app_name": TextOps,
                    "ios_app_name": TextOps,
                    "owner_id": NumericIdOps,
                    "created_at": ContactTimeOps,
                    "updated_at": ContactTimeOps,
                    "signed_up_at": ContactTimeOps,
                    "last_seen_at": ContactTimeOps,
                    "last_contacted_at": ContactTimeOps,
                    "last_replied_at": ContactTimeOps,
                    "last_email_opened_at": ContactTimeOps,
                    "last_email_clicked_at": ContactTimeOps,
                    "unsubscribed_from_emails": BoolOps,
                    "marked_email_as_spam": BoolOps,
                    "has_hard_bounced": BoolOps,
                },
            },
        };

        #! The conversations table
        /** @see @ref intercomrecord_readonly
        */
        const ConversationsTable = <RestSchemaRecordTableInfo>{
            "name": "conversations",
            "display_name": "Conversations",
            "short_desc": "Intercom conversations as records",
            "desc": "Intercom conversations as records; Intercom evaluates a search over them itself.\n\n"
                "A conversation is created and read here. It is not updated as a record: Intercom changes "
                "one by replying to it or by managing its parts, which is what **Reply To Conversation** "
                "does",
            "list_action": "searchConversations",
            "get_action": "retrieveConversation",
            "create_action": "createConversation",
            "id_field": "id",
            "id_option": "conversation_id",
            "records_path": ("conversations",),
            "paging": CursorPaging,
            # returned on every conversation and described nowhere
            "extra_columns": RestSchemaTableDataProvider::makeRecordType((
                {"name": "topics", "type": "*hash",
                    "desc": "the topics Intercom has classified the conversation under"},
                {"name": "ticket", "type": "*hash",
                    "desc": "the ticket the conversation was converted into, if it was"},
            )),
            "query": <RestSchemaRecordQueryInfo>{
                "option": "query",
                "syntax": Search,
                "inexact_ops": InexactOps,
                "field_ops": {
                    "id": NumericIdOps,
                    "admin_assignee_id": NumericIdOps,
                    "team_assignee_id": NumericIdOps,
                    "state": TextOps,
                    "priority": TextOps,
                    "title": TextOps,
                    "created_at": ConversationTimeOps,
                    "updated_at": ConversationTimeOps,
                    "waiting_since": ConversationTimeOps,
                    "snoozed_until": ConversationTimeOps,
                    "open": BoolOps,
                    "read": BoolOps,
                    "ai_agent_participated": BoolOps,
                },
            },
        };

        #! The companies table
        /** @see @ref intercomrecord_company
        */
        const CompaniesTable = <RestSchemaRecordTableInfo>{
            "name": "companies",
            "display_name": "Companies",
            "short_desc": "Intercom companies as records",
            "desc": "Intercom companies as records.\n\n"
                "Intercom publishes no search for companies, so a search reads the collection and is "
                "evaluated as the records arrive. A create matches on the `company_id` **you** give it and "
                "creates a company when nothing matches; an update names the identity **Intercom** gave it",
            "list_action": "listAllCompanies",
            "get_action": "RetrieveACompanyById",
            "create_action": "createOrUpdateCompany",
            "update_action": "UpdateCompany",
            "delete_action": "deleteCompany",
            "id_field": "id",
            "id_option": "company_id",
            "records_path": ("data",),
            "paging": PagePaging,
            "search_options": ("order",),
        };

        #! The help center articles table
        const ArticlesTable = <RestSchemaRecordTableInfo>{
            "name": "articles",
            "display_name": "Articles",
            "short_desc": "the help center articles, as records",
            "desc": "The workspace's help center articles, as records.\n\n"
                "Intercom publishes no filter arguments on this listing, so a search reads the collection "
                "and is evaluated as the records arrive. An article's parent can only be a **collection**: "
                "Intercom withdrew sections at API version 2.10",
            "list_action": "listArticles",
            "get_action": "retrieveArticle",
            "create_action": "createArticle",
            "update_action": "updateArticle",
            "delete_action": "deleteArticle",
            "id_field": "id",
            "id_option": "article_id",
            "records_path": ("data",),
            "paging": CursorPaging,
            # returned on every article and described nowhere
            "extra_columns": RestSchemaTableDataProvider::makeRecordType((
                {"name": "content_id", "type": "*string",
                    "desc": "the identity of the article's content, which Intercom versions separately "
                        "from the article itself"},
                {"name": "audience_ids", "type": "*list",
                    "desc": "the audiences the article is shown to"},
            )),
        };

        #! The help center collections table
        /** @see @ref intercomrecord_readonly
        */
        const CollectionsTable = <RestSchemaRecordTableInfo>{
            "name": "collections",
            "display_name": "Collections",
            "short_desc": "the help center collections, as records",
            "desc": "The workspace's help center collections, as records; read only here.\n\n"
                "What used to be a **section** is now a collection with a parent, which is how Intercom has "
                "represented them since API version 2.10",
            "list_action": "listAllCollections",
            "get_action": "retrieveCollection",
            "id_field": "id",
            "id_option": "collection_id",
            "records_path": ("data",),
            "paging": CursorPaging,
            # returned on every collection and described nowhere
            "extra_columns": RestSchemaTableDataProvider::makeRecordType((
                {"name": "type", "type": "*string",
                    "desc": "the kind of record, always \"collection\" here"},
            )),
        };

        #! The tags table
        /** A tag is created by attaching it to something rather than on its own, so this table reads and
            deletes.  Intercom **refuses** to delete a tag still applied anywhere.
        */
        const TagsTable = <RestSchemaRecordTableInfo>{
            "name": "tags",
            "display_name": "Tags",
            "short_desc": "the workspace's tags, as records",
            "desc": "The workspace's tags, as records.\n\n"
                "Intercom returns them all in one response rather than paging them. A tag is created by "
                "attaching it to a contact or a company rather than on its own, and Intercom refuses to "
                "delete one that is still applied to anything",
            "list_action": "listTags",
            "get_action": "findTag",
            "delete_action": "deleteTag",
            "id_field": "id",
            "id_option": "tag_id",
            "records_path": ("data",),
            "paging": NoPaging,
        };

        #! The teammates table
        /** @see @ref intercomrecord_readonly
        */
        const AdminsTable = <RestSchemaRecordTableInfo>{
            "name": "teammates",
            "display_name": "Teammates",
            "short_desc": "the teammates with access to the workspace, as records",
            "desc": "The teammates who have access to the workspace, as records; read only here, because a "
                "teammate is a person Intercom's own administration manages.\n\n"
                "Intercom returns them all in one response rather than paging them",
            "list_action": "listAdmins",
            "id_field": "id",
            "records_path": ("admins",),
            "paging": NoPaging,
        };

        #! Every table, in presentation order
        const Tables = (ContactsTable, ConversationsTable, CompaniesTable, ArticlesTable, CollectionsTable,
            TagsTable, AdminsTable);

        #! The IDs the record-based actions are registered under
        const RecordActionIds = <RestSchemaRecordActionIds>{};
    }

    #! Renders a search expression as the tree of hashes Intercom's search takes
    /** The expression has already been accepted by
        @ref RestSchemaActions::RestSchemaRecordFilter::canRender() "canRender()", so every operator in it
        is one this syntax declares and every field is one the table's matrix allows.

        @param exp the expression to render

        @return the value of the \c query body property: a single filter, or a boolean combination of them

        @see @ref intercomrecord_tree
    */
    static auto renderQuery(hash<DataProviderExpression> exp) {
        switch (exp.exp) {
            case DP_OP_AND:
            case DP_OP_OR: {
                list<auto> args = map IntercomRecordTables::renderQuery(
                    cast<hash<DataProviderExpression>>($1)), exp.args;
                # a combination of one is the filter itself; Intercom accepts either, and the simpler form
                # is the one a person reading the request will recognise
                if (args.size() == 1) {
                    return args[0];
                }
                return {
                    "operator": exp.exp == DP_OP_AND ? Search.and_op : Search.or_op,
                    "value": args,
                };
            }
        }
        string field = cast<hash<DataProviderFieldReference>>(exp.args[0]).field;
        if (exp.exp == DP_SEARCH_OP_IN) {
            # the values arrive either as one list or as separate arguments, and both mean the same set
            list<auto> values = ();
            for (int i = 1; i < exp.args.size(); ++i) {
                values += exp.args[i].typeCode() == NT_LIST ? exp.args[i] : list(exp.args[i]);
            }
            return {
                "field": field,
                "operator": Search.binary_ops{DP_SEARCH_OP_IN},
                "value": map IntercomRecordTables::renderValue($1), values,
            };
        }
        return {
            "field": field,
            "operator": Search.binary_ops{exp.exp},
            "value": IntercomRecordTables::renderValue(exp.args[1]),
        };
    }

    #! Returns one value as Intercom compares it
    /** Intercom stores and compares every timestamp as an integer count of seconds since the epoch, so a
        %Qore @ref date "date" becomes one.  Everything else travels as it is: Intercom's own description
        types a filter value as a string, an integer, a boolean or a list of them.

        @param value the value from the expression
    */
    static auto renderValue(auto value) {
        if (value.typeCode() == NT_DATE) {
            # the same conversion the manifest's "unix-seconds" codec applies to an action's options
            return get_epoch_seconds(value);
        }
        return value;
    }
}
}
