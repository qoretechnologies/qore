# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ExaDataProvider module: the exported Exa Public API operations

/** ExaManifest.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace ExaDataProvider {
#! The operations this application exports from the Exa Public API
/** Action IDs are a product commitment: an upstream rename is reported as drift and never renames an
    action here.  \c search, \c find-similar and \c contents predate this manifest and keep the IDs they
    were published with.

    @par What the overlay supplies

    Exa's document types nearly everything correctly, and the two things it does not are both worth
    knowing:

    - Six fields lose their enum to the <tt>anyOf: [enum, null]</tt> pattern - a nullable enum, which is
      the vendor saying "one of these, or nothing".  The values are restored, and the fields are not
      creatable, because Exa rejects anything else.
    - Every collection answers the same <tt>{data, hasMore, nextCursor}</tt> envelope and pages with
      \c cursor and \c limit, but nothing in the document says the identifiers in one operation are the
      objects another one lists.  The reference data below is what turns them into dropdowns.

    @par What is not exported

    The 39 Websets operations - everything the document paths under \c /v0 - are <b>not</b> exported,
    because those paths do not exist on the server the document names.  The document declares
    \c "servers: [https://api.exa.ai]" and paths such as \c /v0/websets, and every one of them answers
    \c 404 there; the Websets API is served under \c /websets instead, and not uniformly:
    \c /websets/websets and \c /websets/imports answer, \c /websets/webhooks and \c /websets/events
    do not.  Generating actions from those paths would have shipped 39 actions that fail on their first
    call, and guessing the real ones is not something a schema-driven application should do.  Exporting
    them needs a document whose paths match the service.

    Two of Exa's own declarations are worth recording while that stands.  \c "POST /v0/websets/preview"
    declares its \c search argument as a <b>path</b> parameter with \c "required: false", on a path with
    no \c {search} template variable, which is not a valid %OpenAPI document at all and does not load;
    and \c "GET /v0/teams/me", the natural thing for a connection to ping, is one of the paths that
    answers \c 404.

    What is exported is the search, research-agent, batch and monitor surface, all of it verified
    against the live API.  That includes \c "POST /findSimilar", which the vendor marks deprecated in
    favour of \c /search - the action predates the deprecation and removing it would break existing
    flows, so it is kept and its description says what to prefer.

    @since %ExaDataProvider 1.1
*/
public class ExaManifest {
    public {
        #! Action groups
        const GroupSearch = "Search";
        const GroupAgent = "Research Agent";
        const GroupBatches = "Batches";
        const GroupMonitors = "Search Monitors";

        #! Reference data names
        const RefAgentRun = "exa-agent-runs";
        const RefBatch = "exa-batches";
        const RefMonitor = "exa-monitors";
        const RefMonitorRun = "exa-monitor-runs";

        #! How many results a listing returns per page
        const PageSizeOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Page Size",
            "short_desc": "How many results to return per page",
        };

        #! Where a listing resumes from
        const CursorOverlay = <RestSchemaFieldOverlayInfo>{
            "display_name": "Cursor",
            "short_desc": "The nextCursor of the previous response",
            "desc": "The `nextCursor` of the previous response.  Every collection in this API pages the "
                "same way: read `hasMore`, and pass `nextCursor` here to get the following page.",
        };

        #! The two paging fields every collection in this API takes
        const PagingFields = {
            "limit": PageSizeOverlay,
            "cursor": CursorOverlay,
        };

        #! What a search may be narrowed to, restored from the schema's own enum
        /** Written as <tt>anyOf: [enum, null]</tt>, so the values are lost in generation.
        */
        const CategoryValues = (
            <AllowedValueInfo>{"value": "company", "display_name": "Companies"},
            <AllowedValueInfo>{"value": "publication", "display_name": "Publications"},
            <AllowedValueInfo>{"value": "news", "display_name": "News"},
            <AllowedValueInfo>{"value": "personal site", "display_name": "Personal Sites"},
            <AllowedValueInfo>{"value": "financial report", "display_name": "Financial Reports"},
            <AllowedValueInfo>{"value": "people", "display_name": "People"},
        );

        #! How hard Exa works on a search, restored from the schema's own enum
        const SearchTypeValues = (
            <AllowedValueInfo>{"value": "instant", "display_name": "Instant"},
            <AllowedValueInfo>{"value": "fast", "display_name": "Fast"},
            <AllowedValueInfo>{"value": "auto", "display_name": "Automatic"},
            <AllowedValueInfo>{"value": "deep-lite", "display_name": "Deep (Lite)"},
            <AllowedValueInfo>{"value": "deep", "display_name": "Deep"},
            <AllowedValueInfo>{"value": "deep-reasoning", "display_name": "Deep with Reasoning"},
        );

        #! The compliance modes Exa offers, restored from the schema's own enum
        const ComplianceValues = (
            <AllowedValueInfo>{"value": "hipaa", "display_name": "HIPAA"},
        );

        #! When Exa fetches a page rather than serving it from cache, restored from the schema's enum
        const LivecrawlValues = (
            <AllowedValueInfo>{"value": "never", "display_name": "Never (cache only)"},
            <AllowedValueInfo>{"value": "fallback", "display_name": "Only if not cached"},
            <AllowedValueInfo>{"value": "preferred", "display_name": "Prefer a fresh crawl"},
            <AllowedValueInfo>{"value": "always", "display_name": "Always crawl"},
        );



        #! The models that write an answer, named rather than shown as raw tokens
        const AnswerModelValues = (
            <AllowedValueInfo>{"value": "exa-fast",     "display_name": "Fast"},
            <AllowedValueInfo>{"value": "exa",          "display_name": "Standard"},
            <AllowedValueInfo>{"value": "exa-pro",      "display_name": "Pro"},
            <AllowedValueInfo>{"value": "exa-research", "display_name": "Research"},
        );

        #! How much work a research run does, named rather than shown as raw tokens
        const EffortValues = (
            <AllowedValueInfo>{"value": "minimal", "display_name": "Minimal"},
            <AllowedValueInfo>{"value": "low",     "display_name": "Low"},
            <AllowedValueInfo>{"value": "medium",  "display_name": "Medium"},
            <AllowedValueInfo>{"value": "high",    "display_name": "High"},
            <AllowedValueInfo>{"value": "xhigh",   "display_name": "Very High"},
            <AllowedValueInfo>{"value": "auto",    "display_name": "Automatic"},
            <AllowedValueInfo>{"value": "max",     "display_name": "Maximum"},
        );

        #! Search monitor statuses, named rather than shown as raw tokens
        const MonitorStatusValues = (
            <AllowedValueInfo>{"value": "active",   "display_name": "Active"},
            <AllowedValueInfo>{"value": "paused",   "display_name": "Paused"},
            <AllowedValueInfo>{"value": "disabled", "display_name": "Disabled"},
        );

        #! The statuses a monitor may be set to
        const MonitorSetStatusValues = (
            <AllowedValueInfo>{"value": "active", "display_name": "Active"},
            <AllowedValueInfo>{"value": "paused", "display_name": "Paused"},
        );

        #! What a bulk monitor action does, named rather than shown as raw tokens
        const MonitorActionValues = (
            <AllowedValueInfo>{"value": "pause",   "display_name": "Pause"},
            <AllowedValueInfo>{"value": "unpause", "display_name": "Resume"},
            <AllowedValueInfo>{"value": "delete",  "display_name": "Delete permanently"},
        );

        #! Dropdowns backed by the application's own listing actions
        /** Every collection in this API answers a <tt>{data, hasMore, nextCursor}</tt> envelope, so
            every entry follows \c "data" and caps the page rather than walking a collection to fill a
            control.
        */
        const ReferenceData = {
            RefAgentRun: <RestSchemaReferenceDataInfo>{
                "name": RefAgentRun,
                "action": "list-agent-runs",
                "options": {"limit": 100},
                "records_path": "data",
                "value_field": "id",
                "desc": "research agent runs for this team",
            },
            RefBatch: <RestSchemaReferenceDataInfo>{
                "name": RefBatch,
                "action": "list-batches",
                "options": {"limit": 100},
                "records_path": "data",
                "value_field": "id",
                "desc": "batches for this team",
            },
            RefMonitor: <RestSchemaReferenceDataInfo>{
                "name": RefMonitor,
                "action": "list-monitors",
                "options": {"limit": 100},
                "records_path": "data",
                "value_field": "id",
                "display_field": "name",
                "desc": "search monitors for this team",
            },
            # a run only means anything inside its monitor, so this listing is scoped by the monitor the
            # user has already chosen; the start time reads as "which cycle" where a run ID does not
            RefMonitorRun: <RestSchemaReferenceDataInfo>{
                "name": RefMonitorRun,
                "action": "list-monitor-runs",
                "options": {"limit": 100},
                "options_from": {"id": "id"},
                "records_path": "data",
                "value_field": "id",
                "display_field": "startedAt",
                "desc": "cycles the selected search monitor has run",
            },
        };

        #! The operations this application exports
        const Manifest = (
            # --------------------------------------------------------------- core search and answers
            <RestSchemaActionInfo>{
                "action": "search",
                "method": "POST",
                "path": "/search",
                "display_name": "Search",
                "short_desc": "Search the web and optionally return page contents",
                "desc": "Searches the web with an Exa prompt-engineered query and returns the results, "
                    "optionally with the contents of each page.\n\n"
                    "`type` decides how hard Exa works on the query, from `instant` through to "
                    "`deep-reasoning`, and is what trades latency and cost against quality.  `category` "
                    "narrows the search to one kind of page - companies, news, people and so on - which "
                    "is usually a better filter than adding words to the query.\n\n"
                    "Ask for `contents` to get the page text back with the results rather than "
                    "fetching it separately with **Get Contents**.",
                "groups": (GroupSearch,),
                "field_order": ("query", "type", "category", "numResults", "contents"),
                "fields": {
                    "query": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Query",
                        "short_desc": "What to search for, in natural language",
                        "preselected": True,
                    },
                    "type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search Depth",
                        "short_desc": "How hard Exa works on the query",
                        "desc": "How hard Exa works on the query, trading latency and cost against "
                            "quality.  `auto` lets Exa choose.",
                        "allowed_values": SearchTypeValues,
                        "preselected": True,
                    },
                    "category": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Category",
                        "short_desc": "Narrow the search to one kind of page",
                        "desc": "Narrow the search to one kind of page.  Usually a better filter than "
                            "adding words to the query.",
                        "allowed_values": CategoryValues,
                        "preselected": True,
                    },
                    "compliance": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Compliance Mode",
                        "short_desc": "Run the search under an enterprise compliance mode",
                        "allowed_values": ComplianceValues,
                    },
                    "numResults": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Result Count",
                        "short_desc": "How many results to return",
                        "preselected": True,
                    },
                    "contents": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Contents",
                        "short_desc": "Return the contents of each result as well as the link",
                        "desc": "What to return for each result besides its link: the page `text`, the "
                            "passages that match a query as `highlights`, or a written `summary`.\n\n"
                            "Asking here costs one request; fetching the same pages afterwards with "
                            "**Get Contents** costs two.",
                        "preselected": True,
                    },
                    "includeDomains": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Only These Domains",
                        "short_desc": "Return results only from these domains",
                    },
                    "excludeDomains": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Exclude Domains",
                        "short_desc": "Never return results from these domains",
                    },
                    "startPublishedDate": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Published Since",
                        "short_desc": "Return only pages published on or after this date",
                    },
                    "endPublishedDate": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Published Before",
                        "short_desc": "Return only pages published on or before this date",
                    },
                    "startCrawlDate": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Crawled Since",
                        "short_desc": "Return only pages Exa crawled on or after this date",
                    },
                    "endCrawlDate": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Crawled Before",
                        "short_desc": "Return only pages Exa crawled on or before this date",
                    },
                    "userLocation": <RestSchemaFieldOverlayInfo>{
                        "display_name": "User Location",
                        "short_desc": "Two-letter country code the search is made from",
                    },
                    "systemPrompt": <RestSchemaFieldOverlayInfo>{
                        "display_name": "System Prompt",
                        "short_desc": "Extra instructions that guide the search",
                    },
                    "outputSchema": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Output Schema",
                        "short_desc": "A JSON Schema describing the answer you want back",
                    },
                    "additionalQueries": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Additional Queries",
                        "short_desc": "Further queries run alongside the main one",
                    },
                    "moderation": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Moderation",
                        "short_desc": "Filter out results Exa considers unsafe",
                    },
                    "stream": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Stream",
                        "short_desc": "Return the response as a server-sent event stream",
                        "desc": "Return the response as a server-sent event stream.  Leave this off "
                            "unless the caller consumes SSE: an action returns the whole response.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "answer",
                "method": "POST",
                "path": "/answer",
                "display_name": "Answer a Question",
                "short_desc": "Answer a question from the web, with citations",
                "desc": "Searches the web and writes an answer to the question, citing the pages it "
                    "used.\n\n"
                    "Use this where **Search** would leave the caller to read the results: it returns "
                    "the answer itself, with the sources, rather than a list of links.  `model` trades "
                    "speed against depth, from `exa-fast` to `exa-research`.\n\n"
                    "Set `outputSchema` to get the answer back as structured JSON rather than prose.",
                "groups": (GroupSearch,),
                "field_order": ("query", "model", "text", "outputSchema"),
                "fields": {
                    "query": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Question",
                        "short_desc": "The question to answer, in natural language",
                        "preselected": True,
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "Which model writes the answer",
                        "desc": "Which model writes the answer, trading speed against depth.",
                        "preselected": True,
                        "allowed_values": AnswerModelValues,
                    },
                    "text": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include Page Text",
                        "short_desc": "Return the full text of each cited page",
                        "preselected": True,
                    },
                    "outputSchema": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Output Schema",
                        "short_desc": "A JSON Schema describing the answer you want back",
                        "desc": "A [JSON Schema Draft 7](https://json-schema.org/draft-07) description "
                            "of the answer you want back, which makes the result structured data "
                            "rather than prose.",
                    },
                    "systemPrompt": <RestSchemaFieldOverlayInfo>{
                        "display_name": "System Prompt",
                        "short_desc": "Extra instructions that guide the answer",
                    },
                    "userLocation": <RestSchemaFieldOverlayInfo>{
                        "display_name": "User Location",
                        "short_desc": "Two-letter country code the question is asked from",
                    },
                    "stream": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Stream",
                        "short_desc": "Return the response as a server-sent event stream",
                        "desc": "Return the response as a server-sent event stream.  Leave this off "
                            "unless the caller consumes SSE: an action returns the whole response.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "contents",
                "method": "POST",
                "path": "/contents",
                "display_name": "Get Contents",
                "short_desc": "Fetch the text, highlights or a summary of pages",
                "desc": "Fetches the contents of pages Exa already knows about: their text, the "
                    "passages that match a query, or a written summary.\n\n"
                    "Give the pages as `urls`, or as the `ids` a search returned - one or the other, "
                    "not both.\n\n"
                    "`livecrawl` decides whether Exa serves the page from its cache or fetches it now, "
                    "which is the trade between speed and freshness.",
                "groups": (GroupSearch,),
                "field_order": ("urls", "ids", "text", "highlights", "summary", "livecrawl"),
                "fields": {
                    "urls": <RestSchemaFieldOverlayInfo>{
                        "display_name": "URLs",
                        "short_desc": "The pages to fetch",
                        "desc": "The pages to fetch.  Supply these or `ids`, not both.",
                        "exclusive_with": ("ids",),
                        "required_groups": ("pages",),
                        "preselected": True,
                    },
                    "ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Document IDs",
                        "short_desc": "Document IDs returned by a search",
                        "desc": "Document IDs returned by a search.  Supply these or `urls`, not both.",
                        "exclusive_with": ("urls",),
                        "required_groups": ("pages",),
                        "preselected": True,
                    },
                    "text": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Text",
                        "short_desc": "Return the page text",
                        "preselected": True,
                    },
                    "highlights": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Highlights",
                        "short_desc": "Return the passages that match a query",
                        "preselected": True,
                    },
                    "summary": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Summary",
                        "short_desc": "Return a written summary of each page",
                        "preselected": True,
                    },
                    "livecrawl": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Live Crawl",
                        "short_desc": "Whether to fetch the page now or serve it from cache",
                        "desc": "Whether to fetch the page now or serve it from Exa's cache - the "
                            "trade between speed and freshness.",
                        "allowed_values": LivecrawlValues,
                        "preselected": True,
                    },
                    "livecrawlTimeout": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Live Crawl Timeout",
                        "short_desc": "How long to wait for a live crawl, in milliseconds",
                    },
                    "maxAgeHours": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Maximum Age",
                        "short_desc": "Re-crawl a cached page older than this many hours",
                    },
                    "compliance": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Compliance Mode",
                        "short_desc": "Fetch under an enterprise compliance mode",
                        "allowed_values": ComplianceValues,
                    },
                    "subpages": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Subpages",
                        "short_desc": "How many linked subpages to fetch as well",
                    },
                    "subpageTarget": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Subpage Target",
                        "short_desc": "Which subpages to fetch, by keyword",
                    },
                    "extras": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Extras",
                        "short_desc": "Extra content to return, such as links and images",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "find-similar",
                "method": "POST",
                "path": "/findSimilar",
                "display_name": "Find Similar Links",
                "short_desc": "Find pages similar to a URL",
                "desc": "Finds pages similar to the one given, optionally with their contents.\n\n"
                    "**Exa marks this endpoint deprecated** and recommends **Search** with a query "
                    "describing the page instead, which gives more control over what \"similar\" "
                    "means.  The action is kept because flows already call it.",
                "groups": (GroupSearch,),
                "field_order": ("url", "category", "numResults", "excludeSourceDomain", "contents"),
                "fields": {
                    "url": <RestSchemaFieldOverlayInfo>{
                        "display_name": "URL",
                        "short_desc": "The page to find others like",
                        "preselected": True,
                    },
                    "category": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Category",
                        "short_desc": "Narrow the results to one kind of page",
                        "allowed_values": CategoryValues,
                        "preselected": True,
                    },
                    "numResults": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Result Count",
                        "short_desc": "How many results to return",
                        "preselected": True,
                    },
                    "excludeSourceDomain": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Exclude the Source Domain",
                        "short_desc": "Leave out other pages from the same site",
                        "preselected": True,
                    },
                    "contents": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Contents",
                        "short_desc": "Return the contents of each result as well as the link",
                        "desc": "What to return for each result besides its link: the page `text`, the "
                            "passages that match a query as `highlights`, or a written `summary`.\n\n"
                            "Asking here costs one request; fetching the same pages afterwards with "
                            "**Get Contents** costs two.",
                    },
                    "includeDomains": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Only These Domains",
                        "short_desc": "Return results only from these domains",
                    },
                    "excludeDomains": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Exclude Domains",
                        "short_desc": "Never return results from these domains",
                    },
                    "startPublishedDate": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Published Since",
                        "short_desc": "Return only pages published on or after this date",
                    },
                    "endPublishedDate": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Published Before",
                        "short_desc": "Return only pages published on or before this date",
                    },
                    "startCrawlDate": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Crawled Since",
                        "short_desc": "Return only pages Exa crawled on or after this date",
                    },
                    "endCrawlDate": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Crawled Before",
                        "short_desc": "Return only pages Exa crawled on or before this date",
                    },
                },
            },

            # ------------------------------------------------------------------------ research agent
            <RestSchemaActionInfo>{
                "action": "create-agent-run",
                "method": "POST",
                "path": "/agent/runs",
                "display_name": "Start a Research Run",
                "short_desc": "Ask the research agent a question and get the run it started",
                "desc": "Starts a research run: the agent searches, reads and reasons over the web "
                    "until it can answer, rather than returning a single search's results.\n\n"
                    "Use this where **Answer a Question** is not enough - a question that needs several "
                    "searches, or one whose answer has to be assembled from many pages.  `effort` is "
                    "the budget: `minimal` is quick and shallow, `max` keeps working.\n\n"
                    "The run is asynchronous; read it with **Get a Research Run**.  Set `outputSchema` "
                    "to get structured data back rather than prose.",
                "groups": (GroupAgent,),
                "field_order": ("query", "effort", "outputSchema", "systemPrompt"),
                "fields": {
                    "query": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Question",
                        "short_desc": "What to research, in natural language",
                        "preselected": True,
                    },
                    "effort": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Effort",
                        "short_desc": "How much work the agent puts in, and what it costs",
                        "preselected": True,
                        "allowed_values": EffortValues,
                    },
                    "outputSchema": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Output Schema",
                        "short_desc": "A JSON Schema describing the answer you want back",
                        "preselected": True,
                    },
                    "systemPrompt": <RestSchemaFieldOverlayInfo>{
                        "display_name": "System Prompt",
                        "short_desc": "Extra instructions that guide the agent",
                    },
                    "previousRunId": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Continue From",
                        "short_desc": "A completed run to carry on from",
                        "ref_data": RefAgentRun,
                    },
                    "input": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Input",
                        "short_desc": "Records to process, and records to leave out of the answer",
                    },
                    "dataSources": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Data Sources",
                        "short_desc": "Exa Connect providers the run may use",
                    },
                    "budget": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Budget",
                        "short_desc": "A ceiling on what the run may spend",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-agent-runs",
                "method": "GET",
                "path": "/agent/runs",
                "display_name": "List Research Runs",
                "short_desc": "List the research runs for this team",
                "desc": "Lists research runs, newest first.\n\nResults are paged: read `hasMore` and "
                    "pass `nextCursor` as `cursor` to get the following page.",
                "groups": (GroupAgent,),
                "fields": PagingFields,
            },
            <RestSchemaActionInfo>{
                "action": "get-agent-run",
                "method": "GET",
                "path": "/agent/runs/{id}",
                "display_name": "Get a Research Run",
                "short_desc": "Get a research run and its answer",
                "desc": "Returns a research run, including its answer once it has finished.",
                "groups": (GroupAgent,),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Run",
                        "short_desc": "The run to read",
                        "ref_data": RefAgentRun,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-agent-run-events",
                "method": "GET",
                "path": "/agent/runs/{id}/events",
                "display_name": "List Research Run Events",
                "short_desc": "See what the agent did during a run",
                "desc": "Lists the stored events of a research run - the searches it made and the "
                    "pages it read.  Use it to show how an answer was reached, or to see where a long "
                    "run is spending its time.",
                "groups": (GroupAgent,),
                "fields": PagingFields + {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Run",
                        "short_desc": "The run whose events to list",
                        "ref_data": RefAgentRun,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "cancel-agent-run",
                "method": "POST",
                "path": "/agent/runs/{id}/cancel",
                "display_name": "Cancel a Research Run",
                "short_desc": "Stop a research run and discard its results",
                "desc": "Cancels a queued or running research run without returning results.  Usage "
                    "accrued before the cancellation is still billed.\n\n"
                    "Use **Stop a Research Run** instead to keep what the agent has found so far.",
                "groups": (GroupAgent,),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Run",
                        "short_desc": "The run to cancel",
                        "ref_data": RefAgentRun,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "stop-agent-run",
                "method": "POST",
                "path": "/agent/runs/{id}/stop",
                "display_name": "Stop a Research Run",
                "short_desc": "End a research run early but keep its results",
                "desc": "Ends a running research run early and returns what the agent has gathered so "
                    "far - the difference from **Cancel a Research Run**, which discards it.\n\n"
                    "Exa supports this only for `max` effort runs.",
                "groups": (GroupAgent,),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Run",
                        "short_desc": "The run to stop",
                        "ref_data": RefAgentRun,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-agent-run",
                "method": "DELETE",
                "path": "/agent/runs/{id}",
                "display_name": "Delete a Research Run",
                "short_desc": "Delete a stored research run",
                "desc": "Deletes a stored research run and its answer.",
                "groups": (GroupAgent,),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Run",
                        "short_desc": "The run to delete",
                        "ref_data": RefAgentRun,
                        "preselected": True,
                    },
                },
            },

            # ------------------------------------------------------------------------------- batches
            <RestSchemaActionInfo>{
                "action": "create-batch",
                "method": "POST",
                "path": "/batches",
                "display_name": "Create a Batch",
                "short_desc": "Run many requests asynchronously in one job",
                "desc": "Enqueues many requests to run asynchronously, each dispatched to its own "
                    "route, with the results made available for download when the batch finishes.\n\n"
                    "This is how a large workload is run without one request per item.  Each entry "
                    "needs a `customId` unique within the batch, which is how its result is identified "
                    "afterwards.",
                "groups": (GroupBatches,),
                "fields": {
                    "requests": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Requests",
                        "short_desc": "The requests to enqueue, each with a unique customId",
                        "preselected": True,
                    },
                    "metadata": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Metadata",
                        "short_desc": "Key-value pairs stored with the batch",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-batches",
                "method": "GET",
                "path": "/batches",
                "display_name": "List Batches",
                "short_desc": "List the batches for this team",
                "desc": "Lists batches, newest first, optionally narrowed to one status.",
                "groups": (GroupBatches,),
                "fields": PagingFields + {
                    "status": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Status",
                        "short_desc": "Return only batches in this status",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-batch",
                "method": "GET",
                "path": "/batches/{id}",
                "display_name": "Get a Batch",
                "short_desc": "Get a batch and where to download its results",
                "desc": "Returns a batch, including how many of its requests have finished and the "
                    "`resultsUrl` to download them from once it is complete.",
                "groups": (GroupBatches,),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Batch",
                        "short_desc": "The batch to read",
                        "ref_data": RefBatch,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "cancel-batch",
                "method": "POST",
                "path": "/batches/{id}/cancel",
                "display_name": "Cancel a Batch",
                "short_desc": "Stop a batch scheduling further requests",
                "desc": "Asks a batch to stop.  It schedules no further requests, lets the work already "
                    "in flight finish, and then settles as cancelled; requests that already completed "
                    "keep their results.\n\n"
                    "A batch that has already completed or expired cannot be cancelled.",
                "groups": (GroupBatches,),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Batch",
                        "short_desc": "The batch to cancel",
                        "ref_data": RefBatch,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-batch",
                "method": "DELETE",
                "path": "/batches/{id}",
                "display_name": "Delete a Batch",
                "short_desc": "Delete a finished batch",
                "desc": "Deletes a batch that has reached a terminal status.",
                "groups": (GroupBatches,),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Batch",
                        "short_desc": "The batch to delete",
                        "ref_data": RefBatch,
                        "preselected": True,
                    },
                },
            },

            # ------------------------------------------------------------------------ search monitors
            <RestSchemaActionInfo>{
                "action": "create-monitor",
                "method": "POST",
                "path": "/monitors",
                "display_name": "Create a Search Monitor",
                "short_desc": "Run a search on a schedule and deliver new results to a webhook",
                "desc": "Creates a monitor: a search Exa re-runs on a schedule, delivering what is new "
                    "to your webhook.\n\n"
                    "Exa only sends what you have not already seen - it filters by date since the last "
                    "run and removes near-duplicates - so this is how a query becomes a feed rather "
                    "than something a caller has to poll and de-duplicate itself.",
                "groups": (GroupMonitors,),
                "field_order": ("name", "search", "trigger", "webhook"),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "A name for the monitor",
                        "preselected": True,
                    },
                    "search": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search",
                        "short_desc": "The search to run on each cycle",
                        "desc": "The search the monitor re-runs on each cycle: the same `query`, `type` "
                            "and `category` that **Search** takes.\n\n"
                            "Exa filters each cycle by date since the last run and removes near-"
                            "duplicates, so this is the query whose **new** results are delivered, not "
                            "the query whose results are re-sent every time.",
                        "preselected": True,
                    },
                    "trigger": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Schedule",
                        "short_desc": "How often the search runs",
                        "desc": "How often the search runs, and from when.  A cycle that finds nothing "
                            "new delivers nothing, so the schedule sets how quickly the feed reacts "
                            "rather than how much it sends.",
                        "preselected": True,
                    },
                    "webhook": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Webhook",
                        "short_desc": "Where new results are delivered",
                        "desc": "Where each cycle's new results are delivered.  This is the only way "
                            "results leave a monitor - there is no endpoint that returns them - so a "
                            "monitor without a reachable webhook produces nothing a caller can read.\n\n"
                            "`metadata` is echoed back in every delivery, which is how a receiver routes "
                            "an update without looking the monitor up.",
                        "preselected": True,
                    },
                    "outputSchema": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Output Schema",
                        "short_desc": "A JSON Schema describing the results you want delivered",
                    },
                    "metadata": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Metadata",
                        "short_desc": "Key-value pairs echoed back in each delivery",
                        "desc": "Key-value pairs echoed back in each webhook delivery, so a receiver "
                            "can route updates without looking the monitor up.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-monitors",
                "method": "GET",
                "path": "/monitors",
                "display_name": "List Search Monitors",
                "short_desc": "List the search monitors for this team",
                "desc": "Lists search monitors, optionally narrowed by status, name or metadata.",
                "groups": (GroupMonitors,),
                "fields": PagingFields + {
                    "status": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Status",
                        "short_desc": "Return only monitors in this status",
                        "preselected": True,
                        "allowed_values": MonitorStatusValues,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Return only monitors with this name",
                        "preselected": True,
                    },
                    "metadata": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Metadata",
                        "short_desc": "Return only monitors carrying these key-value pairs",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-monitor",
                "method": "GET",
                "path": "/monitors/{id}",
                "display_name": "Get a Search Monitor",
                "short_desc": "Get one search monitor",
                "desc": "Returns one search monitor, its schedule and its webhook.",
                "groups": (GroupMonitors,),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Monitor",
                        "short_desc": "The monitor to read",
                        "ref_data": RefMonitor,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-monitor",
                "method": "PATCH",
                "path": "/monitors/{id}",
                "display_name": "Update a Search Monitor",
                "short_desc": "Change a monitor's search, schedule or status",
                "desc": "Updates a search monitor.  Use `status` to pause it without deleting it: a "
                    "paused monitor keeps its definition and stops running.  Omit a field to leave it "
                    "unchanged.",
                "groups": (GroupMonitors,),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Monitor",
                        "short_desc": "The monitor to update",
                        "ref_data": RefMonitor,
                        "preselected": True,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Status",
                        "short_desc": "Pause the monitor or start it running again",
                        "preselected": True,
                        "allowed_values": MonitorSetStatusValues,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "A new name for the monitor",
                    },
                    "search": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search",
                        "short_desc": "Search settings to merge into the monitor",
                    },
                    "trigger": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Schedule",
                        "short_desc": "A new schedule; set to null to remove it",
                    },
                    "webhook": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Webhook",
                        "short_desc": "Webhook settings to merge into the monitor",
                    },
                    "outputSchema": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Output Schema",
                        "short_desc": "A JSON Schema describing the results you want delivered",
                    },
                    "metadata": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Metadata",
                        "short_desc": "Metadata to store; set to null to remove it",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-monitor",
                "method": "DELETE",
                "path": "/monitors/{id}",
                "display_name": "Delete a Search Monitor",
                "short_desc": "Delete a search monitor",
                "desc": "Deletes a search monitor.  Results already delivered to the webhook are "
                    "unaffected.",
                "groups": (GroupMonitors,),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Monitor",
                        "short_desc": "The monitor to delete",
                        "ref_data": RefMonitor,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "batch-monitors",
                "method": "POST",
                "path": "/monitors/batch",
                "display_name": "Act on Several Monitors",
                "short_desc": "Pause, resume or delete every monitor matching a filter",
                "desc": "Applies one action - pause, unpause or delete - to every monitor matching a "
                    "filter.\n\n"
                    "A filter is required, so a mistyped request cannot act on the whole account.  Set "
                    "`dry_run` to see which monitors would be affected without changing anything, "
                    "which is worth doing before a delete.",
                "groups": (GroupMonitors,),
                "field_order": ("action", "filter", "dry_run"),
                "fields": {
                    "action": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Action",
                        "short_desc": "What to do with the matching monitors",
                        "preselected": True,
                        "allowed_values": MonitorActionValues,
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "short_desc": "Which monitors to act on; at least one field is required",
                        "preselected": True,
                    },
                    "dry_run": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dry Run",
                        "short_desc": "Report what would be affected without doing it",
                        "preselected": True,
                    },
                    "limit": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Maximum Monitors",
                        "short_desc": "How many monitors this request may act on",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "trigger-monitor",
                "method": "POST",
                "path": "/monitors/{id}/trigger",
                "display_name": "Run a Search Monitor Now",
                "short_desc": "Run a monitor immediately, whatever its schedule says",
                "desc": "Runs a search monitor immediately rather than waiting for its next scheduled "
                    "cycle.  New results are delivered to its webhook as usual.",
                "groups": (GroupMonitors,),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Monitor",
                        "short_desc": "The monitor to run",
                        "ref_data": RefMonitor,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-monitor-runs",
                "method": "GET",
                "path": "/monitors/{id}/runs",
                "display_name": "List Search Monitor Runs",
                "short_desc": "List the cycles a search monitor has run",
                "desc": "Lists the cycles a search monitor has run, newest first, with what each one "
                    "found.",
                "groups": (GroupMonitors,),
                "fields": PagingFields + {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Monitor",
                        "short_desc": "The monitor whose runs to list",
                        "ref_data": RefMonitor,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-monitor-run",
                "method": "GET",
                "path": "/monitors/{id}/runs/{runId}",
                "display_name": "Get a Search Monitor Run",
                "short_desc": "Get one cycle of a search monitor",
                "desc": "Returns one cycle of a search monitor, including what it found and why it "
                    "failed if it did.",
                "groups": (GroupMonitors,),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Monitor",
                        "short_desc": "The monitor the run belongs to",
                        "ref_data": RefMonitor,
                        "has_dependents": True,
                        "preselected": True,
                    },
                    "runId": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Run",
                        "short_desc": "The run to read",
                        "ref_data": RefMonitorRun,
                        "depends_on": ("id",),
                        "preselected": True,
                    },
                },
            },

        );
    }
}
}
