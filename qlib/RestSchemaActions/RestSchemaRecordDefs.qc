# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore RestSchemaActions module definition

/*  RestSchemaRecordDefs.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the RestSchemaActions module
public namespace RestSchemaActions {
/** @defgroup RestSchemaRecordPagingModes REST Schema Record Paging Modes
    Identifies how a collection listing walks from one page of records to the next.

    Every record table declares one of these in
    @ref RestSchemaActions::RestSchemaRecordPagingInfo::mode "RestSchemaRecordPagingInfo::mode", because a
    record search has to be able to read past the first page: a filter the API cannot evaluate is applied to
    the records as they arrive, so the number of pages a search reads is not known before it runs.
*/
#/@{
#! Paging mode: the listing takes a page number
public const RSRP_PAGE = "page";

#! Paging mode: the listing takes a record offset
public const RSRP_OFFSET = "offset";

#! Paging mode: the listing takes an opaque cursor reported by the previous response
public const RSRP_CURSOR = "cursor";

#! Paging mode: the listing takes the ID of the last record of the previous page
public const RSRP_AFTER_ID = "after-id";

#! Paging mode: the listing is not paged and returns everything it has in one response
public const RSRP_NONE = "none";

#! All valid paging modes
public const RestSchemaRecordPagingModes = {
    RSRP_PAGE: True,
    RSRP_OFFSET: True,
    RSRP_CURSOR: True,
    RSRP_AFTER_ID: True,
    RSRP_NONE: True,
};
#/@}

#! Describes how a record table's listing action walks from one page to the next
/** A record search may have to read arbitrarily far into a collection: the limit given by the caller counts
    **matching** records, and a predicate the API cannot evaluate is applied to each page as it arrives.  A
    table that could only read the page the listing returns by default would silently under-return, which is
    the defect this declaration exists to prevent.

    @see @ref RestSchemaRecordPagingModes
*/
public hashdecl RestSchemaRecordPagingInfo {
    #! How the listing walks from one page to the next
    /** @see @ref RestSchemaRecordPagingModes
    */
    string mode = RSRP_PAGE;

    #! The listing action option carrying the page number, offset or cursor
    /** Not used with @ref RSRP_NONE.
    */
    *string page_option;

    #! The number of the first page, for @ref RSRP_PAGE
    /** Almost every API numbers pages from 1; the exception is rare enough to be worth declaring rather than
        assuming.
    */
    int first_page = 1;

    #! The listing action option carrying the page size
    *string size_option;

    #! The largest page the API accepts
    /** Sent as the page size on every request, so that a search reads as few pages as the API allows.  A page
        size larger than the API's maximum is normally rejected or silently clamped, and both make paging
        wrong, so this is the vendor's documented cap rather than a preference.
    */
    *int max_size;

    #! A path of field names leading from the listing response to the next cursor, for @ref RSRP_CURSOR
    *softlist<string> next_path;

    #! A path of field names leading from the listing response to a flag saying more records follow
    /** When absent, a page shorter than the size asked for is taken as the last one.  When present it is
        authoritative: some APIs return a full page and no more records after it.
    */
    *softlist<string> has_more_path;

    #! Extracts the cursor from the value at @ref next_path, when that value is a URL rather than a cursor
    /** Confluence reports the next page as a link containing a \c cursor argument rather than as the cursor
        itself.
    */
    *code<*string(auto)> next_cursor;
}

#! Declares one predicate a record table's listing action can evaluate itself
/** Filtering is what makes a record search on a REST API usable at all: a table with ten thousand records
    behind a page size of 100 cannot be searched by reading all of it.  This declares, for one **field** and
    one **operator**, the listing action option that expresses it.

    @section restschemarecordfilter_exact Exactness Is the Load-Bearing Property

    @ref exact says whether the API's filter means exactly what the operator means.  It defaults to
    @ref False, which is the answer that cannot produce a wrong result: the filter is sent **and** the
    predicate is evaluated again on the records that come back, so an API that returns more than it was asked
    for costs a little client-side work and nothing else.

    Declaring @ref exact is therefore a statement about the API, and getting it wrong is the one way this
    machinery can under-return.  A filter that is documented as fuzzy, case-insensitive, prefix-matching or
    "best effort" is not exact whatever it is called, and a filter whose semantics are not documented at all
    is not exact either.
*/
public hashdecl RestSchemaRecordFilterInfo {
    #! The record field this rule filters on
    string field;

    #! The generic data provider operator this rule implements
    /** @see @ref dataprovider_generic_search_ops
    */
    string operator;

    #! The listing action option carrying the value
    string option;

    #! Whether the API's filter means exactly what the operator means
    /** When @ref True the predicate is satisfied entirely by the API and is dropped from the client-side
        residual.  When @ref False - the default - the filter is sent as a narrowing hint and the predicate is
        evaluated again on the records that come back.

        @see @ref restschemarecordfilter_exact
    */
    bool exact = False;

    #! Converts the predicate's value to the value the option takes
    /** Use where the option's spelling differs from the record's: a state named differently on the wire, a
        date the API wants as a day rather than an instant, a list the API wants comma-separated.
    */
    *code<auto(auto)> to_wire;

    #! Whether the option takes a list of values
    /** Set for an option implementing @ref DP_SEARCH_OP_IN, whose value is the whole list.
    */
    *bool multi;
}

#! Describes a query language the API evaluates server-side, and how to render an expression tree into it
/** Several of the applications in this family publish a real filter expression language - Xero's \c where,
    Bitbucket's BBQL, Klaviyo's \c filter and Intercom's search tree - and for those an entire expression tree
    maps onto the wire rather than one predicate at a time.  That is the difference between a table that
    declares @ref DataProvider::DataProviderInfo::supports_native_search "supports_native_search" honestly and
    one that only narrows what it fetches.

    The syntax is declared rather than coded because most of it is small and entirely regular: a token per
    operator in one of three rendering forms, and one function that renders a value.  What is **not**
    regular - which fields the API will accept in a filter - belongs to the table, not the syntax.

    @section restschemaquerysyntax_forms The Three Rendering Forms

    |!Form|!Declared in|!Renders as|!Example
    |infix|@ref binary_ops|<tt>field op value</tt>|<tt>Status=="AUTHORISED"</tt>
    |call|@ref call_ops|<tt>op(field,value)</tt>|<tt>equals(channel,"email")</tt>
    |method|@ref method_ops|<tt>field.Method(value)</tt>|<tt>Name.Contains("x")</tt>

    An operator absent from all three cannot be rendered, and any predicate using it is evaluated client-side
    instead.

    @section restschemaquerysyntax_connectives A Language Need Not Have Every Connective

    @ref and_op is mandatory because a language with no conjunction cannot express a search at all, but
    @ref or_op and @ref not_op are optional, and a language that does not declare one **cannot render it**:
    the whole expression becomes residual and is evaluated client-side.  Klaviyo's \c filter argument supports
    neither, so declaring them there would either drop half a search or send Klaviyo a query it rejects.

    @section restschemaquerysyntax_hook Rendering the Whole Expression

    @ref render_query takes the rendering over entirely, for a language whose query is not a string at all:
    Intercom's search takes a **tree of hashes** in the request body.  The operator tables are still declared
    alongside it and still decide what is pushed down, so
    @ref DataProvider::DataProviderInfo::supports_native_search "supports_native_search" and the residual
    predicate stay declarative; only the spelling moves into the hook.
*/
public hashdecl RestSchemaQuerySyntaxInfo {
    #! The syntax name, used in error messages
    string name;

    #! Generic operator to infix token, ex: <tt>{DP_SEARCH_OP_EQ: "=="}</tt>
    /** @see @ref restschemaquerysyntax_forms
    */
    hash<string, string> binary_ops;

    #! Generic operator to a token rendered as <tt>op(field,value)</tt>
    /** Klaviyo spells every predicate this way: <tt>equals(email,"x")</tt>,
        <tt>greater-than(created,"2026-01-01")</tt>.

        @see @ref restschemaquerysyntax_forms
    */
    *hash<string, string> call_ops;

    #! Generic operator to a method name rendered as <tt>field.Method(value)</tt>
    /** Xero spells "contains" as <tt>Name.Contains("x")</tt>; Bitbucket spells it as an infix \c ~ and
        declares it in @ref binary_ops instead.

        @see @ref restschemaquerysyntax_forms
    */
    *hash<string, string> method_ops;

    #! The token joining the arguments of a logical AND
    string and_op = "AND";

    #! The token joining the arguments of a logical OR, if the language has one
    /** @see @ref restschemaquerysyntax_connectives
    */
    *string or_op = "OR";

    #! The token negating a sub-expression, if the language has one
    /** @see @ref restschemaquerysyntax_connectives
    */
    *string not_op = "NOT";

    #! The literal separator joining the arguments of a logical AND
    /** Defaults to @ref and_op with a space on each side, which is how an infix language reads.  Klaviyo
        joins its predicates with a bare comma and no spaces, so it declares <tt>","</tt> here.
    */
    *string and_sep;

    #! The literal separator joining the arguments of a logical OR
    /** Defaults to @ref or_op with a space on each side.
    */
    *string or_sep;

    #! Whether a rendered group of more than one argument is wrapped in parentheses
    /** True for an infix language, where grouping is what makes precedence unambiguous.  False for a language
        with no grouping at all: Klaviyo's \c filter is a flat comma-separated list of predicates and rejects
        a parenthesised one.
    */
    bool parenthesize = True;

    #! Renders one value as the query language spells it
    /** @param value the value from the expression
        @param field the record field it is being compared against, so that a syntax can render an identifier
        differently from a name - Xero wraps a GUID in \c guid("...")

        @return the rendered literal

        Required unless @ref render_query is set, which renders the values itself.
    */
    *code<string(auto, string)> render_value;

    #! Renders one field reference as the query language spells it
    /** Absent when the field name is used unchanged.
    */
    *code<string(string)> render_field;

    #! Renders the whole expression, replacing the built-in rendering entirely
    /** @param exp the expression to render; it has already been accepted by
        @ref RestSchemaActions::RestSchemaRecordFilter::canRender() "canRender()", so every operator in it is
        one this syntax declares and every connective is one it has

        @return the value to place on the query option, which need **not** be a string: Intercom's search
        takes a hash

        @see @ref restschemaquerysyntax_hook
    */
    *code<auto(hash<DataProviderExpression>)> render_query;
}

#! Declares that a record table's listing action takes a whole filter expression in one option
/** @see @ref RestSchemaActions::RestSchemaQuerySyntaxInfo "RestSchemaQuerySyntaxInfo"
*/
public hashdecl RestSchemaRecordQueryInfo {
    #! The listing action option carrying the rendered query
    string option;

    #! The query language the option takes
    hash<RestSchemaQuerySyntaxInfo> syntax;

    #! The record fields the API will accept in a filter, if it will not accept all of them
    /** A predicate naming a field absent from this set is evaluated client-side.  Both Xero and Bitbucket
        reject a filter naming a field they do not index, so offering one would turn a search into an error.
    */
    *hash<string, bool> fields;

    #! Operators the language can render but whose meaning is not exactly the operator's
    /** A rendered query is normally taken as exact, because the service evaluates it.  An operator listed
        here is the exception: it is still rendered and sent, because it narrows what has to be read, but the
        predicate is **also** evaluated on what comes back.

        This is for an operator whose spelling matches and whose semantics do not quite: a "contains" the
        vendor documents without saying whether it is case-sensitive, a comparison that coerces its operands.
        Declaring one here is how a search stays correct without giving up the narrowing.
    */
    *hash<string, bool> inexact_ops;

    #! Record field to the name the query language uses for it, where the two differ
    *hash<string, string> field_map;
}

#! Describes one record-shaped resource an application exposes as a table
/** A table is assembled from actions the manifest already exports, so that the paths, the request contracts,
    the escaping, the codecs and the response shapes are the schema's and are not written a second time here.
    What this descriptor adds is what a schema cannot say: which listing is the collection behind which table,
    which field is the record's identity, and which predicates the API can evaluate for itself.

    @section restschemarecordtable_scope Scope Options

    Almost no collection in this family is global: an issue belongs to a repository, a page to a space, a task
    to a project.  @ref scope_options names the listing action's options that identify the collection, and
    they become required search and create options of the table, because without them there is no collection
    to read.

    @section restschemarecordtable_type The Record Type

    By default the record type is derived from the schema's own description of the record, so it cannot drift
    from the contract the table's actions use.  It is taken from **both** reading actions: a listing returns a
    projection and a single read returns the record, the two rarely agree - GitLab describes a project with 24
    fields in its listing and 161 in its single read - and the richer of them is published, with any column
    only the other declares added to it.

    That matters more than it looks: a column the table does not declare cannot be searched at all, because
    the framework rejects a predicate on an undeclared field before the table is reached.

    @ref extra_columns adds the columns a vendor returns and describes nowhere, keeping the schema primary.

    @ref record_type replaces the derived type outright, for a table that publishes a projection of its own
    rather than the API's shape.  A projection is a real product decision - it is what existing mappers read -
    and it needs @ref to_record and @ref to_wire to convert in both directions.
*/
public hashdecl RestSchemaRecordTableInfo {
    #! The table name, unique within the application
    string name;

    #! The table display name
    string display_name;

    #! The table short description in plain text
    string short_desc;

    #! The table description with Markdown formatting
    string desc;

    #! The action listing the collection
    string list_action;

    #! The action reading one record by its identity
    /** When present, a search whose predicate is exactly an equality on @ref id_field is answered by reading
        the one record rather than by walking the collection.
    */
    *string get_action;

    #! The action creating a record
    *string create_action;

    #! The action updating a record
    *string update_action;

    #! The action deleting a record
    *string delete_action;

    #! The record field carrying the record's identity
    string id_field = "id";

    #! The option carrying the record's identity on the get, update and delete actions
    /** Defaults to @ref id_field.  Ignored when @ref id_parts is set.
    */
    *string id_option;

    #! The record's identity, where the API addresses a record with more than one value
    /** Keyed by the option name on the get, update and delete actions, with the value naming the record
        field supplying it - a \c "." separated path where the value is nested in the record.

        An API does not always address a record with one value.  GitHub names a repository by its
        \c {owner} **and** its \c {repo}, both path variables, and a repository record carries the second as
        \c name and the first as \c "owner.login", because its \c owner is an object rather than a string:

        @code{.py}
"id_parts": {"owner": "owner.login", "repo": "name"},
        @endcode

        This is **not** the same thing as
        @ref RestSchemaRecordTableInfo::scope_options "scope_options", and the difference decides which one
        to use.  A scope option identifies the **collection** and is the same for every record in a search -
        the repository an issue belongs to, the customer a subscription belongs to - so the caller supplies
        it and the listing requires it.  An identity part identifies **one record** and varies from row to
        row within a single listing, so it is read back off each record.  GitHub's repository listing spans
        owners, which is precisely why \c owner cannot be a scope there.

        Set this **or** @ref id_field and @ref id_option, not both kinds; a single-valued identity is the
        common case and is written the short way.
    */
    *hash<string, string> id_parts;

    #! A path of field names leading from the listing response to the list of records
    /** Not needed when the listing's output is itself the list.
    */
    *softlist<string> records_path;

    #! A path of field names leading from the single-record response to the record
    *softlist<string> record_path;

    #! A path of field names leading from the create or update response to the record
    /** Defaults to @ref record_path.
    */
    *softlist<string> write_record_path;

    #! The listing action options identifying the collection
    /** @see @ref restschemarecordtable_scope
    */
    *softlist<string> scope_options;

    #! How the listing walks from one page to the next
    hash<RestSchemaRecordPagingInfo> paging;

    #! The predicates the listing action can evaluate itself
    *softlist<hash<RestSchemaRecordFilterInfo>> filters;

    #! The filter expression language the listing action takes, if it has one
    *hash<RestSchemaRecordQueryInfo> query;

    #! Further listing options exposed as search options of the table
    /** A sort order, or a scope the collection accepts that is not a predicate on a record field.  Each names
        an option of the listing action.
    */
    *softlist<string> search_options;

    #! Overrides merged into the search options taken from the listing action
    /** A table's options are the listing's, so each is described once and in one place.  A table may still
        need to present one differently from the action, and a default value is the case that matters: the
        state a listing defaults to need not be the state a **table** defaults to, and a table that changed
        it by accident would return a different set of records from the one the application it replaces
        returned.

        Keyed by option name.
    */
    *hash<string, hash<auto>> search_option_overrides;

    #! An explicit record type, replacing the one derived from the reading actions' responses
    /** @see @ref restschemarecordtable_type
    */
    *hash<string, AbstractDataField> record_type;

    #! Columns the table declares in addition to the ones the schema describes
    /** For a record field the vendor's description does not declare at all.  Bitbucket returns a
        repository's \c slug in every response and describes it nowhere, and a column the table does not
        declare cannot be searched: the framework rejects the predicate before the table is reached.

        Merged over the derived type, so the schema stays the primary description and this is the short
        list of what it is missing - which is a far lighter thing to maintain than the whole record type,
        and does not go stale when the vendor adds a field.

        @see @ref restschemarecordtable_type
    */
    *hash<string, AbstractDataField> extra_columns;

    #! The record fields the table will not accept on a create or an update
    /** An identity and its timestamps are the service's to assign, and sending them is either ignored or
        rejected.
    */
    *softlist<string> read_only_fields;

    #! Converts a record as the API returns it into the table's shape
    /** Required with @ref record_type, and unused without it.
    */
    *code<hash<auto>(hash<auto>)> to_record;

    #! Converts a record in the table's shape into the options a create or update takes
    /** Required with @ref record_type, and unused without it.
    */
    *code<hash<auto>(hash<auto>)> to_wire;

    #! Whether the table's create action takes the record as a list of records
    /** Xero's writes take a list of documents even when there is one of them.
    */
    *string write_list_option;
}
}
