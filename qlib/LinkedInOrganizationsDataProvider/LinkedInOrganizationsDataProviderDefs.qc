# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinkedInOrganizationsDataProvider module definition

/** LinkedInOrganizationsDataProviderDefs.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the LinkedInOrganizationsDataProvider module
public namespace LinkedInOrganizationsDataProvider {
#! LinkedIn Organizations app name
public const AppName = "LinkedInOrganizations";

#! Logo file
/** Read as a separate file rather than inlined so that the doc build and the app catalog share one
    source of truth for the image
*/
const LinkedInOrganizationsLogo = File::readTextFile(get_script_dir() + "/linkedin-logo.svg");

#! The path of the Organization Access Control API
public const OrganizationAclsPath = LinkedInRestClient::LinkedInRestPrefix + "/dmaOrganizationAcls";

#! The path of the Organizations API
public const OrganizationsPath = LinkedInRestClient::LinkedInRestPrefix + "/dmaOrganizations";

#! The path of the Feed Content Finder API
public const FeedContentsPath = LinkedInRestClient::LinkedInRestPrefix + "/dmaFeedContentsExternal";

#! The path of the DMA Posts API
public const PostsPath = LinkedInRestClient::LinkedInRestPrefix + "/dmaPosts";

#! The path of the DMA Comments API
public const CommentsPath = LinkedInRestClient::LinkedInRestPrefix + "/dmaComments";

#! The path of the DMA Reactions API
public const ReactionsPath = LinkedInRestClient::LinkedInRestPrefix + "/dmaReactions";

#! The path of the DMA Social Metadata API
public const SocialMetadataPath = LinkedInRestClient::LinkedInRestPrefix + "/dmaSocialMetadata";

#! The path of the Organizational Page Follows API
public const PageFollowsPath = LinkedInRestClient::LinkedInRestPrefix
    + "/dmaOrganizationalPageFollows";

#! The path of the Organizational Page Edge Analytics API
public const EdgeAnalyticsPath = LinkedInRestClient::LinkedInRestPrefix
    + "/dmaOrganizationalPageEdgeAnalytics";

#! The path of the DMA member identity endpoint
public const MePath = LinkedInRestClient::LinkedInRestPrefix + "/dmaMe";

#! The maximum \c maxPaginationCount LinkedIn accepts on any cursor-paged DMA finder
public const MaxPaginationCount = 1000;

#! The default number of items a listing action requests
public const DefaultPageSize = 25;

#! The maximum number of entries a Rest.li \c BATCH_GET is given in one request
/** A URL has a finite length and a composite comment or reaction URN is long, so a listing that
    resolves many URNs sends several batches rather than one enormous one.
*/
public const MaxBatchSize = 50;

#! The shortest poll interval the \c new-post trigger allows, in seconds
/** LinkedIn rate-limits the DMA feed finders to **one request per 60 seconds** and serves data that
    can be up to 48 hours old, so polling faster than this cannot see anything new and earns HTTP
    429s.
*/
public const MinPollInterval = 60;

#! The default poll interval for the \c new-post trigger, in seconds
/** Well above @ref MinPollInterval because each tick costs two rate-limited requests and the data
    is up to 48 hours delayed anyway
*/
public const DefaultPollInterval = 900;

#! The name of the event raised when an organization publishes a post
public const EVENT_NEW_POST = "new-post";

#! The dimensions page analytics can be broken down by
public const AnalyticsDimensionValues = (
    <AllowedValueInfo>{
        "value": "JOB_FUNCTION",
        "display_name": "Job Function",
        "short_desc": "Break down by the member's job function",
        "desc": "Break down by the member's job function",
    },
    <AllowedValueInfo>{
        "value": "INDUSTRY",
        "display_name": "Industry",
        "short_desc": "Break down by the member's industry",
        "desc": "Break down by the member's industry",
    },
    <AllowedValueInfo>{
        "value": "SENIORITY",
        "display_name": "Seniority",
        "short_desc": "Break down by the member's seniority",
        "desc": "Break down by the member's seniority",
    },
    <AllowedValueInfo>{
        "value": "COUNTRY_GEO",
        "display_name": "Country",
        "short_desc": "Break down by the member's country",
        "desc": "Break down by the member's country",
    },
    <AllowedValueInfo>{
        "value": "REGION_GEO",
        "display_name": "Region",
        "short_desc": "Break down by the member's region",
        "desc": "Break down by the member's region",
    },
    <AllowedValueInfo>{
        "value": "STAFF_COUNT_RANGE",
        "display_name": "Company Size",
        "short_desc": "Break down by the size of the member's company",
        "desc": "Break down by the size of the member's company",
    },
    <AllowedValueInfo>{
        "value": "RELATIONSHIP",
        "display_name": "Relationship",
        "short_desc": "Break down by the member's relationship to the organization",
        "desc": "Break down by the member's relationship to the organization, which distinguishes "
            "employees from everyone else",
    },
);

#! The page sections visitor analytics can be attributed to
public const VisitorSourceValues = (
    <AllowedValueInfo>{"value": "OVERVIEW", "display_name": "Overview",
        "short_desc": "The page's overview tab", "desc": "The page's overview tab"},
    <AllowedValueInfo>{"value": "ABOUT", "display_name": "About",
        "short_desc": "The page's about tab", "desc": "The page's about tab"},
    <AllowedValueInfo>{"value": "JOBS", "display_name": "Jobs",
        "short_desc": "The page's jobs tab", "desc": "The page's jobs tab"},
    <AllowedValueInfo>{"value": "LIFE", "display_name": "Life",
        "short_desc": "The page's life tab", "desc": "The page's life tab"},
    <AllowedValueInfo>{"value": "CAREER", "display_name": "Careers",
        "short_desc": "The page's careers tab", "desc": "The page's careers tab"},
    <AllowedValueInfo>{"value": "PRODUCTS", "display_name": "Products",
        "short_desc": "The page's products tab", "desc": "The page's products tab"},
    <AllowedValueInfo>{"value": "INSIGHTS", "display_name": "Insights",
        "short_desc": "The page's insights tab", "desc": "The page's insights tab"},
    <AllowedValueInfo>{"value": "PEOPLE", "display_name": "People",
        "short_desc": "The page's people tab", "desc": "The page's people tab"},
);

#! The view contexts a post can be read in
public const ViewContextValues = (
    <AllowedValueInfo>{
        "value": "READER",
        "display_name": "Reader",
        "short_desc": "The published version visible to a general audience",
        "desc": "The published version visible to a general audience",
    },
    <AllowedValueInfo>{
        "value": "AUTHOR",
        "display_name": "Author",
        "short_desc": "The latest version, including drafts and scheduled posts",
        "desc": "The latest version, which may be a draft or a scheduled post; a scheduled post is "
            "identified by its `scheduled_at` field being set",
    },
);
}
