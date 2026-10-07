# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore PineconeDataProvider module: the exported Pinecone API operations

/** PineconeManifest.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace PineconeDataProvider {
#! The operations this application exports from the Pinecone API
/** Pinecone publishes one description per API rather than one per product, and this application exports
    all three of the ones a data integration needs: the <b>control plane</b> that manages indexes, backups
    and collections, the <b>data plane</b> that searches and writes vectors and documents, and the
    <b>inference</b> API that embeds and reranks text.  52 operations in total, every one of them exported.

    Action IDs are a product commitment: an upstream rename is reported as drift and never renames an
    action here.  \c list-indexes predates this manifest - it was the application's only action - and
    keeps the ID it was published with.

    @section pineconemanifest_planes Three Documents, Three Action Sets

    A @ref RestSchemaActions::RestSchemaActionSet "RestSchemaActionSet" binds one schema, because a
    request's URI prefix comes from that schema's base path, so each document becomes its own set.  They
    are built in dependency order and reach each other's dropdowns as peers:

    |!Set|!Operations|!Depends on
    |inference|4|nothing
    |db_control|24|inference, for the embedding-model dropdown
    |db_data|24|db_control, for the index dropdown and the host source; inference, for the reranking model

    @section pineconemanifest_xenum Every Enum Is a Vendor Extension

    This is the one thing to know before reading the overlay below.  Pinecone declares <b>no</b>
    \c "enum" for any of its finite value sets.  It writes them as \c "x-enum" - a vendor extension -
    beside a bare \c "type: string":

    @code{.yaml}
metric:
  description: "The distance metric used for similarity search.\nPossible values: `cosine`, `dotproduct`, or `euclidean`."
  x-enum: [cosine, dotproduct, euclidean]
  type: string
    @endcode

    An extension is invisible to the parser, so every one of these generates as free text and a user
    gets a text box where the API accepts exactly three values.  There are <b>28</b> such declarations
    across the three documents; the ones that reach an action option are restored here as
    @ref RestSchemaActions::RestSchemaFieldOverlayInfo::allowed_values "allowed_values" with display
    names, and the values are the vendor's own - copied from the \c "x-enum" list, not from
    documentation and not from memory.

    The distinction that matters when reading a diff: a value set that appears here <b>is</b> in the
    committed document, in the \c "x-enum" beside the field.  If the vendor adds a fourth metric, the
    import shows it and this list has to grow; nothing detects that automatically, because drift
    compares what the parser can see.

    @section pineconemanifest_region Region Is the One Set the Document Does Not Declare

    \c cloud is restored from an \c "x-enum" like everything else; \c region is not declared anywhere,
    and none of Pinecone's seven API descriptions publishes an endpoint that lists regions.  Asking the
    API does not enumerate them either - an unknown pair answers

    @verbatim
{"error":{"code":"NOT_FOUND","message":"Resource cloud: aws region: eu-north-1 not found"},"status":404}
    @endverbatim

    which checks a value without revealing the set.

    So @ref RegionValues is taken from the vendor's published list and <b>every pair in it was verified
    against the live API</b>, by creating an index in it and dropping it again; \c "aws"/"eu-north-1" and
    \c "gcp"/"us-east-1" were both refused, which is what shows Pinecone validates the *pair* rather
    than the region string.  The option is
    @ref RestSchemaActions::RestSchemaFieldOverlayInfo::allowed_values_creatable "creatable", because a
    list written today cannot know about a region Pinecone opens tomorrow and a dropdown is an aid
    rather than a constraint.

    The display names carry the cloud and the place - "AWS · Frankfurt" rather than \c eu-central-1 -
    because the identifier alone says neither, and because the same-looking identifier can exist on one
    cloud and not another.  \c create-index takes its cloud and region inside the \c oneOf deployment
    object rather than beside it, and a nested overlay reaches them there: an entry is applied to every
    alternative declaring the field, and one no alternative declares is refused.

    @section pineconemanifest_version The API Version Is the Connection's

    All three documents declare a <b>required</b> \c X-Pinecone-Api-Version header on every one of the 52
    operations.  It is the connection's business rather than each action's - see
    @ref PineconeRestClient::PineconeRestClientBase::ApiVersion "PineconeRestClientBase::ApiVersion" -
    so every action drops it with
    @ref RestSchemaActions::RestSchemaFieldOverlayInfo::ignore "ignore" and the client's default headers
    fill it in when the request is built.

    Optional would have been the dangerous case here, and this one is worse: the header is declared
    required, but the server accepts a request without it and answers under whatever version it currently
    defaults to.  An action generated from one version and sent against another makes a request the
    description does not describe, so the client pins it and this module's tests assert the two are
    equal.

    @section pineconemanifest_host The Index Is the Host

    The 24 data-plane operations do not name an index anywhere - not in a path variable, not in a query
    argument, not in a body property.  The index <b>is</b> the host: Pinecone's own description declares the
    data-plane server as the template \c "https://{index_host}", and only the control plane knows what
    that host is for a given index.

    @ref HostSources declares that relationship once.  The framework synthesizes an \c index option on
    every action naming it, resolves the host through \c describe-index, caches it per client, and
    consumes the option - it is never sent.  See
    @ref RestSchemaActions::RestSchemaHostSourceInfo "RestSchemaHostSourceInfo" and the
    @ref restschemaactions_hosts "host routing" section of the module's main page.

    The dropdown behind that option is the ordinary index listing, so a user picks an index by name and
    never sees a host at all.

    @section pineconemanifest_selectors Selector Bodies

    Pinecone writes every "delete by one of three things" body the same way: an object declaring all of
    its properties, plus an \c "anyOf" of branches that narrow them, plus a \c "not" excluding the
    combinations it will not take.  \c delete-documents is the clearest:

    @code{.yaml}
type: object
properties: {ids: ..., filter: ..., delete_all: ...}
anyOf:
  - {title: "By ID",           required: [ids],        properties: {delete_all: {enum: [false]}}, not: {required: [filter]}}
  - {title: "By filter",       required: [filter],     properties: {delete_all: {enum: [false]}}, not: {required: [ids]}}
  - {title: "Whole namespace", required: [delete_all], properties: {delete_all: {enum: [true]}},  not: {anyOf: [...]}}
    @endcode

    Those branches introduce nothing - every property they name is declared by the object above them - so
    the composition is a constraint, not a set of alternative shapes.  Reading it as alternatives
    published \c delete-documents and \c update-documents with <b>no options at all</b>, and
    \c fetch-documents with one opaque \c body option; that was a defect in %OpenApi3's composition
    handling and is fixed rather than worked around here.  The either/or is expressed to the user with
    @ref RestSchemaActions::RestSchemaFieldOverlayInfo::required_groups "required_groups" and
    @ref RestSchemaActions::RestSchemaFieldOverlayInfo::exclusive_with "exclusive_with", which say the
    same thing in a form a form can render.

    @section pineconemanifest_collections Collections Are Legacy

    \c list-collections, \c create-collection, \c describe-collection and \c delete-collection are
    exported, and Pinecone's own description says of each that collections are supported only for
    pod-based indexes and that pod-based indexes <b>cannot be created</b> at API version \c 2026-07.  They
    are kept because a project that still has an index created under an earlier version still has
    collections to manage, and withdrawing the actions would remove the only way to reach them.  Each
    action's description says so, so nobody reaches for one expecting it to work on a serverless index.

    @since %PineconeDataProvider 1.0
*/
public class PineconeManifest {
    public {
        #! Action groups
        const GroupIndexes = "Indexes";
        const GroupBackups = "Backups";
        const GroupCollections = "Collections";
        const GroupNamespaces = "Namespaces";
        const GroupVectors = "Vectors";
        const GroupDocuments = "Documents";
        const GroupImports = "Imports";
        const GroupInference = "Inference";

        #! Reference data names
        const RefIndex = "pinecone-indexes";
        const RefCollection = "pinecone-collections";
        const RefBackup = "pinecone-backups";
        const RefRestoreJob = "pinecone-restore-jobs";
        const RefNamespace = "pinecone-namespaces";
        const RefImport = "pinecone-imports";
        const RefEmbedModel = "pinecone-embed-models";
        const RefRerankModel = "pinecone-rerank-models";

        #! The host source resolving an index's data-plane host
        const HostIndex = "index_host";

        #! The dated API version header, which the connection owns
        /** Declared required on all 52 operations; the client sends it on every call, so no action asks
            for it.  @ref RestSchemaActions::RestSchemaFieldOverlayInfo::connection_option "connection_option"
            names \c headers, which is what a user would set to override it.
        */
        const VersionHeaderOverlay = <RestSchemaFieldOverlayInfo>{
            "ignore": True,
            "connection_option": "headers",
        };

        #! The version header, keyed as every operation declares it
        const VersionHeaderFields = {
            "X-Pinecone-Api-Version": VersionHeaderOverlay,
        };

        #! The distance metrics Pinecone offers, restored from the document's own \c "x-enum"
        const MetricValues = (
            <AllowedValueInfo>{
                "value": "cosine",
                "display_name": "Cosine",
                "desc": "Cosine similarity: the angle between the vectors, ignoring their length.  The "
                    "usual choice for text embeddings.",
            },
            <AllowedValueInfo>{
                "value": "dotproduct",
                "display_name": "Dot Product",
                "desc": "The dot product of the vectors, which takes length into account as well as "
                    "direction.  Required for hybrid (dense plus sparse) search.",
            },
            <AllowedValueInfo>{
                "value": "euclidean",
                "display_name": "Euclidean",
                "desc": "Straight-line distance between the vectors; the smaller the score, the more "
                    "similar.",
            },
        );

        #! The clouds an index may be hosted on, restored from the document's own \c "x-enum"
        const CloudValues = (
            <AllowedValueInfo>{"value": "aws", "display_name": "AWS", "desc": "Amazon Web Services."},
            <AllowedValueInfo>{"value": "gcp", "display_name": "Google Cloud",
                "desc": "Google Cloud Platform."},
            <AllowedValueInfo>{"value": "azure", "display_name": "Azure", "desc": "Microsoft Azure."},
        );

        #! The regions a serverless index may be hosted in
        /** The one value set in this application that does <b>not</b> come from the document: Pinecone
            declares \c region as a bare string, publishes no endpoint that lists regions in any of its
            seven API descriptions, and its own validation reports an unknown pair without enumerating
            the known ones (\c "Resource cloud: aws region: eu-north-1 not found").

            So the list is the vendor's published one, and every pair in it was <b>verified against the
            live API</b> by creating an index in it and dropping it again; \c "aws"/"eu-north-1" and
            \c "gcp"/"us-east-1" were both refused, which is what shows the *pair* is validated rather
            than the region string on its own.

            A region belongs to one cloud, so a value here is only valid with the matching
            @ref CloudValues entry.  Pinecone opens regions over time and this list cannot know about
            one added after it was written, which is what
            @ref RestSchemaActions::RestSchemaFieldOverlayInfo::allowed_values_creatable "allowed_values_creatable"
            is for: the dropdown is an aid, and a region typed by hand is still sent.
        */
        const RegionValues = (
            <AllowedValueInfo>{
                "value": "us-east-1",
                "display_name": "AWS · Virginia",
                "desc": "AWS `us-east-1`, in Virginia.  The only region available on the Starter plan.",
            },
            <AllowedValueInfo>{
                "value": "us-west-2",
                "display_name": "AWS · Oregon",
                "desc": "AWS `us-west-2`, in Oregon.",
            },
            <AllowedValueInfo>{
                "value": "eu-west-1",
                "display_name": "AWS · Ireland",
                "desc": "AWS `eu-west-1`, in Ireland.",
            },
            <AllowedValueInfo>{
                "value": "eu-central-1",
                "display_name": "AWS · Frankfurt",
                "desc": "AWS `eu-central-1`, in Frankfurt.",
            },
            <AllowedValueInfo>{
                "value": "ap-southeast-1",
                "display_name": "AWS · Singapore",
                "desc": "AWS `ap-southeast-1`, in Singapore.",
            },
            <AllowedValueInfo>{
                "value": "us-central1",
                "display_name": "Google Cloud · Iowa",
                "desc": "Google Cloud `us-central1`, in Iowa.",
            },
            <AllowedValueInfo>{
                "value": "europe-west4",
                "display_name": "Google Cloud · Netherlands",
                "desc": "Google Cloud `europe-west4`, in the Netherlands.",
            },
            <AllowedValueInfo>{
                "value": "eastus2",
                "display_name": "Azure · Virginia",
                "desc": "Azure `eastus2`, in Virginia.",
            },
        );

        #! The region an index is hosted in
        /** @see @ref RegionValues for where the list comes from and why it is creatable
        */
        const RegionOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Region",
            "short_desc": "The region to host the index in",
            "desc": "The region to host the index in.  A region belongs to one cloud, so it has to match "
                "the **Cloud** chosen beside it: `us-east-1` exists on AWS and not on Google Cloud, and "
                "Pinecone refuses the pair rather than the value.\n\n"
                "The list is Pinecone's published set at the time of writing and a region it has opened "
                "since can still be typed in; see "
                "[Available cloud regions](https://docs.pinecone.io/troubleshooting/available-cloud-regions).",
            "allowed_values": RegionValues,
            "allowed_values_creatable": True,
        };

        #! Whether an index may be deleted, restored from the document's own \c "x-enum"
        const DeletionProtectionValues = (
            <AllowedValueInfo>{
                "value": "disabled",
                "display_name": "Disabled",
                "desc": "The index can be deleted.  This is the default.",
            },
            <AllowedValueInfo>{
                "value": "enabled",
                "display_name": "Enabled",
                "desc": "Deleting the index is refused until protection is turned off again with "
                    "**Configure Index**.",
            },
        );

        #! How often a backup schedule runs, restored from the document's own \c "x-enum"
        const ScheduleFrequencyValues = (
            <AllowedValueInfo>{"value": "daily", "display_name": "Daily", "desc": "Back up once a day."},
            <AllowedValueInfo>{"value": "weekly", "display_name": "Weekly",
                "desc": "Back up once a week."},
            <AllowedValueInfo>{"value": "monthly", "display_name": "Monthly",
                "desc": "Back up once a month."},
        );

        #! The kinds of backup schedule, restored from the document's own \c "x-enum"
        /** One value today; it is still a closed set, and a field the API accepts exactly one value for
            is better shown as that value than as an empty box.
        */
        const ScheduleTypeValues = (
            <AllowedValueInfo>{
                "value": "time-based",
                "display_name": "Time-based",
                "desc": "Run the backup on a fixed schedule.  The only kind Pinecone supports.",
            },
        );

        #! How reads are served, from the read-capacity discriminator's own mapping
        const ReadCapacityModeValues = (
            <AllowedValueInfo>{
                "value": "OnDemand",
                "display_name": "On demand",
                "desc": "Reads are served without dedicated nodes and billed by use.  The default, and "
                    "not available on a BYOC index.",
            },
            <AllowedValueInfo>{
                "value": "Dedicated",
                "display_name": "Dedicated nodes",
                "desc": "Reads are served by dedicated nodes, for a predictable workload.  Requires "
                    "`node_type` and `scaling` beside it.",
            },
        );

        #! How required terms are matched, restored from the document's own \c "x-enum"
        const MatchStrategyValues = (
            <AllowedValueInfo>{
                "value": "all",
                "display_name": "All terms",
                "desc": "Every term listed must appear in the hit.  The only strategy Pinecone "
                    "supports.",
            },
        );

        #! What an import does when a record fails, restored from the document's own \c "x-enum"
        const ImportErrorModeValues = (
            <AllowedValueInfo>{
                "value": "continue",
                "display_name": "Continue",
                "desc": "Skip the record that failed and import the rest.  The failures are reported on "
                    "the import.",
            },
            <AllowedValueInfo>{
                "value": "abort",
                "display_name": "Abort",
                "desc": "Stop the whole import at the first record that fails.",
            },
        );

        #! The kinds of hosted model, restored from the document's own \c "x-enum"
        const ModelTypeValues = (
            <AllowedValueInfo>{
                "value": "embed",
                "display_name": "Embedding",
                "desc": "Models that turn text into vectors.",
            },
            <AllowedValueInfo>{
                "value": "rerank",
                "display_name": "Reranking",
                "desc": "Models that re-order search results by relevance to a query.",
            },
        );

        #! The kinds of vector an embedding model produces, restored from the document's own \c "x-enum"
        const VectorTypeValues = (
            <AllowedValueInfo>{
                "value": "dense",
                "display_name": "Dense",
                "desc": "Fixed-length vectors of floating-point numbers, for semantic similarity.",
            },
            <AllowedValueInfo>{
                "value": "sparse",
                "display_name": "Sparse",
                "desc": "Mostly-zero vectors keyed by term, for keyword-style matching.",
            },
        );

        #! How a document search scores documents, restored from the document's own \c "x-enum"
        const ScoringMethodValues = (
            <AllowedValueInfo>{
                "value": "dense_vector",
                "display_name": "Dense Vector",
                "desc": "Score by similarity to a dense vector.  Needs `values` and one field.",
            },
            <AllowedValueInfo>{
                "value": "sparse_vector",
                "display_name": "Sparse Vector",
                "desc": "Score by similarity to a sparse vector.  Needs `sparse_values` and one field.",
            },
            <AllowedValueInfo>{
                "value": "text",
                "display_name": "Text (BM25)",
                "desc": "Score by BM25 text similarity against one or more fields.  Needs `query`.",
            },
            <AllowedValueInfo>{
                "value": "query_string",
                "display_name": "Query String",
                "desc": "Score with a Lucene query string, which carries its own field qualifiers.  "
                    "Naming a field as well is rejected.",
            },
        );

        #! How many results a listing returns per page
        const LimitOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Page Size",
            "short_desc": "How many results to return per page",
        };

        #! Where a listing resumes from
        const PaginationTokenOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Pagination Token",
            "short_desc": "The pagination token from the previous response",
            "desc": "The `pagination.next` value from the previous response.  A response that carries no "
                "`pagination` is the last page.",
        };

        #! The two paging arguments the control plane's listings take
        const PagingFields = {
            "limit": LimitOverlay,
            "paginationToken": PaginationTokenOverlay,
        };

        #! The index an action works on, where the index is named by the request
        /** Used by the control-plane actions, which take the index name in the path.  A data-plane
            action does <b>not</b> use this: there the index selects the host, and its option is
            synthesized by the host source - see @ref IndexKeyOverlay.
        */
        const IndexNameOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Index",
            "short_desc": "The index this applies to",
            "desc": "The index this applies to, by name.",
            "ref_data": RefIndex,
            "supports_custom_values": True,
            "preselected": True,
        };

        #! The index whose data-plane host a request is sent to
        /** Declared once here and applied to all 24 data-plane actions.  The option does not come from
            the schema - no data-plane operation names an index - so the host source synthesizes it, and
            this is where its presentation is declared.  An action's own overlay entry for \c "index"
            refines this rather than replacing it.
        */
        const IndexKeyOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Index",
            "short_desc": "The index whose data this request reads or writes",
            "desc": "The index whose data this request reads or writes.\n\n"
                "Pinecone serves each index's data from **its own host**, which only the control plane "
                "knows, so this is not sent with the request: it selects where the request goes.  The "
                "host is looked up once per index and reused.",
            "ref_data": RefIndex,
            "supports_custom_values": True,
            "preselected": True,
        };

        #! The namespace a data-plane request applies to, where it is optional
        const NamespaceOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Namespace",
            "short_desc": "The namespace to work in",
            "desc": "The namespace to work in.  A namespace partitions an index, and a request only ever "
                "sees one of them.\n\n"
                "Leave this empty to use the default namespace, which Pinecone also calls "
                "`__default__`.",
            "ref_data": RefNamespace,
            "supports_custom_values": True,
            "depends_on": ("index",),
            "preselected": True,
        };

        #! The namespace a data-plane request applies to, where the path requires it
        const NamespacePathOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Namespace",
            "short_desc": "The namespace to work in",
            "desc": "The namespace to work in.  Use `__default__` for the namespace requests address "
                "when they name none.",
            "ref_data": RefNamespace,
            "supports_custom_values": True,
            "depends_on": ("index",),
            "preselected": True,
        };

        #! The backup schedule an action works on
        /** No dropdown, deliberately: Pinecone lists backup schedules <b>per index</b>
            (\c "GET /indexes/{index_name}/backup-schedules") and publishes no project-wide listing, so
            a dropdown here would need an index to scope it - and none of the four actions that take a
            schedule ID has one, because the schedule ID addresses the schedule on its own.  Adding an
            index option purely to fill a dropdown would add an option the API does not take.

            <b>List Backup Schedules</b> is where the IDs come from.
        */
        const ScheduleIdOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Backup Schedule",
            "short_desc": "The ID of the backup schedule",
            "desc": "The ID of the backup schedule, as returned by **List Backup Schedules** for the "
                "index it belongs to.",
            "preselected": True,
        };

        #! The bulk import an action works on
        const ImportIdOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Import",
            "short_desc": "The bulk import this applies to",
            "ref_data": RefImport,
            "supports_custom_values": True,
            "depends_on": ("index",),
            "preselected": True,
        };

        #! The collection an action works on
        const CollectionOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Collection",
            "short_desc": "The collection this applies to",
            "ref_data": RefCollection,
            "supports_custom_values": True,
            "preselected": True,
        };

        #! A metadata filter expression
        const FilterOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Metadata Filter",
            "short_desc": "A metadata filter selecting which records apply",
            "desc": "A metadata filter selecting which records apply, written in Pinecone's own filter "
                "language — a hash of field names to conditions:\n\n"
                "```json\n"
                "{\"genre\": {\"$in\": [\"comedy\", \"drama\"]}, \"year\": {\"$eq\": 2019}}\n"
                "```\n\n"
                "The operators are `$eq`, `$ne`, `$gt`, `$gte`, `$lt`, `$lte`, `$in`, `$nin`, `$exists` "
                "and `$and` / `$or`.  See "
                "[Understanding metadata](https://docs.pinecone.io/guides/index-data/indexing-overview#metadata).",
            "example_value": {"genre": {"$in": ("comedy", "drama")}, "year": {"$eq": 2019}},
        };

        #! Tags on an index
        const TagsOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Tags",
            "short_desc": "Arbitrary key/value tags to store on the index",
            "desc": "Arbitrary key/value tags to store on the index, for your own bookkeeping.  Pinecone "
                "does not interpret them.",
            "example_value": {"environment": "production"},
        };

        #! Whether the index may be deleted
        const DeletionProtectionOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Deletion Protection",
            "short_desc": "Whether deleting the index is refused",
            "desc": "Whether deleting the index is refused.  Turn it on for an index a flow depends on; "
                "**Configure Index** turns it off again.",
            "allowed_values": DeletionProtectionValues,
        };

        #! How much read capacity an index is given
        const ReadCapacityOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Read Capacity",
            "short_desc": "On-demand or dedicated read nodes",
            "desc": "How reads are served.\n\n"
                "- `{\"mode\": \"OnDemand\"}` — the default; you are billed for what you read.\n"
                "- `{\"mode\": \"Dedicated\", \"node_type\": ..., \"scaling\": ...}` — dedicated read "
                "nodes for a predictable workload.\n\n"
                "A BYOC index must set `Dedicated` explicitly: it does not support on-demand, and "
                "omitting this defaults to on-demand and is rejected.",
            "fields": {
                "mode": <RestSchemaFieldOverlayInfo>{
                    "display_name": "Read Capacity Mode",
                    "short_desc": "On-demand reads or dedicated read nodes",
                    "allowed_values": ReadCapacityModeValues,
                },
            },
        };

        #! Dropdowns backed by the application's own listing actions
        /** The control plane's listings; the data-plane ones are in @ref DataReferenceData, because a
            listing has to run against the set whose schema declares it.
        */
        const ControlReferenceData = {
            RefIndex: <RestSchemaReferenceDataInfo>{
                "name": RefIndex,
                "action": "list-indexes",
                "records_path": "indexes",
                "value_field": "name",
                "desc": "the indexes in this project",
            },
            RefCollection: <RestSchemaReferenceDataInfo>{
                "name": RefCollection,
                "action": "list-collections",
                "records_path": "collections",
                "value_field": "name",
                "desc": "the collections in this project",
            },
            RefBackup: <RestSchemaReferenceDataInfo>{
                "name": RefBackup,
                "action": "list-backups",
                "options": {"limit": 100},
                "records_path": "data",
                "value_field": "backup_id",
                "display_field": "name",
                "desc": "the backups in this project",
            },
            RefRestoreJob: <RestSchemaReferenceDataInfo>{
                "name": RefRestoreJob,
                "action": "list-restore-jobs",
                "options": {"limit": 100},
                "records_path": "data",
                "value_field": "restore_job_id",
                "desc": "the restore jobs in this project",
            },
        };

        #! Dropdowns backed by the data plane's own listing actions
        /** Both are scoped by the index, because a namespace and an import belong to one index and the
            listing that reads them is itself routed to that index's host.  \c options_from forwards the
            calling action's synthesized \c index option into the listing's, so the dropdown is empty
            until an index has been chosen rather than listing another index's namespaces.
        */
        const DataReferenceData = {
            RefNamespace: <RestSchemaReferenceDataInfo>{
                "name": RefNamespace,
                "action": "list-namespaces",
                "options": {"limit": 100},
                "options_from": {"index": "index"},
                "records_path": "namespaces",
                "value_field": "name",
                "desc": "the namespaces in the selected index",
            },
            RefImport: <RestSchemaReferenceDataInfo>{
                "name": RefImport,
                "action": "list-imports",
                "options": {"limit": 100},
                "options_from": {"index": "index"},
                "records_path": "data",
                "value_field": "id",
                "desc": "the bulk imports of the selected index",
            },
        };

        #! Dropdowns backed by the inference API's model listing
        /** One listing, two dropdowns: an embedding model and a reranking model are not
            interchangeable, and offering every model in both places would let a flow be configured with
            one that cannot do the job.  \c options fixes the \c type argument, so each dropdown asks for
            its own kind.
        */
        const InferenceReferenceData = {
            RefEmbedModel: <RestSchemaReferenceDataInfo>{
                "name": RefEmbedModel,
                "action": "list-models",
                "options": {"type": "embed"},
                "records_path": "models",
                "value_field": "model",
                "desc": "the embedding models Pinecone hosts",
            },
            RefRerankModel: <RestSchemaReferenceDataInfo>{
                "name": RefRerankModel,
                "action": "list-models",
                "options": {"type": "rerank"},
                "records_path": "models",
                "value_field": "model",
                "desc": "the reranking models Pinecone hosts",
            },
        };

        #! The host source resolving an index's data-plane host
        /** Declared by the control-plane set, which exports the lookup, and reached from the data-plane
            set as a peer - exactly as reference data is.

            \c allowed_host_suffixes is required rather than optional and is what bounds where the
            connection's credentials may be sent: following a host out of a response means addressing
            somewhere the connection URL did not name.  It is matched label by label, so
            \c "evil-pinecone.io" does not pass for \c "pinecone.io".
        */
        const HostSources = {
            HostIndex: <RestSchemaHostSourceInfo>{
                "name": HostIndex,
                "action": "describe-index",
                "key_option": "index",
                "lookup_option": "index_name",
                "host_field": ("host",),
                "scheme": "https",
                "allowed_host_suffixes": ("pinecone.io",),
                "key_overlay": IndexKeyOverlay,
                "desc": "the data-plane host serving the selected index",
            },
        };

        #! The control-plane operations this application exports
        /** Index, backup, backup-schedule, restore-job and collection management: everything that
            describes what exists, as opposed to what is in it.
        */
        const ControlManifest = (
            <RestSchemaActionInfo>{
                "action": "list-indexes",
                "method": "GET",
                "path": "/indexes",
                "operation_id": "list_indexes",
                "display_name": "List Indexes",
                "short_desc": "List the indexes in the project",
                "desc": "Lists every index in the project the API key belongs to, with the host, the "
                    "schema and the current state of each.\n\n"
                    "This is also the cheapest call that proves a key works, which is what the "
                    "connection pings.",
                "groups": (GroupIndexes,),
                "fields": VersionHeaderFields,
            },
            <RestSchemaActionInfo>{
                "action": "describe-index",
                "method": "GET",
                "path": "/indexes/{index_name}",
                "operation_id": "describe_index",
                "display_name": "Describe Index",
                "short_desc": "Get one index, including its data-plane host",
                "desc": "Returns one index: its schema, its deployment, its state, and the **host** its "
                    "data plane is served from.\n\n"
                    "Creating an index is asynchronous, so this is what a flow polls until "
                    "`status.ready` is true before writing to it.",
                "groups": (GroupIndexes,),
                "fields": VersionHeaderFields + {
                    "index_name": IndexNameOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-index",
                "method": "POST",
                "path": "/indexes",
                "operation_id": "create_index",
                "display_name": "Create Index",
                "short_desc": "Create an index from a field schema",
                "desc": "Creates an index, defining the fields it searches over: dense vectors, sparse "
                    "vectors and full-text search fields.\n\n"
                    "At least one `dense_vector`, `sparse_vector`, or `string` field with "
                    "`full_text_search` is required; an empty schema is rejected.  Two reserved names "
                    "make a **classic vector index**, read and written through the vectors actions "
                    "rather than the documents ones: `_values` for dense and `_sparse_values` for "
                    "sparse, with no other field beside them.\n\n"
                    "To have Pinecone embed your text for you, use **Create Index with Integrated "
                    "Embedding** instead.\n\n"
                    "Creation is **asynchronous**: the index is returned before it is usable, so poll "
                    "**Describe Index** until it is ready.",
                "groups": (GroupIndexes,),
                "field_order": ("name", "schema", "deployment", "deletion_protection", "tags"),
                "fields": VersionHeaderFields + {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Index Name",
                        "short_desc": "A name for the new index",
                        "desc": "A name for the new index, unique within the project.  Lower-case "
                            "letters, digits and hyphens, at most 45 characters.",
                        "preselected": True,
                    },
                    "schema": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Field Schema",
                        "short_desc": "The fields the index searches over",
                        "desc": "A map of field names to configurations, as `{\"fields\": {...}}`.\n\n"
                            "```json\n"
                            "{\"fields\": {\"embedding\": {\"type\": \"dense_vector\", "
                            "\"dimension\": 1536, \"metric\": \"cosine\"},\n"
                            "             \"body\": {\"type\": \"string\", \"full_text_search\": "
                            "{\"language\": \"en\"}}}}\n"
                            "```\n\n"
                            "For a classic vector index use the reserved `_values` (dense) and/or "
                            "`_sparse_values` (sparse) and nothing else.\n\n"
                            "At most one dense vector field and one sparse vector field, and at most "
                            "100 full-text fields.  Metadata used for filtering is **not** declared "
                            "here; it is indexed automatically from the documents you upsert.",
                        "preselected": True,
                    },
                    "deployment": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "cloud": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Cloud",
                                "short_desc": "The public cloud to host the index on",
                                "allowed_values": CloudValues,
                            },
                            "region": RegionOverlay,
                        },
                        "display_name": "Deployment",
                        "short_desc": "Where the index runs",
                        "desc": "Where the index runs.  Defaults to serverless on AWS in `us-east-1` "
                            "when omitted.\n\n"
                            "- `{\"deployment_type\": \"managed\", \"cloud\": \"aws\", \"region\": "
                            "\"us-east-1\"}` — serverless, managed by Pinecone\n"
                            "- `{\"deployment_type\": \"byoc\", ...}` — bring your own compute\n\n"
                            "`cloud` is one of `aws`, `gcp` or `azure`; for the regions each one "
                            "offers see "
                            "[Regions](https://docs.pinecone.io/guides/index-data/create-an-index#cloud-regions).",
                        "preselected": True,
                    },
                    "deletion_protection": DeletionProtectionOverlay,
                    "tags": TagsOverlay,
                    "read_capacity": ReadCapacityOverlay,
                    "cmek_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Customer-Managed Key ID",
                        "short_desc": "The customer-managed encryption key to encrypt the index with",
                        # no endpoint in any of Pinecone's descriptions lists CMEK keys, so there is no
                        # listing to back a dropdown with; saying where the value comes from is the most
                        # a free-text field can do
                        "desc": "The ID of a customer-managed encryption key to encrypt the index with, "
                            "registered for your organization in the "
                            "[Pinecone console](https://app.pinecone.io/).  Requires CMEK to be enabled "
                            "for the organization.\n\nAn encrypted index cannot carry `full_text_search` "
                            "fields.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-index-for-model",
                "method": "POST",
                "path": "/indexes/create-for-model",
                "operation_id": "create_index_for_model",
                "display_name": "Create Index with Integrated Embedding",
                "short_desc": "Create an index that embeds text for you",
                "desc": "Creates an index tied to one of Pinecone's hosted embedding models, so you "
                    "upsert and search with **text** and Pinecone produces the vectors.\n\n"
                    "This is the index type the **Upsert Text** and **Search with Text** actions need; "
                    "sending text to any other index is rejected.  It is not available on BYOC.\n\n"
                    "Creation is asynchronous — poll **Describe Index** until it is ready.",
                "groups": (GroupIndexes,),
                "field_order": ("name", "cloud", "region", "model", "field_map", "metric",
                    "deletion_protection", "tags"),
                "fields": VersionHeaderFields + {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Index Name",
                        "short_desc": "A name for the new index",
                        "desc": "A name for the new index, unique within the project.",
                        "preselected": True,
                    },
                    "cloud": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cloud",
                        "short_desc": "The public cloud to host the index on",
                        "allowed_values": CloudValues,
                        "preselected": True,
                    },
                    "region": cast<hash<RestSchemaFieldOverlayInfo>>(RegionOverlay
                        + {"preselected": True}),
                    # the embedding configuration is a grouping object, not a domain object: publishing
                    # it whole would ask a user to build a hash with a required "field_map" inside it,
                    # so its properties are lifted into options and refined through the nested overlay
                    "embed": <RestSchemaFieldOverlayInfo>{
                        "lift": True,
                        "fields": {
                            "model": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Embedding Model",
                                "short_desc": "The hosted model that embeds your text",
                                "desc": "The hosted embedding model that turns your text into vectors.  "
                                    "The model fixes the vector dimension and the metric unless you "
                                    "override them.",
                                "ref_data": RefEmbedModel,
                                "supports_custom_values": True,
                                "preselected": True,
                            },
                            "field_map": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Field Map",
                                "short_desc": "Which field of your records holds the text to embed",
                                "desc": "Which field of your records holds the text to embed, as "
                                    "`{\"text\": \"<your field name>\"}`.",
                                "preselected": True,
                                "example_value": {"text": "your-text-field"},
                            },
                            "metric": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Distance Metric",
                                "short_desc": "How similarity between vectors is measured",
                                "desc": "How similarity between vectors is measured.  Leave it empty to "
                                    "use the model's own default, which is usually the right choice.",
                                "allowed_values": MetricValues,
                            },
                            "dimension": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Dimension",
                                "short_desc": "The vector dimension, where the model supports more than "
                                    "one",
                                "example_value": 1024,
                            },
                            "read_parameters": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Read Parameters",
                                "short_desc": "Model parameters applied when searching",
                            },
                            "write_parameters": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Write Parameters",
                                "short_desc": "Model parameters applied when upserting",
                            },
                        },
                    },
                    "deletion_protection": DeletionProtectionOverlay,
                    "tags": TagsOverlay,
                    "read_capacity": ReadCapacityOverlay,
                    "schema": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Metadata Schema",
                        "short_desc": "Which metadata fields are indexed for filtering",
                        "desc": "Which metadata fields are indexed for filtering, as "
                            "`{\"fields\": {\"<name>\": {\"filterable\": true}}}`.  By default every "
                            "metadata field is indexed; naming fields here restricts it to those.  Only "
                            "`filterable: true` is accepted — to leave a field unindexed, omit it.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "configure-index",
                "method": "PATCH",
                "path": "/indexes/{index_name}",
                "operation_id": "configure_index",
                "display_name": "Configure Index",
                "short_desc": "Change an existing index's settings",
                "desc": "Changes an existing index: its deployment, its read capacity, its tags, its "
                    "deletion protection, or fields added to its schema.\n\n"
                    "Turning **deletion protection** off here is the prerequisite for deleting a "
                    "protected index.",
                "groups": (GroupIndexes,),
                "field_order": ("index_name", "deletion_protection", "tags"),
                "fields": VersionHeaderFields + {
                    "index_name": IndexNameOverlay,
                    "deletion_protection": DeletionProtectionOverlay,
                    "tags": TagsOverlay,
                    "read_capacity": ReadCapacityOverlay,
                    "deployment": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployment",
                        "short_desc": "Changes to the deployment configuration",
                    },
                    "schema": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Schema Changes",
                        "short_desc": "Fields to add to the index schema",
                        "desc": "Fields to add to the index schema.  An existing field cannot be "
                            "retyped or removed.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-index",
                "method": "DELETE",
                "path": "/indexes/{index_name}",
                "operation_id": "delete_index",
                "display_name": "Delete Index",
                "short_desc": "Delete an index and everything in it",
                "desc": "Deletes an index and every record in it.  **This cannot be undone**; take a "
                    "backup first if the data matters.\n\n"
                    "An index with deletion protection enabled is refused until **Configure Index** "
                    "turns it off.",
                "groups": (GroupIndexes,),
                "fields": VersionHeaderFields + {
                    "index_name": IndexNameOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-backups",
                "method": "GET",
                "path": "/backups",
                "operation_id": "list_project_backups",
                "display_name": "List Backups",
                "short_desc": "List every backup in the project",
                "desc": "Lists every backup in the project, across all indexes.  Use **List Index "
                    "Backups** for the backups of one index.",
                "groups": (GroupBackups,),
                "fields": VersionHeaderFields + PagingFields,
            },
            <RestSchemaActionInfo>{
                "action": "list-index-backups",
                "method": "GET",
                "path": "/indexes/{index_name}/backups",
                "operation_id": "list_index_backups",
                "display_name": "List Index Backups",
                "short_desc": "List the backups taken from one index",
                "desc": "Lists the backups taken from one index.\n\n"
                    "By default the index must still exist: a name that resolves to no active index "
                    "answers **404** rather than an empty list, including when only deleted indexes "
                    "have used it.  Set **Include Deleted** to reach the backups of an index that has "
                    "since been deleted — which is exactly when a backup matters most.",
                "groups": (GroupBackups,),
                "field_order": ("index_name", "include_deleted"),
                "fields": VersionHeaderFields + PagingFields + {
                    "index_name": IndexNameOverlay,
                    "include_deleted": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include Deleted",
                        "short_desc": "Also list backups of deleted indexes with this name",
                        "desc": "Also list backups from every index that has ever used this name, "
                            "including deleted ones.  Without it, a name with no active index answers "
                            "404.",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-backup",
                "method": "POST",
                "path": "/indexes/{index_name}/backups",
                "operation_id": "create_backup",
                "display_name": "Create Backup",
                "short_desc": "Back an index up now",
                "desc": "Takes a backup of an index now.  The backup records the index's schema, and an "
                    "index created from it with **Create Index from Backup** inherits that schema.",
                "groups": (GroupBackups,),
                "field_order": ("index_name", "name", "description"),
                "fields": VersionHeaderFields + {
                    "index_name": IndexNameOverlay,
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Backup Name",
                        "short_desc": "A name for the backup",
                        "preselected": True,
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "A description of what this backup is for",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "describe-backup",
                "method": "GET",
                "path": "/backups/{backup_id}",
                "operation_id": "describe_backup",
                "display_name": "Describe Backup",
                "short_desc": "Get one backup",
                "desc": "Returns one backup: its state, its size, the index it was taken from and the "
                    "schema it will restore.",
                "groups": (GroupBackups,),
                "fields": VersionHeaderFields + {
                    "backup_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Backup",
                        "short_desc": "The backup to describe",
                        "ref_data": RefBackup,
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-backup",
                "method": "DELETE",
                "path": "/backups/{backup_id}",
                "operation_id": "delete_backup",
                "display_name": "Delete Backup",
                "short_desc": "Delete a backup permanently",
                "desc": "Deletes a backup permanently.  The index it was taken from is not affected.",
                "groups": (GroupBackups,),
                "fields": VersionHeaderFields + {
                    "backup_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Backup",
                        "short_desc": "The backup to delete",
                        "ref_data": RefBackup,
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-index-from-backup",
                "method": "POST",
                "path": "/backups/{backup_id}/create-index",
                "operation_id": "create_index_from_backup_operation",
                "display_name": "Create Index from Backup",
                "short_desc": "Restore a backup into a new index",
                "desc": "Creates a new index from a backup.  The new index **inherits the schema** of "
                    "the index the backup was taken from, including its full-text fields and its "
                    "integrated embedding configuration, and the request cannot override it.\n\n"
                    "The restore runs asynchronously: this returns a restore job, which **Describe "
                    "Restore Job** reports the progress of.",
                "groups": (GroupBackups,),
                "field_order": ("backup_id", "name", "deletion_protection", "tags"),
                "fields": VersionHeaderFields + {
                    "backup_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Backup",
                        "short_desc": "The backup to restore",
                        "ref_data": RefBackup,
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "New Index Name",
                        "short_desc": "A name for the restored index",
                        "preselected": True,
                    },
                    "deletion_protection": DeletionProtectionOverlay,
                    "tags": TagsOverlay,
                    "read_capacity": ReadCapacityOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-backup-schedules",
                "method": "GET",
                "path": "/indexes/{index_name}/backup-schedules",
                "operation_id": "list_backup_schedules",
                "display_name": "List Backup Schedules",
                "short_desc": "List the backup schedules of an index",
                "desc": "Lists the backup schedules attached to an index.",
                "groups": (GroupBackups,),
                "field_order": ("index_name",),
                "fields": VersionHeaderFields + PagingFields + {
                    "index_name": IndexNameOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-backup-schedule",
                "method": "POST",
                "path": "/indexes/{index_name}/backup-schedules",
                "operation_id": "create_backup_schedule",
                "display_name": "Create Backup Schedule",
                "short_desc": "Back an index up automatically on a schedule",
                "desc": "Attaches a time-based backup schedule to an index, so it is backed up without "
                    "anyone asking.\n\n"
                    "Serverless and BYOC indexes only; a pod-based index is rejected with **400**.  An "
                    "index may have only one **enabled** schedule at a time.",
                "groups": (GroupBackups,),
                "field_order": ("index_name", "name", "schedule", "retention"),
                "fields": VersionHeaderFields + {
                    "index_name": IndexNameOverlay,
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Schedule Name",
                        "short_desc": "A name for the schedule",
                        "preselected": True,
                    },
                    "schedule": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Schedule",
                        "short_desc": "How often the backup runs",
                        "desc": "How often the backup runs, as "
                            "`{\"type\": \"time-based\", \"frequency\": \"daily\"}`.",
                        # both members are closed sets in the document, and the frequency is the same
                        # one "Update Backup Schedule" offers as a dropdown - it would be odd for the
                        # create to take it as free text
                        "fields": {
                            "type": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Schedule Type",
                                "short_desc": "The kind of schedule",
                                "allowed_values": ScheduleTypeValues,
                            },
                            "frequency": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Frequency",
                                "short_desc": "How often the backup runs",
                                "allowed_values": ScheduleFrequencyValues,
                            },
                        },
                        "preselected": True,
                    },
                    "retention": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Retention",
                        "short_desc": "How long each backup is kept",
                        "desc": "How long each backup is kept, as `{\"expire_after_days\": 30}`.",
                        "preselected": True,
                        "example_value": {"expire_after_days": 90},
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "describe-backup-schedule",
                "method": "GET",
                "path": "/backup-schedules/{schedule_id}",
                "operation_id": "describe_backup_schedule",
                "display_name": "Describe Backup Schedule",
                "short_desc": "Get one backup schedule",
                "desc": "Returns one backup schedule: its frequency, its retention, whether it is "
                    "enabled and when it next runs.",
                "groups": (GroupBackups,),
                "fields": VersionHeaderFields + {
                    "schedule_id": ScheduleIdOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-backup-schedule",
                "method": "PATCH",
                "path": "/backup-schedules/{schedule_id}",
                "operation_id": "update_backup_schedule",
                "display_name": "Update Backup Schedule",
                "short_desc": "Change a backup schedule's frequency, retention or state",
                "desc": "Changes a backup schedule's frequency, its retention, or whether it is "
                    "enabled.\n\n"
                    "Re-enabling a disabled schedule **enqueues a backup immediately**, and is rejected "
                    "with **409** when the index already has another enabled schedule.",
                "groups": (GroupBackups,),
                "field_order": ("schedule_id", "enabled", "frequency", "retention"),
                "fields": VersionHeaderFields + {
                    "schedule_id": ScheduleIdOverlay,
                    "frequency": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Frequency",
                        "short_desc": "How often the backup runs",
                        "allowed_values": ScheduleFrequencyValues,
                        "preselected": True,
                    },
                    "retention": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Retention",
                        "short_desc": "How long each backup is kept",
                        "desc": "How long each backup is kept, as `{\"expire_after_days\": 30}`.",
                        "example_value": {"expire_after_days": 90},
                    },
                    "enabled": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Enabled",
                        "short_desc": "Whether the schedule runs",
                        "desc": "Whether the schedule runs.  Turning it back on enqueues a backup at "
                            "once.",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-backup-schedule",
                "method": "DELETE",
                "path": "/backup-schedules/{schedule_id}",
                "operation_id": "delete_backup_schedule",
                "display_name": "Delete Backup Schedule",
                "short_desc": "Remove a backup schedule",
                "desc": "Removes a backup schedule permanently.  Backups it has already taken are kept.",
                "groups": (GroupBackups,),
                "fields": VersionHeaderFields + {
                    "schedule_id": ScheduleIdOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-backup-schedule-history",
                "method": "GET",
                "path": "/backup-schedules/{schedule_id}/history",
                "operation_id": "list_backup_schedule_history",
                "display_name": "List Backup Schedule History",
                "short_desc": "List the backups a schedule has taken",
                "desc": "Lists the backups a schedule has taken, with the state of each — which is how "
                    "you find out that an automatic backup has been failing.",
                "groups": (GroupBackups,),
                "field_order": ("schedule_id",),
                "fields": VersionHeaderFields + PagingFields + {
                    "schedule_id": ScheduleIdOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-restore-jobs",
                "method": "GET",
                "path": "/restore-jobs",
                "operation_id": "list_restore_jobs",
                "display_name": "List Restore Jobs",
                "short_desc": "List the project's restore jobs",
                "desc": "Lists the restore jobs in the project, with the progress of each.",
                "groups": (GroupBackups,),
                "fields": VersionHeaderFields + PagingFields,
            },
            <RestSchemaActionInfo>{
                "action": "describe-restore-job",
                "method": "GET",
                "path": "/restore-jobs/{job_id}",
                "operation_id": "describe_restore_job",
                "display_name": "Describe Restore Job",
                "short_desc": "Get one restore job and its progress",
                "desc": "Returns one restore job: its state, how far it has got, and the index it is "
                    "restoring into.  This is what a flow polls after **Create Index from Backup**.",
                "groups": (GroupBackups,),
                "fields": VersionHeaderFields + {
                    "job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Restore Job",
                        "short_desc": "The restore job to describe",
                        "ref_data": RefRestoreJob,
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-collections",
                "method": "GET",
                "path": "/collections",
                "operation_id": "list_collections",
                "display_name": "List Collections",
                "short_desc": "List the project's collections (pod-based indexes only)",
                "desc": "Lists the collections in the project.\n\n"
                    "**Collections are legacy.**  They are supported only for pod-based indexes, and a "
                    "pod-based index cannot be created at this API version — so this applies only to a "
                    "project that still has one from an earlier version.  For anything current, use "
                    "**backups** instead.",
                "groups": (GroupCollections,),
                "fields": VersionHeaderFields,
            },
            <RestSchemaActionInfo>{
                "action": "create-collection",
                "method": "POST",
                "path": "/collections",
                "operation_id": "create_collection",
                "display_name": "Create Collection",
                "short_desc": "Snapshot a pod-based index into a collection",
                "desc": "Creates a collection from a pod-based index — a static snapshot of its "
                    "records.\n\n"
                    "**Collections are legacy**: pod-based only, and pod-based indexes cannot be "
                    "created at this API version.  Use **Create Backup** for a serverless index.",
                "groups": (GroupCollections,),
                "field_order": ("name", "source"),
                "fields": VersionHeaderFields + {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Collection Name",
                        "short_desc": "A name for the new collection",
                        "preselected": True,
                    },
                    "source": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Source Index",
                        "short_desc": "The pod-based index to snapshot",
                        "ref_data": RefIndex,
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "describe-collection",
                "method": "GET",
                "path": "/collections/{collection_name}",
                "operation_id": "describe_collection",
                "display_name": "Describe Collection",
                "short_desc": "Get one collection (pod-based indexes only)",
                "desc": "Returns one collection: its size, its state, its dimension and how many "
                    "vectors it holds.\n\n"
                    "**Collections are legacy** — see **List Collections**.",
                "groups": (GroupCollections,),
                "fields": VersionHeaderFields + {
                    "collection_name": CollectionOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-collection",
                "method": "DELETE",
                "path": "/collections/{collection_name}",
                "operation_id": "delete_collection",
                "display_name": "Delete Collection",
                "short_desc": "Delete a collection permanently",
                "desc": "Deletes a collection permanently.  The index it was taken from is not "
                    "affected.\n\n"
                    "**Collections are legacy** — see **List Collections**.",
                "groups": (GroupCollections,),
                "fields": VersionHeaderFields + {
                    "collection_name": CollectionOverlay,
                },
            },
        );

        #! The data-plane operations this application exports
        /** Every one of these is routed to the selected index's own host by @ref HostSources, so every
            one declares \c host_from and gains a synthesized \c index option that is consumed rather
            than sent.

            The set divides into three families that do <b>not</b> mix, and each action's description says
            which index it needs:

            |!Family|!Reads and writes|!Index it needs
            |vectors|ids, values and metadata|a classic vector index (\c _values / \c _sparse_values)
            |records|text, embedded by Pinecone|an index with integrated embedding
            |documents|typed fields declared in the index schema|an index with a field schema
        */
        const DataManifest = (
            <RestSchemaActionInfo>{
                "action": "describe-index-stats",
                "method": "POST",
                "path": "/describe_index_stats",
                "operation_id": "describeIndexStats",
                "display_name": "Get Index Statistics",
                "short_desc": "Count the records in an index, by namespace",
                "desc": "Returns statistics about what an index holds: the record count per namespace, "
                    "the vector dimension and — for a pod-based index — how full it is.\n\n"
                    "Serverless indexes scale automatically, so fullness is meaningless for them.",
                "groups": (GroupVectors,),
                "host_from": HostIndex,
                "fields": VersionHeaderFields + {
                    "filter": FilterOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "query-vectors",
                "method": "POST",
                "path": "/query",
                "operation_id": "queryVectors",
                "display_name": "Search with a Vector",
                "short_desc": "Find the records most similar to a query vector",
                "desc": "Searches a namespace with a query vector and returns the most similar records "
                    "with their scores.\n\n"
                    "Give the query as a **vector**, as a **sparse vector**, or as the **ID** of a "
                    "record already in the index — one of the three, not a mixture: naming an `id` "
                    "excludes both vector forms.\n\n"
                    "This is the classic vectors API.  For an index with integrated embedding use "
                    "**Search with Text**, and for one with a field schema use **Search Documents**.",
                "groups": (GroupVectors,),
                "host_from": HostIndex,
                "field_order": ("index", "namespace", "vector", "id", "topK", "filter",
                    "includeMetadata", "includeValues"),
                "fields": VersionHeaderFields + {
                    "namespace": NamespaceOverlay,
                    "topK": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Result Count",
                        "short_desc": "How many results to return",
                        "desc": "How many results to return, from 1 to 10000.",
                        "preselected": True,
                    },
                    "vector": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Query Vector",
                        "short_desc": "The dense vector to search with",
                        "desc": "The dense vector to search with.  It must have the same number of "
                            "dimensions as the index.\n\n"
                            "Mutually exclusive with **Query by Record ID**.",
                        "exclusive_with": ("id",),
                        "required_groups": ("query",),
                        "preselected": True,
                    },
                    "sparseVector": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Query Sparse Vector",
                        "short_desc": "The sparse vector to search with",
                        "desc": "The sparse vector to search with, as `{\"indices\": [...], "
                            "\"values\": [...]}`.\n\n"
                            "Whether it may be sent together with a dense vector depends on the index: "
                            "one that supports only a single query predicate rejects the combination.",
                        "exclusive_with": ("id",),
                        "required_groups": ("query",),
                    },
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Query by Record ID",
                        "short_desc": "Use an existing record's vector as the query",
                        "desc": "Use the vector of a record already in the index as the query — \"more "
                            "like this one\".\n\n"
                            "Mutually exclusive with both vector forms.",
                        "exclusive_with": ("vector", "sparseVector"),
                        "required_groups": ("query",),
                        "preselected": True,
                    },
                    "filter": FilterOverlay + {"preselected": True},
                    "includeValues": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include Vector Values",
                        "short_desc": "Return each result's vector as well as its ID",
                        "desc": "Return each result's vector as well as its ID.  Leave this off unless "
                            "you need the vectors: on an on-demand index they are read from object "
                            "storage, which is slower, especially with a large result count.",
                    },
                    "includeMetadata": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include Metadata",
                        "short_desc": "Return each result's metadata as well as its ID",
                        "preselected": True,
                    },
                    "scanFactor": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Scan Factor",
                        "short_desc": "How much of the index to scan, trading accuracy for speed",
                        "desc": "How much of the index to scan, from 0.5 to 4.0 (the default).  Lower it "
                            "only if query latency is too high, and expect slightly less accurate "
                            "results.  Dedicated dense indexes only.",
                    },
                    "maxCandidates": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Maximum Candidates",
                        "short_desc": "How many candidates to consider before ranking",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "upsert-vectors",
                "method": "POST",
                "path": "/vectors/upsert",
                "operation_id": "upsertVectors",
                "display_name": "Upsert Vectors",
                "short_desc": "Write records with their vectors into a namespace",
                "desc": "Writes records into a namespace.  A record whose ID already exists is "
                    "**overwritten**.\n\n"
                    "Each record is `{\"id\": ..., \"values\": [...], \"metadata\": {...}}`.  The "
                    "vectors must match the index's dimension.",
                "groups": (GroupVectors,),
                "host_from": HostIndex,
                "field_order": ("index", "namespace", "vectors"),
                "fields": VersionHeaderFields + {
                    "namespace": NamespaceOverlay,
                    "vectors": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Records",
                        "short_desc": "The records to write",
                        "desc": "The records to write, each with an `id`, its `values`, and optionally "
                            "`metadata` and `sparseValues`.",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-vector",
                "method": "POST",
                "path": "/vectors/update",
                "operation_id": "updateVector",
                "display_name": "Update Vectors",
                "short_desc": "Change one record by ID, or many by metadata filter",
                "desc": "Updates records in place.\n\n"
                    "- by **ID**: changes that one record's vector and/or metadata\n"
                    "- by **filter**: changes metadata across every record the filter matches\n\n"
                    "One or the other, not both.  A vector value can only be set by ID — a filtered "
                    "update changes metadata alone.\n\n"
                    "**Set Metadata** merges: the fields named are replaced and the rest are left "
                    "alone.  Use **Dry Run** to count what a filter would touch before touching it.",
                "groups": (GroupVectors,),
                "host_from": HostIndex,
                "field_order": ("index", "namespace", "id", "filter", "values", "setMetadata",
                    "dryRun"),
                "fields": VersionHeaderFields + {
                    "namespace": NamespaceOverlay,
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Record ID",
                        "short_desc": "The single record to update",
                        "desc": "The single record to update.  Mutually exclusive with **Metadata "
                            "Filter**.",
                        "exclusive_with": ("filter",),
                        "required_groups": ("selector",),
                        "preselected": True,
                    },
                    "filter": FilterOverlay + {
                        "short_desc": "Update every record matching this filter",
                        "exclusive_with": ("id",),
                        "required_groups": ("selector",),
                        "preselected": True,
                    },
                    "values": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Vector Values",
                        "short_desc": "A new dense vector for the record",
                        "desc": "A new dense vector for the record, which replaces the previous one.  "
                            "Only valid when updating by **Record ID**.",
                    },
                    "sparseValues": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Sparse Vector Values",
                        "short_desc": "A new sparse vector for the record",
                        "desc": "A new sparse vector for the record.  Only valid when updating by "
                            "**Record ID**.",
                    },
                    "setMetadata": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Set Metadata",
                        "short_desc": "Metadata fields to set, leaving the others alone",
                        "desc": "Metadata fields to set.  Only the fields named are changed; anything "
                            "else on the record is left as it was.  Field names may not begin with `$`.",
                        "preselected": True,
                    },
                    "dryRun": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dry Run",
                        "short_desc": "Count what the filter matches without changing anything",
                        "desc": "Count what the filter matches and change nothing.  Worth doing before "
                            "any filtered update.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-vectors",
                "method": "POST",
                "path": "/vectors/delete",
                "operation_id": "deleteVectors",
                "display_name": "Delete Vectors",
                "short_desc": "Delete records by ID, by filter, or the whole namespace",
                "desc": "Deletes records from one namespace, selected in exactly one of three ways:\n\n"
                    "- **Record IDs** — delete those records\n"
                    "- **Metadata Filter** — delete every record the filter matches\n"
                    "- **Delete Everything** — empty the namespace\n\n"
                    "**This cannot be undone.**",
                "groups": (GroupVectors,),
                "host_from": HostIndex,
                "field_order": ("index", "namespace", "ids", "filter", "deleteAll"),
                "fields": VersionHeaderFields + {
                    "namespace": NamespaceOverlay,
                    "ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Record IDs",
                        "short_desc": "The records to delete",
                        "desc": "The records to delete, at most 1000 per call.",
                        "exclusive_with": ("filter", "deleteAll"),
                        "required_groups": ("selector",),
                        "preselected": True,
                    },
                    "filter": FilterOverlay + {
                        "short_desc": "Delete every record matching this filter",
                        "exclusive_with": ("ids", "deleteAll"),
                        "required_groups": ("selector",),
                        "preselected": True,
                    },
                    "deleteAll": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Delete Everything",
                        "short_desc": "Delete every record in the namespace",
                        "desc": "Delete every record in the namespace.  Cannot be combined with "
                            "**Record IDs** or a **Metadata Filter**.",
                        "exclusive_with": ("ids", "filter"),
                        "required_groups": ("selector",),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "fetch-vectors",
                "method": "GET",
                "path": "/vectors/fetch",
                "operation_id": "fetchVectors",
                "display_name": "Fetch Vectors",
                "short_desc": "Read records by ID",
                "desc": "Reads records by ID, returning their vectors and metadata.\n\n"
                    "On an on-demand index the vectors come from object storage, so this can be slow.  "
                    "If only the metadata or the IDs matter, **Search with a Vector** with vector "
                    "values turned off is cheaper.",
                "groups": (GroupVectors,),
                "host_from": HostIndex,
                "field_order": ("index", "namespace", "ids"),
                "fields": VersionHeaderFields + {
                    "namespace": NamespaceOverlay,
                    "ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Record IDs",
                        "short_desc": "The records to read",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "fetch-vectors-by-metadata",
                "method": "POST",
                "path": "/vectors/fetch_by_metadata",
                "operation_id": "fetch_vectors_by_metadata",
                "display_name": "Fetch Vectors by Metadata",
                "short_desc": "Read every record matching a metadata filter",
                "desc": "Reads every record matching a metadata filter, a page at a time, returning "
                    "their vectors and metadata.\n\n"
                    "Use this to walk a subset of a namespace when you know the metadata but not the "
                    "IDs.",
                "groups": (GroupVectors,),
                "host_from": HostIndex,
                "field_order": ("index", "namespace", "filter", "limit"),
                "fields": VersionHeaderFields + {
                    "namespace": NamespaceOverlay,
                    "filter": FilterOverlay + {"preselected": True},
                    "limit": LimitOverlay,
                    "paginationToken": PaginationTokenOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-vector-ids",
                "method": "GET",
                "path": "/vectors/list",
                "operation_id": "listVectors",
                "display_name": "List Record IDs",
                "short_desc": "List the record IDs in a namespace",
                "desc": "Lists the IDs of the records in a namespace, in sorted order, without reading "
                    "their vectors — so it is much cheaper than fetching them.\n\n"
                    "**Prefix** narrows it to IDs starting with a given string, which is how a flow "
                    "walks the chunks of one source document when the IDs are built that way.\n\n"
                    "Serverless indexes only.",
                "groups": (GroupVectors,),
                "host_from": HostIndex,
                "field_order": ("index", "namespace", "prefix", "limit"),
                "fields": VersionHeaderFields + {
                    "namespace": NamespaceOverlay,
                    "prefix": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID Prefix",
                        "short_desc": "Only list IDs starting with this",
                        "preselected": True,
                    },
                    "limit": LimitOverlay,
                    "paginationToken": PaginationTokenOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-namespaces",
                "method": "GET",
                "path": "/namespaces",
                "operation_id": "listNamespacesOperation",
                "display_name": "List Namespaces",
                "short_desc": "List the namespaces in an index",
                "desc": "Lists the namespaces in an index, with the record count of each.\n\n"
                    "Serverless indexes only.",
                "groups": (GroupNamespaces,),
                "host_from": HostIndex,
                "field_order": ("index", "prefix", "limit"),
                "fields": VersionHeaderFields + {
                    "prefix": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name Prefix",
                        "short_desc": "Only list namespaces starting with this",
                    },
                    "limit": LimitOverlay,
                    "paginationToken": PaginationTokenOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-namespace",
                "method": "POST",
                "path": "/namespaces",
                "operation_id": "createNamespace",
                "display_name": "Create Namespace",
                "short_desc": "Create a namespace in an index",
                "desc": "Creates a namespace in an index.\n\n"
                    "A namespace does not normally have to be created — writing to one brings it into "
                    "existence — so create one explicitly when you want to declare its **metadata "
                    "schema** up front.\n\n"
                    "Serverless indexes only.  `__default__` is reserved and always exists.",
                "groups": (GroupNamespaces,),
                "host_from": HostIndex,
                "field_order": ("index", "name", "schema"),
                "fields": VersionHeaderFields + {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Namespace Name",
                        "short_desc": "A name for the new namespace",
                        "desc": "A name for the new namespace: ASCII, 1 to 512 characters.  "
                            "`__default__` is reserved.",
                        "preselected": True,
                    },
                    "schema": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Metadata Schema",
                        "short_desc": "Which metadata fields are indexed for filtering",
                        "desc": "Which metadata fields are indexed for filtering, as "
                            "`{\"fields\": {\"<name>\": {\"filterable\": true}}}`.  By default every "
                            "metadata field is indexed.  Only `filterable: true` is accepted; to leave "
                            "a field unindexed, omit it.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "describe-namespace",
                "method": "GET",
                "path": "/namespaces/{namespace}",
                "operation_id": "describeNamespace",
                "display_name": "Describe Namespace",
                "short_desc": "Get one namespace and its record count",
                "desc": "Returns one namespace: how many records it holds, its size and its metadata "
                    "schema.\n\n"
                    "Serverless indexes only.",
                "groups": (GroupNamespaces,),
                "host_from": HostIndex,
                "field_order": ("index", "namespace"),
                "fields": VersionHeaderFields + {
                    "namespace": NamespacePathOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-namespace",
                "method": "DELETE",
                "path": "/namespaces/{namespace}",
                "operation_id": "deleteNamespace",
                "display_name": "Delete Namespace",
                "short_desc": "Delete a namespace and everything in it",
                "desc": "Deletes a namespace and every record in it.  **This cannot be undone.**\n\n"
                    "Serverless indexes only.",
                "groups": (GroupNamespaces,),
                "host_from": HostIndex,
                "field_order": ("index", "namespace"),
                "fields": VersionHeaderFields + {
                    "namespace": NamespacePathOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "upsert-records",
                "method": "POST",
                "path": "/records/namespaces/{namespace}/upsert",
                "operation_id": "upsertRecordsNamespace",
                "display_name": "Upsert Text",
                "short_desc": "Write text records and let Pinecone embed them",
                "desc": "Writes records as **text**, which Pinecone converts to vectors with the index's "
                    "own embedding model — so no embedding step is needed in the flow.\n\n"
                    "Requires an index created with **Create Index with Integrated Embedding**; any "
                    "other index rejects the request with **400**, and it is not available on BYOC.\n\n"
                    "Each record needs an `_id` and the text field named in the index's field map; "
                    "anything else is stored as metadata.",
                "groups": (GroupNamespaces, GroupDocuments),
                "host_from": HostIndex,
                "field_order": ("index", "namespace", "records"),
                "fields": VersionHeaderFields + {
                    "namespace": NamespacePathOverlay,
                    "body": <RestSchemaFieldOverlayInfo>{
                        "name": "records",
                        "display_name": "Records",
                        "short_desc": "The text records to write",
                        "desc": "The text records to write.  Each needs an `_id` and the field named in "
                            "the index's field map; any other field is stored as metadata.",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "search-records",
                "method": "POST",
                "path": "/records/namespaces/{namespace}/search",
                "operation_id": "searchRecordsNamespace",
                "display_name": "Search with Text",
                "short_desc": "Search with a query text and optionally rerank",
                "desc": "Searches a namespace with **text**, a vector, or the ID of an existing record, "
                    "and returns the most similar records with their scores.\n\n"
                    "Searching with text requires an index with integrated embedding; the vector and "
                    "record-ID forms work on any index.\n\n"
                    "**Rerank** runs a second, more accurate model over the first results — the usual "
                    "way to improve relevance without widening the search.",
                "groups": (GroupNamespaces, GroupDocuments),
                "host_from": HostIndex,
                "field_order": ("index", "namespace", "inputs", "top_k", "filter", "fields", "rerank"),
                "fields": VersionHeaderFields + {
                    "namespace": NamespacePathOverlay,
                    # the query is a grouping object: it holds the result count and the filter beside
                    # the query itself, and publishing it whole asks a user to build a hash by hand
                    "query": <RestSchemaFieldOverlayInfo>{
                        "lift": True,
                        "fields": {
                            "top_k": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Result Count",
                                "short_desc": "How many results to return",
                                "desc": "How many results to return, from 1 to 10000.",
                                "example_value": 10,
                                "preselected": True,
                            },
                            "inputs": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Query Text",
                                "short_desc": "The text to search with",
                                "desc": "The text to search with, as `{\"text\": \"...\"}`.  Requires "
                                    "an index with integrated embedding.",
                                "example_value": {"text": "your query text"},
                                "preselected": True,
                            },
                            # the vendor's "SearchRecordsVector" declares no description of its
                            # own, so without this the option would be the only one in the
                            # application presented with no explanation at all
                            "vector": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Query Vector",
                                "short_desc": "A vector to search with instead of text",
                                "desc": "A vector to search with instead of text, for an index "
                                    "without integrated embedding — or when the embedding is "
                                    "computed elsewhere.\n\n"
                                    "Dense values go in `values`; a sparse vector is given as "
                                    "`sparse_values` with the matching `sparse_indices`.",
                                "example_value": {"values": (0.1, 0.2, 0.3)},
                            },
                            "id": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Query by Record ID",
                                "short_desc": "Use an existing record as the query",
                                "example_value": "example-vector-1",
                            },
                            # the merge yields "hash<auto>", which the strictly-typed nested "fields"
                            # map will not take, so the result is cast back to the overlay type
                            "filter": cast<hash<RestSchemaFieldOverlayInfo>>(FilterOverlay
                                + {"preselected": True}),
                            "match_terms": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Required Terms",
                                "short_desc": "Terms every result must contain",
                                "fields": {
                                    "strategy": <RestSchemaFieldOverlayInfo>{
                                        "display_name": "Match Strategy",
                                        "short_desc": "How the terms are matched",
                                        "allowed_values": MatchStrategyValues,
                                    },
                                },
                            },
                        },
                    },
                    "fields": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Fields to Return",
                        "short_desc": "Which record fields to return on each hit",
                        "preselected": True,
                    },
                    "rerank": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Rerank",
                        "short_desc": "Re-order the results with a reranking model",
                        "desc": "Re-order the results with a reranking model, as "
                            "`{\"model\": \"...\", \"rank_fields\": [\"...\"], \"top_n\": 10}`.\n\n"
                            "**List Models** with the type `rerank` lists the models available.",
                        "fields": {
                            "model": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Reranking Model",
                                "short_desc": "The model that re-orders the results",
                                "ref_data": RefRerankModel,
                                "supports_custom_values": True,
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "upsert-documents",
                "method": "POST",
                "path": "/namespaces/{namespace}/documents/upsert",
                "operation_id": "upsertDocuments",
                "display_name": "Upsert Documents",
                "short_desc": "Write documents into a namespace",
                "desc": "Writes documents into a namespace.  A document whose `_id` already exists is "
                    "overwritten.\n\n"
                    "Each document needs an `_id` and at least one field declared in the index's "
                    "schema.  Any other field is stored as metadata, returned by **Fetch Documents**, "
                    "and indexed for filtering automatically.",
                "groups": (GroupDocuments,),
                "host_from": HostIndex,
                "field_order": ("index", "namespace", "documents"),
                "fields": VersionHeaderFields + {
                    "namespace": NamespacePathOverlay,
                    "documents": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Documents",
                        "short_desc": "The documents to write",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "search-documents",
                "method": "POST",
                "path": "/namespaces/{namespace}/documents/search",
                "operation_id": "searchDocuments",
                "display_name": "Search Documents",
                "short_desc": "Search documents by vector, text or query string",
                "desc": "Searches documents in a namespace and returns the best matches with their "
                    "scores.\n\n"
                    "**Score By** chooses how they are scored, and is the whole point of this action:\n"
                    "- `{\"type\": \"dense_vector\", \"fields\": [\"embedding\"], \"values\": [...]}`\n"
                    "- `{\"type\": \"sparse_vector\", \"fields\": [\"sparse\"], \"sparse_values\": "
                    "{...}}`\n"
                    "- `{\"type\": \"text\", \"fields\": [\"body\"], \"query\": \"...\"}` — BM25\n"
                    "- `{\"type\": \"query_string\", \"query\": \"title:(cat OR dog)\"}` — Lucene "
                    "syntax; naming a field as well is rejected\n\n"
                    "Passing a **list** of these scores by several methods at once, which is how hybrid "
                    "search is expressed here.",
                "groups": (GroupDocuments,),
                "host_from": HostIndex,
                "field_order": ("index", "namespace", "score_by", "top_k", "filter",
                    "include_fields"),
                "fields": VersionHeaderFields + {
                    "namespace": NamespacePathOverlay,
                    "score_by": <RestSchemaFieldOverlayInfo>{
                        # the clause type is an "x-enum" like every other value set in this API, and it
                        # decides which other members of the clause are needed, so it is restored on the
                        # element rather than left as free text
                        "fields": {
                            "type": <RestSchemaFieldOverlayInfo>{
                                "display_name": "Scoring Method",
                                "short_desc": "How this clause scores documents",
                                "allowed_values": ScoringMethodValues,
                            },
                        },
                        "display_name": "Score By",
                        "short_desc": "How documents are scored against the query",
                        "desc": "How documents are scored: one scoring method, or a list of them for "
                            "hybrid search.  The `type` is one of `dense_vector`, `sparse_vector`, "
                            "`text` or `query_string`, and decides which other members are needed — "
                            "see this action's description.",
                        "preselected": True,
                    },
                    "top_k": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Result Count",
                        "short_desc": "How many documents to return",
                        "preselected": True,
                    },
                    "filter": FilterOverlay + {"preselected": True},
                    "include_fields": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Fields to Return",
                        "short_desc": "Which document fields to return on each hit",
                        "desc": "Which document fields to return on each hit.  Empty returns every "
                            "field, as does `[\"*\"]`.",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "fetch-documents",
                "method": "POST",
                "path": "/namespaces/{namespace}/documents/fetch",
                "operation_id": "fetchDocuments",
                "display_name": "Fetch Documents",
                "short_desc": "Read documents by ID or by metadata filter",
                "desc": "Reads documents from a namespace, selected either by **ID** or by a "
                    "**metadata filter** — exactly one of the two.\n\n"
                    "A fetch by filter is paged; a fetch by IDs is already bounded by the ID list and "
                    "returns them all.",
                "groups": (GroupDocuments,),
                "host_from": HostIndex,
                "field_order": ("index", "namespace", "ids", "filter", "include_fields", "limit"),
                "fields": VersionHeaderFields + {
                    "namespace": NamespacePathOverlay,
                    "ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Document IDs",
                        "short_desc": "The documents to read",
                        "desc": "The documents to read, at most 1000.  Mutually exclusive with "
                            "**Metadata Filter**.",
                        "exclusive_with": ("filter",),
                        "required_groups": ("selector",),
                        "preselected": True,
                    },
                    "filter": FilterOverlay + {
                        "short_desc": "Read every document matching this filter",
                        "exclusive_with": ("ids",),
                        "required_groups": ("selector",),
                        "preselected": True,
                    },
                    "include_fields": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Fields to Return",
                        "short_desc": "Which document fields to return",
                        "desc": "Which document fields to return.  Empty returns every field, as does "
                            "`[\"*\"]`.",
                        "preselected": True,
                    },
                    "limit": LimitOverlay + {
                        "desc": "How many documents to return per page, from 1 to 10000 (100 by "
                            "default).  Applies to a fetch by filter; a fetch by IDs ignores an "
                            "in-range value, but a value outside the range is rejected either way.",
                    },
                    "pagination_token": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Pagination Token",
                        "short_desc": "The pagination token from the previous response",
                        "desc": "The pagination token from the previous response.  Only valid together "
                            "with a **Metadata Filter**.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-documents",
                "method": "POST",
                "path": "/namespaces/{namespace}/documents/update",
                "operation_id": "updateDocuments",
                "display_name": "Update Documents",
                "short_desc": "Patch documents by ID, or set fields across a filter",
                "desc": "Applies partial updates to documents, selected in one of two ways:\n\n"
                    "- **Documents** — one patch per document, each identified by its `_id`.  Fields "
                    "you set take new values, fields listed in `_remove_fields` are removed, and "
                    "anything unmentioned is left alone.  A patch for a document that does not exist is "
                    "ignored.\n"
                    "- **Metadata Filter** — with **Set Fields** and/or **Remove Fields**, applied to "
                    "every document the filter matches.\n\n"
                    "One or the other, not both; a filtered update must say what to change.",
                "groups": (GroupDocuments,),
                "host_from": HostIndex,
                "field_order": ("index", "namespace", "documents", "filter", "set_fields",
                    "remove_fields"),
                "fields": VersionHeaderFields + {
                    "namespace": NamespacePathOverlay,
                    "documents": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Documents",
                        "short_desc": "One patch per document, each identified by its _id",
                        "exclusive_with": ("filter", "set_fields", "remove_fields"),
                        "required_groups": ("selector",),
                        "preselected": True,
                    },
                    "filter": FilterOverlay + {
                        "short_desc": "Update every document matching this filter",
                        "exclusive_with": ("documents",),
                        "required_groups": ("selector",),
                        "preselected": True,
                    },
                    "set_fields": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Set Fields",
                        "short_desc": "Fields to set on every matching document",
                        "desc": "Fields to set on every document the filter matches, and the values to "
                            "set them to.  Only valid with a **Metadata Filter**.",
                        "exclusive_with": ("documents",),
                        "preselected": True,
                    },
                    "remove_fields": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Remove Fields",
                        "short_desc": "Fields to remove from every matching document",
                        "desc": "Fields to remove from every document the filter matches.  Only valid "
                            "with a **Metadata Filter**.",
                        "exclusive_with": ("documents",),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-documents",
                "method": "POST",
                "path": "/namespaces/{namespace}/documents/delete",
                "operation_id": "deleteDocuments",
                "display_name": "Delete Documents",
                "short_desc": "Delete documents by ID, by filter, or the whole namespace",
                "desc": "Deletes documents from a namespace, selected in exactly one of three ways:\n\n"
                    "- **Document IDs** — delete those documents\n"
                    "- **Metadata Filter** — delete every document the filter matches; the response "
                    "reports how many matched\n"
                    "- **Delete Everything** — empty the namespace\n\n"
                    "A filter here selects on metadata alone: the text-match operators "
                    "(`$match_phrase`, `$match_all`, `$match_any`) are accepted in search but not "
                    "here.\n\n"
                    "**This cannot be undone.**",
                "groups": (GroupDocuments,),
                "host_from": HostIndex,
                "field_order": ("index", "namespace", "ids", "filter", "delete_all"),
                "fields": VersionHeaderFields + {
                    "namespace": NamespacePathOverlay,
                    "ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Document IDs",
                        "short_desc": "The documents to delete",
                        "desc": "The documents to delete, at most 1000 per call.",
                        "exclusive_with": ("filter", "delete_all"),
                        "required_groups": ("selector",),
                        "preselected": True,
                    },
                    "filter": FilterOverlay + {
                        "short_desc": "Delete every document matching this filter",
                        "desc": "Delete every document matching this metadata filter.  An empty filter "
                            "is rejected rather than matching everything — use **Delete Everything** "
                            "for that.  Text-match operators are not accepted here.",
                        "exclusive_with": ("ids", "delete_all"),
                        "required_groups": ("selector",),
                        "preselected": True,
                    },
                    "delete_all": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Delete Everything",
                        "short_desc": "Delete every document in the namespace",
                        "exclusive_with": ("ids", "filter"),
                        "required_groups": ("selector",),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-documents",
                "method": "POST",
                "path": "/namespaces/{namespace}/documents/list",
                "operation_id": "listDocuments",
                "display_name": "List Documents",
                "short_desc": "List the documents in a namespace",
                "desc": "Lists the documents in a namespace, in sorted order.  **Prefix** narrows it to "
                    "documents whose ID starts with a given string.",
                "groups": (GroupDocuments,),
                "host_from": HostIndex,
                "field_order": ("index", "namespace", "prefix", "limit"),
                "fields": VersionHeaderFields + {
                    "namespace": NamespacePathOverlay,
                    "prefix": <RestSchemaFieldOverlayInfo>{
                        "display_name": "ID Prefix",
                        "short_desc": "Only list documents whose ID starts with this",
                        "preselected": True,
                    },
                    "limit": LimitOverlay,
                    "pagination_token": PaginationTokenOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-imports",
                "method": "GET",
                "path": "/bulk/imports",
                "operation_id": "listBulkImports",
                "display_name": "List Imports",
                "short_desc": "List an index's bulk imports",
                "desc": "Lists the recent and running bulk imports of an index, with the progress of "
                    "each.",
                "groups": (GroupImports,),
                "host_from": HostIndex,
                "field_order": ("index", "limit"),
                "fields": VersionHeaderFields + {
                    "limit": LimitOverlay,
                    "paginationToken": PaginationTokenOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "start-import",
                "method": "POST",
                "path": "/bulk/imports",
                "operation_id": "startBulkImport",
                "display_name": "Start Import",
                "short_desc": "Import records into an index from object storage",
                "desc": "Starts an asynchronous import of records from object storage into an index — "
                    "the way to load a large dataset without sending it through this API record by "
                    "record.\n\n"
                    "The **URI** names the storage prefix to read, and an **Integration** supplies the "
                    "credentials to read it with, configured in the Pinecone console.\n\n"
                    "This returns an import ID; **Describe Import** reports the progress.",
                "groups": (GroupImports,),
                "host_from": HostIndex,
                "field_order": ("index", "uri", "integrationId", "errorMode"),
                "fields": VersionHeaderFields + {
                    "uri": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Source URI",
                        "short_desc": "The object-storage prefix to import from",
                        "desc": "The object-storage prefix to import from, such as "
                            "`s3://my-bucket/my-prefix/`.",
                        "preselected": True,
                    },
                    "integrationId": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Storage Integration",
                        "short_desc": "The stored credentials to read the bucket with",
                        "desc": "The ID of the storage integration holding the credentials to read the "
                            "bucket with, configured in the "
                            "[Pinecone console](https://app.pinecone.io/).  Not needed for a publicly "
                            "readable bucket.",
                        "preselected": True,
                    },
                    "errorMode": <RestSchemaFieldOverlayInfo>{
                        "display_name": "On Error",
                        "short_desc": "Whether a bad record aborts the import",
                        "desc": "What to do when a record fails, as `{\"onError\": \"continue\"}`.  "
                            "`continue` skips it and imports the rest; `abort` stops the whole import.",
                        "fields": {
                            "onError": <RestSchemaFieldOverlayInfo>{
                                "display_name": "On Error",
                                "short_desc": "Whether a bad record aborts the import",
                                "allowed_values": ImportErrorModeValues,
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "describe-import",
                "method": "GET",
                "path": "/bulk/imports/{id}",
                "operation_id": "describeBulkImport",
                "display_name": "Describe Import",
                "short_desc": "Get one import and its progress",
                "desc": "Returns one bulk import: its state, how far it has got, how many records it "
                    "has written, and why it failed if it did.",
                "groups": (GroupImports,),
                "host_from": HostIndex,
                "field_order": ("index", "id"),
                "fields": VersionHeaderFields + {
                    "id": ImportIdOverlay,
                },
            },
            <RestSchemaActionInfo>{
                "action": "cancel-import",
                "method": "DELETE",
                "path": "/bulk/imports/{id}",
                "operation_id": "cancelBulkImport",
                "display_name": "Cancel Import",
                "short_desc": "Stop an import that is still running",
                "desc": "Stops a bulk import that has not finished.  It has no effect on one that "
                    "has.\n\n"
                    "Records already written are **kept**; cancelling does not roll them back.",
                "groups": (GroupImports,),
                "host_from": HostIndex,
                "field_order": ("index", "id"),
                "fields": VersionHeaderFields + {
                    "id": ImportIdOverlay,
                },
            },
        );

        #! The inference operations this application exports
        /** Embedding and reranking as standalone calls, plus the model catalogue that backs both model
            dropdowns.  These need no index: they are Pinecone's hosted models used directly, which is
            what makes them useful beside a vector store that is not Pinecone's.

            \c embed and \c rerank declare an <b>optional</b> request body while marking the properties
            inside it required, so everything generates optional; the overlay narrows the ones the API
            actually needs back to required, which an overlay is allowed to do because it is a
            narrowing.
        */
        const InferenceManifest = (
            <RestSchemaActionInfo>{
                "action": "embed",
                "method": "POST",
                "path": "/embed",
                "operation_id": "embed",
                "display_name": "Generate Embeddings",
                "short_desc": "Turn text into vectors with a hosted model",
                "desc": "Turns text into vectors using one of Pinecone's hosted embedding models.\n\n"
                    "This is the standalone form: it writes nothing to an index, so it is what to use "
                    "to embed a query before **Search with a Vector**, or to embed text for a store "
                    "that is not Pinecone.  To have Pinecone embed on the way in, create the index "
                    "with integrated embedding instead.",
                "groups": (GroupInference,),
                "field_order": ("model", "inputs", "parameters"),
                "fields": VersionHeaderFields + {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "The embedding model to use",
                        "ref_data": RefEmbedModel,
                        "supports_custom_values": True,
                        "required": True,
                        "preselected": True,
                    },
                    "inputs": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Inputs",
                        "short_desc": "The texts to embed",
                        "desc": "The texts to embed, each as `{\"text\": \"...\"}`.",
                        "required": True,
                        "preselected": True,
                        "example_value": ({"text": "The quick brown fox jumps over the lazy dog."},),
                    },
                    "parameters": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model Parameters",
                        "short_desc": "Model-specific parameters",
                        "desc": "Model-specific parameters, such as `{\"input_type\": \"query\", "
                            "\"truncate\": \"END\"}`.  Which ones a model accepts is reported by "
                            "**Describe Model**.\n\n"
                            "`input_type` matters: several models embed a **query** differently from a "
                            "**passage**, and using the wrong one quietly costs relevance.",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "rerank",
                "method": "POST",
                "path": "/rerank",
                "operation_id": "rerank",
                "display_name": "Rerank Results",
                "short_desc": "Re-order documents by relevance to a query",
                "desc": "Re-orders a list of documents by how relevant each is to a query, using a "
                    "hosted reranking model.\n\n"
                    "The usual use is a second pass over search results: retrieve more than you need "
                    "with a vector search, then rerank and keep the best.  A reranker reads the query "
                    "and the document together, so it is more accurate than vector similarity and too "
                    "slow to run over a whole index.",
                "groups": (GroupInference,),
                "field_order": ("model", "query", "documents", "rank_fields", "top_n",
                    "return_documents"),
                "fields": VersionHeaderFields + {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "The reranking model to use",
                        "ref_data": RefRerankModel,
                        "supports_custom_values": True,
                        "required": True,
                        "preselected": True,
                    },
                    "query": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Query",
                        "short_desc": "The query the documents are ranked against",
                        "required": True,
                        "preselected": True,
                        "example_value": "What is the capital of France?",
                    },
                    "documents": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Documents",
                        "short_desc": "The documents to re-order",
                        "desc": "The documents to re-order, each a hash carrying at least the field "
                            "named in **Rank Fields**.",
                        "required": True,
                        "preselected": True,
                    },
                    "rank_fields": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Rank Fields",
                        "short_desc": "Which document field the model reads",
                        "desc": "Which document field the model reads.  Most models take exactly one.",
                        "preselected": True,
                        "example_value": ("chunk_text",),
                    },
                    "top_n": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Result Count",
                        "short_desc": "How many of the reranked documents to return",
                        "desc": "How many of the reranked documents to return.  Defaults to all of "
                            "them.",
                        "preselected": True,
                    },
                    "return_documents": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Return Documents",
                        "short_desc": "Return each document, not just its index and score",
                    },
                    "parameters": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model Parameters",
                        "short_desc": "Model-specific parameters",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-models",
                "method": "GET",
                "path": "/models",
                "operation_id": "list_models",
                "display_name": "List Models",
                "short_desc": "List the embedding and reranking models Pinecone hosts",
                "desc": "Lists the models Pinecone hosts, with the vector type, dimensions and "
                    "parameters each supports.\n\n"
                    "This is also what fills the model dropdowns on the embedding, reranking and "
                    "integrated-embedding index actions, so the choices offered are whatever Pinecone "
                    "currently serves rather than a list fixed when this module was written.",
                "groups": (GroupInference,),
                "fields": VersionHeaderFields + {
                    "type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model Type",
                        "short_desc": "List only embedding or only reranking models",
                        # the description is given explicitly because the parameter's own description and
                        # its schema's say the same thing in different words, and joining them produces
                        # "Filter models by type ('embed' or 'rerank').: Filter models by type.  Possible
                        # values: ...".  The join only drops a duplicate when one side contains the
                        # other, which near-duplicates like these do not.
                        "desc": "List only embedding models or only reranking models.  Leave it empty to "
                            "list both.",
                        "allowed_values": ModelTypeValues,
                        "preselected": True,
                    },
                    "vector_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Vector Type",
                        "short_desc": "List only dense or only sparse embedding models",
                        "desc": "List only dense or only sparse embedding models.  Applies to embedding "
                            "models alone.",
                        "allowed_values": VectorTypeValues,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "describe-model",
                "method": "GET",
                "path": "/models/{model_name}",
                "operation_id": "get_model",
                "display_name": "Describe Model",
                "short_desc": "Get one hosted model and what it supports",
                "desc": "Returns one hosted model: what it does, the vector type and dimensions it "
                    "produces, its maximum input length and batch size, and **which parameters it "
                    "accepts** — which is where to look before setting **Model Parameters** on an "
                    "embedding or reranking call.",
                "groups": (GroupInference,),
                "fields": VersionHeaderFields + {
                    "model_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "The model to describe",
                        "ref_data": RefEmbedModel,
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
            },
        );
    }
}
}
