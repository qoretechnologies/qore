# -*- mode: qore; indent-tabs-mode: nil -*-
/** PineconeUsage.qc Copyright 2026 Qore Technologies, s.r.o.

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

/** @page pineconerecords Pinecone Record Tables

    Use the <tt>tables</tt> child of a Pinecone data provider for record-based reads, creates, updates, and
    deletes. Each table also appears in the generic <b>Search</b>, <b>Search Single</b>, <b>Create</b>,
    <b>Update</b>, and <b>Delete</b> action forms when it supports that operation.

    |!Table|!Identity|!Scope|!Operations
    |indexes|name|project connection|read, create, update, delete
    |collections|name|project connection|read, create, delete (legacy pod indexes)
    |backups|backup_id|project-wide reads; index_name for create|read, create, delete
    |backup-schedules|schedule_id|index_name for list/create; global ID for other operations|read, create, update, delete
    |restore-jobs|restore_job_id|project connection|read only
    |models|model|project connection|read only; type and vector_type filters
    |namespaces|name|index|read, create, delete
    |imports|id|index|read, create, cancel through delete
    |vectors|id|index, optional namespace (default namespace when empty)|read, upsert through create, update, delete
    |documents|_id|index, namespace|read, upsert through create, update, delete

    @section pineconerecordsread Complete Reads

    Unfiltered vector and document scans list IDs, then fetch each page's complete records in a single
    batch. They retain listing order and skip IDs concurrently deleted between list and fetch. A
    continuation token advances the scan even when a short page, or an entirely deleted page, returns
    no records. Filtered scans use the paginated metadata fetch endpoints. Ranked similarity search
    remains available through the API actions; <tt>top_k</tt> results cannot enumerate a table.

    Vector records contain <tt>id</tt>, optional <tt>values</tt>, optional <tt>sparseValues</tt>, and an open <tt>metadata</tt>
    hash. Documents contain <tt>_id</tt> and an open <tt>fields</tt> hash. Write forms come from the operation's
    request schema and exclude response-only fields. Document writes flatten <tt>fields</tt> into Pinecone's
    wire format; an <tt>_id</tt> inside <tt>fields</tt> is rejected to prevent overriding the record identity.

    @code{.py}
AbstractDataProvider pinecone = conn.getDataProvider();
AbstractDataProvider vectors = pinecone.getChildProviderPath("tables/vectors");
hash<auto> scope = {"index": "products", "namespace": "europe"};
foreach hash<auto> record in (vectors.searchRecords({"metadata.category": "books"}, scope)) {
    printf("%s: %y\n", record.id, record.metadata);
}
AbstractDataProvider documents = pinecone.getChildProviderPath("tables/documents");
hash<auto> doc_scope = {"index": "reports", "namespace": "finance"};
documents.createRecord({"_id": "report-2026-q3", "fields": {
    "title": "Quarterly report", "year": 2026,
}}, doc_scope);
# After the write is visible, update the existing document by its ID.
documents.updateRecords({"fields": {"title": "Revised quarterly report"}},
    {"_id": "report-2026-q3"}, doc_scope);
    @endcode

    @section pineconerecordsfilters Metadata Filters

    Predicates on <tt>metadata.category</tt> or <tt>fields.category</tt> use Pinecone's <tt>$eq</tt>, <tt>$ne</tt>, <tt>$gt</tt>, <tt>$gte</tt>,
    <tt>$lt</tt>, <tt>$lte</tt>, <tt>$in</tt>, <tt>$nin</tt>, and <tt>$exists</tt> operators, with <tt>$and</tt> and <tt>$or</tt>. Native comparisons
    follow Pinecone's metadata semantics. <tt>is_null</tt> tests field absence, since metadata does not store
    null values. Inequalities require numeric operands. Membership lists must contain at most 10,000
    strings or numbers; boolean, mixed-type, or larger lists are evaluated locally. Literal dots in field names use the data
    provider's escaped-dot notation. Nested object traversal, logical NOT, and unsupported expressions
    are evaluated locally against full records. The requested result limit is counted after residual
    filtering, so a nonmatching first page does not hide matches on later pages.

    @section pineconerecordswrites Write Semantics and Consistency

    <b>Pinecone is eventually consistent.</b> A successful write acknowledges acceptance; an immediately
    following read may still show the preceding state. Record operations do not add sleeps, retries,
    transactions, or a read-after-write guarantee. See
    <a href="https://docs.pinecone.io/guides/index-data/check-data-freshness">Check data freshness</a>.

    Vector/document create returns the submitted record after the service acknowledges the upsert.
    Updates patch supplied values; vector metadata becomes <tt>setMetadata</tt>. Updates and deletes check
    the selected identity first because Pinecone also acknowledges writes to missing IDs. The reported
    count is the number of matched identities whose writes were acknowledged, subject to concurrent
    modification and eventual consistency. These operations cannot provide atomic affected-row counts.

    Collection writes require a predicate. A non-identity predicate is resolved completely before the
    first write; <tt>max_records</tt> can bound the affected set. Deleting a namespace removes its contents.
    Deleting an import cancels the job and does not undo records already imported.

    @section pineconerecordstesting Live Integration Stages

    The offline tests run without credentials. The live test in <tt>PineconeRecordTables.qtest</tt> uses
    <tt>PINECONE_APIKEY</tt>, <tt>PINECONE_TABLE_STATE</tt> (a fixture JSON path), and <tt>PINECONE_TABLE_PHASE</tt>.
    Run <tt>prepare</tt>, <tt>read</tt>, <tt>updated</tt>, <tt>deleted</tt>, and <tt>cleaned</tt> as separate stages after preceding writes are visible.
    Each stage asserts once and never sleeps or polls. <tt>prepare</tt> creates uniquely named test indexes;
    <tt>deleted</tt> verifies record deletions and requests index deletion; <tt>cleaned</tt> verifies index removal. <tt>cleanup</tt> explicitly removes the same owned
    fixtures. Failed stages also attempt cleanup. The fixture file contains no credentials.
*/

/** @page pineconeadmin Pinecone Organization Administration

    The <b>Pinecone Admin</b> app provisions projects and manages organization access. It uses a separate
    <tt>pinecone-admin</tt> connection and the pinned Admin API schema. The database/inference app continues
    to use a project API key through <tt>pinecone</tt>.

    |!Resource|!Actions
    |Projects|list, create, get, update, delete
    |Organizations|list, get, update, delete
    |API keys|list for project, create, get, update, delete
    |Service accounts|list, create, get, update, delete, rotate secret
    |Users|list, get, remove
    |Invites|list, create, get, delete, resend
    |Role bindings|list, create, get, delete

    @section pineconeadminconnection Service-Account Connection

    Create a service account in Pinecone and grant the roles its workflows require. Configure its
    <b>Client ID</b> and <b>Client Secret</b>, both sensitive options. The service account's resource <tt>id</tt>
    is a different value from its OAuth2 <tt>client_id</tt>.

    @code{.py}
PineconeAdminRestConnection conn("pinecone-admin", "Organization provisioning",
    PineconeAdminRestConnection::DefaultUrl, {}, {
        "oauth2_client_id": ENV.PINECONE_CLIENT_ID,
        "oauth2_client_secret": ENV.PINECONE_CLIENT_SECRET,
    });
conn.ping(True);
AbstractDataProvider admin = conn.getDataProvider();
hash<auto> project = admin.getChildProviderEx("create-project").doRequest({"name": "Support search"});
hash<auto> key = admin.getChildProviderEx("create-api-key").doRequest({
    "project_id": project.id, "name": "Search reader", "roles": ("DataPlaneViewer",),
});
# Store key.value securely in the project's Pinecone connection; do not log it.
    @endcode

    Tokens use <tt>client_credentials</tt>, <tt>https://login.pinecone.io/oauth/token</tt>, and the fixed audience
    <tt>https://api.pinecone.io/</tt>. Both REST clients renew an expiring access token by acquiring another
    client-credentials token. The app has no interactive authorization step. Ping authenticates and
    lists accessible projects, including for organizations whose project list is empty.

    @section pineconeadminforms Action Forms and References

    All 32 actions publish typed request and response fields from the vendored <tt>2026-07</tt> schema.
    Vendor <tt>x-enum</tt> values become named choices, including roles inside role bindings. ID fields offer
    live references where the API can enumerate them and accept custom IDs. API-key references first
    list projects, then list their keys. Polymorphic principal IDs identify users, service accounts,
    API keys, or invites according to <tt>principal_type</tt>; use the resource ID returned by the matching
    listing. Paginated reference lists offer a first page of choices and always accept a custom ID;
    use the corresponding list action's cursor options to inspect subsequent pages.

    <b>Issued secrets are returned only at creation or rotation.</b> API-key <tt>value</tt> and service-account
    credentials are marked sensitive. Store them securely when returned. Rotation invalidates the
    previous secret; update the connections that use the service account. <b>Create Invite</b> and
    <b>Resend Invite</b> send email to the specified user.

    @section pineconeadminvalidation Validation

    <tt>PineconeAdminRestClient.qtest</tt> exercises real local HTTP token and resource endpoints, including
    invalid credentials, fixed audience, token expiry and authenticated ping. Optional live project
    creation/deletion in <tt>PineconeAdminDataProvider.qtest</tt> requires <tt>PINECONE_ADMIN_LIVE_TEST=1</tt> and
    either <tt>PINECONE_ADMIN_CONNECTION</tt> or both <tt>PINECONE_CLIENT_ID</tt> and <tt>PINECONE_CLIENT_SECRET</tt>.
    A project API key cannot substitute for Admin credentials.
*/
