# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinkedInOrganizationsDataProvider module definition

/** LinkedInOrganizationsProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The \c organization request field, shared by every action scoped to one page
public const LinkedInOrganizationField = {
    "organization": {
        "display_name": "Organization",
        "short_desc": "The organization whose data is read",
        "desc": "The organization whose data is read.  The list offers the organizations the "
            "authenticated member administers",
        "type": AbstractDataProviderTypeMap."string",
        "required": True,
        "ref_data": "organizations",
        "example_value": "2414183",
    },
};

#! The cursor-paging request fields, shared by every listing action
public const LinkedInPagingFields = {
    "count": {
        "display_name": "Count",
        "short_desc": "The maximum number of items to return",
        "desc": "The maximum number of items to return in one page (LinkedIn's maximum is `"
            + MaxPaginationCount + "`).  Fewer items may come back even when more are available, so "
            "the presence of a `cursor` in the response — not a short page — is what says the "
            "listing continues",
        "type": AbstractDataProviderTypeMap."*softint",
        "default_value": DefaultPageSize,
    },
    "cursor": {
        "display_name": "Cursor",
        "short_desc": "The cursor from a previous call, to continue the listing",
        "desc": "The `cursor` returned by a previous call, to retrieve the next page.  Leave it "
            "unset to start at the beginning",
        "type": AbstractDataProviderTypeMap."*string",
    },
};

#! The \c entity request field, shared by the engagement actions
public const LinkedInEntityField = {
    "entity": {
        "display_name": "Content URN",
        "short_desc": "The URN of the post, comment or activity",
        "desc": "The URN of the content to read engagement for: a post (`urn:li:share:...`, "
            "`urn:li:ugcPost:...`), an activity (`urn:li:activity:...`) or a comment "
            "(`urn:li:comment:(...)`).  The **List Posts** action returns post URNs in this form",
        "type": AbstractDataProviderTypeMap."string",
        "required": True,
        "example_value": "urn:li:share:1188488118",
    },
};

#! Common base for the LinkedIn Organizations action providers
public class LinkedInOrgActionProviderBase inherits LinkedInOrganizationsDataProviderBase {
    constructor(LinkedInRestClient::LinkedInRestClientIo rest)
            : LinkedInOrganizationsDataProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInOrganizationsDataProviderBase(options) {}

    #! Returns the organization URN for an organization ID or URN
    /** @param organization the numeric organization ID, or a full organization URN

        @return the organization URN
    */
    static string organizationUrn(string organization) {
        return organization =~ /^urn:li:/ ? organization : "urn:li:organization:" + organization;
    }

    #! Returns the organizational-page URN for an organization
    /** The analytics and follows APIs address an **organizational page**, while the feed API
        addresses an **organization**; the two are different URN types.  The page URN is read from
        the organization's own profile rather than assembled from its ID, because nothing in
        LinkedIn's contract promises the two share a number.

        @param organization the numeric organization ID, or an organizational-page URN to use as-is

        @return the organizational-page URN

        @throw LINKEDIN-ORGANIZATION-ERROR the organization has no organizational page, or is not
        visible to the authenticated member
    */
    private string organizationalPageUrn(string organization) {
        if (organization =~ /^urn:li:organizationalPage:/) {
            return organization;
        }
        string id = organization =~ /^urn:li:/
            ? LinkedInOrganizationsUtil::urnId(organization)
            : organization;
        list<auto> orgs = resolveOrganizations((id,));
        if (!orgs) {
            throw "LINKEDIN-ORGANIZATION-ERROR", sprintf("organization %y was not returned by "
                "LinkedIn; the authenticated member may not administer it", organization);
        }
        *string page = orgs[0].organizational_page;
        if (!page.val()) {
            throw "LINKEDIN-ORGANIZATION-ERROR", sprintf("organization %y has no organizational "
                "page, so page analytics and follows cannot be read for it", organization);
        }
        return page;
    }

    #! Runs a feed finder and resolves the URNs it names through a BATCH_GET
    /** This two-step shape is how every DMA content listing works: the feed finder returns only
        URNs, in creation order, with the cursor that resumes it, and the entity resource resolves
        those URNs to objects.

        @param query the feed finder's query parameters
        @param count the page size requested
        @param cursor the cursor from a previous call, if any
        @param resolve_path the resource path to resolve the URNs against
        @param resolve_query any additional query parameters for the resolve step

        @return a hash of \c urns (in finder order), \c results (keyed by URN) and \c cursor
    */
    private hash<auto> findAndResolve(hash<auto> query, int count, *string cursor,
            string resolve_path, *hash<auto> resolve_query) {
        hash<auto> page = doCursorFinder(FeedContentsPath, query, count, cursor);
        list<auto> urns = ();
        foreach hash<auto> element in (page.elements) {
            if (element.id.typeCode() == NT_STRING && element.id.val()) {
                push urns, element.id;
            }
        }
        return {
            "urns": urns,
            "results": urns ? batchGet(resolve_path, urns, resolve_query) : {},
            "cursor": page.cursor,
        };
    }
}

#! Data provider for retrieving the authenticated member
public class LinkedInGetCurrentMemberDataProvider inherits LinkedInOrgActionProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-current-member",
            "type": "LinkedInGetCurrentMemberDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = NOTHING;
        const ResponseType = LinkedInDmaMemberDataType;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest) : LinkedInOrgActionProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInOrgActionProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> resp = doDmaRequest("GET", MePath);
        hash<auto> body = resp.body.typeCode() == NT_HASH ? resp.body : {};
        return {
            "urn": body.id.val() ? "urn:li:person:" + body.id : NOTHING,
            "id": body.id,
            "first_name": body.localizedFirstName ?? body.firstName.localized.firstValue(),
            "last_name": body.localizedLastName ?? body.lastName.localized.firstValue(),
            "headline": body.localizedHeadline ?? body.headline.localized.firstValue(),
            "raw": body,
        };
    }
}

#! Data provider for listing the organizations the authenticated member administers
public class LinkedInListOrganizationsDataProvider inherits LinkedInOrgActionProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-organizations",
            "type": "LinkedInListOrganizationsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = NOTHING;
        const ResponseType = LinkedInOrganizationListDataType;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest) : LinkedInOrgActionProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInOrgActionProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return {
            "organizations": getAdministeredOrganizations(),
        };
    }
}

#! Request type for retrieving one organization
public class LinkedInGetOrganizationRequestDataType inherits HashDataType {
    public {
        const Fields = LinkedInOrganizationField;
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInGetOrganizationRequestDataType = new LinkedInGetOrganizationRequestDataType();

#! Data provider for retrieving one organization's profile
public class LinkedInGetOrganizationDataProvider inherits LinkedInOrgActionProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-organization",
            "type": "LinkedInGetOrganizationDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = LinkedInGetOrganizationRequestDataType;
        const ResponseType = LinkedInOrganizationDataType;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest) : LinkedInOrgActionProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInOrgActionProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.organization =~ /^urn:li:/
            ? LinkedInOrganizationsUtil::urnId(req.organization)
            : req.organization;
        list<auto> orgs = resolveOrganizations((id,));
        if (!orgs) {
            throw "LINKEDIN-ORGANIZATION-ERROR", sprintf("organization %y was not returned by "
                "LinkedIn; the authenticated member may not administer it", req.organization);
        }
        return orgs[0];
    }
}

#! Request type for listing an organization's posts
public class LinkedInListPostsRequestDataType inherits HashDataType {
    public {
        const Fields = LinkedInOrganizationField + LinkedInPagingFields + {
            "view_context": {
                "display_name": "View Context",
                "short_desc": "Whether to read the posts as their author or as a reader",
                "desc": "Whether to read the posts as their author or as a reader.  `AUTHOR` also "
                    "returns drafts and scheduled posts, which carry a `scheduled_at` value",
                "type": AbstractDataProviderTypeMap."*string",
                "default_value": "READER",
                "allowed_values": ViewContextValues,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInListPostsRequestDataType = new LinkedInListPostsRequestDataType();

#! Data provider for listing an organization's posts
public class LinkedInListPostsDataProvider inherits LinkedInOrgActionProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-posts",
            "type": "LinkedInListPostsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = LinkedInListPostsRequestDataType;
        const ResponseType = LinkedInOrgPostListDataType;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest) : LinkedInOrgActionProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInOrgActionProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> found = findAndResolve({
            "q": "postsByAuthor",
            "author": LinkedInOrgActionProviderBase::organizationUrn(req.organization),
        }, (req.count ?? DefaultPageSize).toInt(), req.cursor, PostsPath, {
            "viewContext": req.view_context ?? "READER",
        });

        # the finder's order is the meaningful one (creation time, newest first); the BATCH_GET
        # result is an unordered map, so it is walked in finder order rather than by its own keys
        list<auto> posts = ();
        foreach string urn in (found.urns) {
            if (*hash<auto> post = found.results{urn}) {
                push posts, LinkedInOrganizationsUtil::normalizePost(post);
            }
        }
        return {
            "posts": posts,
            "cursor": found.cursor,
        };
    }
}

#! Request type for retrieving one post
public class LinkedInGetOrgPostRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "post": {
                "display_name": "Post URN",
                "short_desc": "The URN of the post to retrieve",
                "desc": "The URN of the post to retrieve, as returned by the **List Posts** action",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "urn:li:share:1188488118",
            },
            "view_context": {
                "display_name": "View Context",
                "short_desc": "Whether to read the post as its author or as a reader",
                "desc": "Whether to read the post as its author or as a reader",
                "type": AbstractDataProviderTypeMap."*string",
                "default_value": "READER",
                "allowed_values": ViewContextValues,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInGetOrgPostRequestDataType = new LinkedInGetOrgPostRequestDataType();

#! Data provider for retrieving one of an organization's posts
public class LinkedInGetOrgPostDataProvider inherits LinkedInOrgActionProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-post",
            "type": "LinkedInGetOrgPostDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = LinkedInGetOrgPostRequestDataType;
        const ResponseType = LinkedInOrgPostDataType;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest) : LinkedInOrgActionProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInOrgActionProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # the DMA Posts API only supports BATCH_GET, so a single post is a batch of one
        hash<auto> results = batchGet(PostsPath, (req.post,), {
            "viewContext": req.view_context ?? "READER",
        });
        *hash<auto> post = results{req.post};
        if (!post) {
            throw "LINKEDIN-POST-ERROR", sprintf("post %y was not returned by LinkedIn; it must "
                "have been authored by, or be a reshare of a post authored by, an organization the "
                "authenticated member administers", req.post);
        }
        return LinkedInOrganizationsUtil::normalizePost(post);
    }
}

#! Request type for listing comments on a piece of content
public class LinkedInListCommentsRequestDataType inherits HashDataType {
    public {
        const Fields = LinkedInEntityField + LinkedInPagingFields;
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInListCommentsRequestDataType = new LinkedInListCommentsRequestDataType();

#! Data provider for listing the comments on a piece of content
public class LinkedInListCommentsDataProvider inherits LinkedInOrgActionProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-comments",
            "type": "LinkedInListCommentsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = LinkedInListCommentsRequestDataType;
        const ResponseType = LinkedInCommentListDataType;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest) : LinkedInOrgActionProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInOrgActionProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> found = findAndResolve({
            "q": "commentsOnEntity",
            "entity": req.entity,
        }, (req.count ?? DefaultPageSize).toInt(), req.cursor, CommentsPath);

        list<auto> comments = ();
        foreach string urn in (found.urns) {
            if (*hash<auto> comment = found.results{urn}) {
                push comments, LinkedInOrganizationsUtil::normalizeComment(urn, comment);
            }
        }
        return {
            "comments": comments,
            "cursor": found.cursor,
        };
    }
}

#! Request type for listing reactions on a piece of content
public class LinkedInListReactionsRequestDataType inherits HashDataType {
    public {
        const Fields = LinkedInEntityField + LinkedInPagingFields;
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInListReactionsRequestDataType = new LinkedInListReactionsRequestDataType();

#! Data provider for listing the reactions on a piece of content
public class LinkedInListReactionsDataProvider inherits LinkedInOrgActionProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-reactions",
            "type": "LinkedInListReactionsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = LinkedInListReactionsRequestDataType;
        const ResponseType = LinkedInReactionListDataType;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest) : LinkedInOrgActionProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInOrgActionProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> found = findAndResolve({
            "q": "reactionsOnEntity",
            "entity": req.entity,
        }, (req.count ?? DefaultPageSize).toInt(), req.cursor, ReactionsPath);

        list<auto> reactions = ();
        foreach string urn in (found.urns) {
            if (*hash<auto> reaction = found.results{urn}) {
                push reactions, LinkedInOrganizationsUtil::normalizeReaction(urn, reaction);
            }
        }
        return {
            "reactions": reactions,
            "cursor": found.cursor,
        };
    }
}

#! Request type for reading engagement summaries
public class LinkedInGetSocialMetadataRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "entities": {
                "display_name": "Content URNs",
                "short_desc": "The URNs of the content to summarize",
                "desc": "The URNs of the content to summarize: posts, activities or comments.  "
                    "Several can be given at once, which is much cheaper than one call each",
                "type": new ListDataType("LinkedInSocialMetadataEntities", SoftStringType),
                "required": True,
                "example_value": ("urn:li:share:1188488118",),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInGetSocialMetadataRequestDataType =
    new LinkedInGetSocialMetadataRequestDataType();

#! Data provider for reading the engagement summary of one or more pieces of content
public class LinkedInGetSocialMetadataDataProvider inherits LinkedInOrgActionProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-social-metadata",
            "type": "LinkedInGetSocialMetadataDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = LinkedInGetSocialMetadataRequestDataType;
        const ResponseType = LinkedInSocialMetadataListDataType;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest) : LinkedInOrgActionProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInOrgActionProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        list<auto> urns = req.entities;
        if (!urns) {
            throw "LINKEDIN-REQUEST-ERROR", "at least one content URN must be supplied";
        }
        hash<auto> results = batchGet(SocialMetadataPath, urns);
        list<auto> entities = ();
        foreach string urn in (urns) {
            if (*hash<auto> meta = results{urn}) {
                push entities, LinkedInOrganizationsUtil::normalizeSocialMetadata(urn, meta);
            }
        }
        return {
            "entities": entities,
        };
    }
}

#! Request type for listing an organization's followers
public class LinkedInListFollowersRequestDataType inherits HashDataType {
    public {
        const Fields = LinkedInOrganizationField + {
            "edge_type": {
                "display_name": "Follower Type",
                "short_desc": "Whether to list members or other pages",
                "desc": "Whether to list members or other organizational pages as followers",
                "type": AbstractDataProviderTypeMap."*string",
                "default_value": "MEMBER_FOLLOWS_ORGANIZATIONAL_PAGE",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "MEMBER_FOLLOWS_ORGANIZATIONAL_PAGE",
                        "display_name": "Members",
                        "short_desc": "Members who follow the page",
                        "desc": "Members who follow the page",
                    },
                    <AllowedValueInfo>{
                        "value": "ORGANIZATIONAL_PAGE_FOLLOWS_ORGANIZATIONAL_PAGE",
                        "display_name": "Organizations",
                        "short_desc": "Other organizational pages that follow the page",
                        "desc": "Other organizational pages that follow the page",
                    },
                ),
            },
        } + LinkedInPagingFields;
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInListFollowersRequestDataType = new LinkedInListFollowersRequestDataType();

#! Data provider for listing the followers of an organization's page
public class LinkedInListFollowersDataProvider inherits LinkedInOrgActionProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-followers",
            "type": "LinkedInListFollowersDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = LinkedInListFollowersRequestDataType;
        const ResponseType = LinkedInFollowListDataType;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest) : LinkedInOrgActionProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInOrgActionProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> page = doCursorFinder(PageFollowsPath, {
            "q": "followee",
            "followee": organizationalPageUrn(req.organization),
            "edgeType": req.edge_type ?? "MEMBER_FOLLOWS_ORGANIZATIONAL_PAGE",
        }, (req.count ?? DefaultPageSize).toInt(), req.cursor);
        return {
            "follows": (map LinkedInOrganizationsUtil::normalizeFollow($1), page.elements),
            "cursor": page.cursor,
        };
    }
}

#! Request type for listing the pages an organization follows
public class LinkedInListFollowingRequestDataType inherits HashDataType {
    public {
        const Fields = LinkedInOrganizationField + LinkedInPagingFields;
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInListFollowingRequestDataType = new LinkedInListFollowingRequestDataType();

#! Data provider for listing the pages an organization's page follows
public class LinkedInListFollowingDataProvider inherits LinkedInOrgActionProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-following",
            "type": "LinkedInListFollowingDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = LinkedInListFollowingRequestDataType;
        const ResponseType = LinkedInFollowListDataType;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest) : LinkedInOrgActionProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInOrgActionProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> page = doCursorFinder(PageFollowsPath, {
            "q": "follower",
            "follower": organizationalPageUrn(req.organization),
        }, (req.count ?? DefaultPageSize).toInt(), req.cursor);
        return {
            "follows": (map LinkedInOrganizationsUtil::normalizeFollow($1), page.elements),
            "cursor": page.cursor,
        };
    }
}

#! Common base for the page analytics providers
public class LinkedInAnalyticsDataProviderBase inherits LinkedInOrgActionProviderBase {
    constructor(LinkedInRestClient::LinkedInRestClientIo rest) : LinkedInOrgActionProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInOrgActionProviderBase(options) {}

    #! Runs the dimension finder and returns the breakdown
    /** @param req the action request
        @param analytics_type \c "FOLLOWER" or \c "VISITOR"
        @param extra any additional query parameters

        @return the breakdown matching @ref LinkedInAnalyticsDataType
    */
    private hash<auto> getBreakdown(hash<auto> req, string analytics_type, *hash<auto> extra) {
        hash<auto!> query = {
            "q": "dimension",
            "organizationalPage": organizationalPageUrn(req.organization),
            "analyticsType": analytics_type,
            "dimensionType": req.dimension_type,
        } + (extra ?? {});

        hash<auto> resp = doDmaRequest("GET", EdgeAnalyticsPath, query);
        list<auto> rows = ();
        int total = 0;
        foreach hash<auto> element in (resp.body.elements ?? ()) {
            hash<auto> row = LinkedInOrganizationsUtil::normalizeAnalytics(element);
            total += (row.count ?? 0).toInt();
            push rows, row;
        }
        return {
            "rows": rows,
            "total": total,
        };
    }

    #! Returns a Rest.li time range value, or NOTHING when the range is not set
    /** @param start the inclusive start
        @param end the exclusive end

        @return \c "(start:<ms>,end:<ms>)", or @ref nothing when neither bound was given
    */
    static *string timeRangeValue(*date start, *date end) {
        if (!exists start && !exists end) {
            return;
        }
        list<string> parts = ();
        if (exists start) {
            push parts, sprintf("start:%d", LinkedInRestClient::linkedin_date_to_ms(start));
        }
        if (exists end) {
            push parts, sprintf("end:%d", LinkedInRestClient::linkedin_date_to_ms(end));
        }
        return sprintf("(%s)", join(",", parts));
    }
}

#! Request type for a follower breakdown
public class LinkedInGetFollowerStatisticsRequestDataType inherits HashDataType {
    public {
        const Fields = LinkedInOrganizationField + {
            "dimension_type": {
                "display_name": "Break Down By",
                "short_desc": "The member attribute to break the followers down by",
                "desc": "The member attribute to break the followers down by",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "default_value": "INDUSTRY",
                "allowed_values": AnalyticsDimensionValues,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInGetFollowerStatisticsRequestDataType =
    new LinkedInGetFollowerStatisticsRequestDataType();

#! Data provider for an organization's follower demographics
public class LinkedInGetFollowerStatisticsDataProvider inherits LinkedInAnalyticsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-follower-statistics",
            "type": "LinkedInGetFollowerStatisticsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = LinkedInGetFollowerStatisticsRequestDataType;
        const ResponseType = LinkedInAnalyticsDataType;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest)
            : LinkedInAnalyticsDataProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInAnalyticsDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # NOTE: no time range is sent.  Follower analytics are a snapshot of the page's current
        # followers and LinkedIn documents that it **ignores** timeRange for FOLLOWER, so sending
        # one would suggest a filter that does not exist
        return getBreakdown(req, "FOLLOWER");
    }
}

#! Request type for a visitor breakdown
public class LinkedInGetVisitorStatisticsRequestDataType inherits HashDataType {
    public {
        const Fields = LinkedInOrganizationField + {
            "dimension_type": {
                "display_name": "Break Down By",
                "short_desc": "The member attribute to break the visitors down by",
                "desc": "The member attribute to break the visitors down by",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "default_value": "INDUSTRY",
                "allowed_values": AnalyticsDimensionValues,
            },
            "start": {
                "display_name": "From",
                "short_desc": "The start of the period to count visits in",
                "desc": "The start of the period to count visits in (inclusive)",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "end": {
                "display_name": "To",
                "short_desc": "The end of the period to count visits in",
                "desc": "The end of the period to count visits in (exclusive)",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "source_type": {
                "display_name": "Page Section",
                "short_desc": "Count visits to one section of the page only",
                "desc": "Count visits to one section of the page only; when unset, visits to every "
                    "section are counted together",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": VisitorSourceValues,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInGetVisitorStatisticsRequestDataType =
    new LinkedInGetVisitorStatisticsRequestDataType();

#! Data provider for an organization's page visitor demographics
public class LinkedInGetVisitorStatisticsDataProvider inherits LinkedInAnalyticsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-visitor-statistics",
            "type": "LinkedInGetVisitorStatisticsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys});
        const RequestType = LinkedInGetVisitorStatisticsRequestDataType;
        const ResponseType = LinkedInAnalyticsDataType;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest)
            : LinkedInAnalyticsDataProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInAnalyticsDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto!> extra = {};
        if (*string range = LinkedInAnalyticsDataProviderBase::timeRangeValue(req.start, req.end)) {
            extra.timeRange = range;
        }
        if (req.source_type.val()) {
            extra.sourceType = req.source_type;
        }
        return getBreakdown(req, "VISITOR", extra);
    }
}
}
