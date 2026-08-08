# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinkedInOrganizationsDataProvider module definition

/** LinkedInOrganizationsResponseTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! A DMA audit stamp
public class LinkedInAuditStampDataType inherits HashDataType {
    private {
        const Fields = {
            "time": {
                "display_name": "Time",
                "short_desc": "When the action happened",
                "desc": "When the action happened",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "actor": {
                "display_name": "Actor URN",
                "short_desc": "The URN of the entity that performed the action",
                "desc": "The URN of the entity that performed the action.\n\n"
                    "**Absent** when the actor is a member who has not consented to sharing their "
                    "data through LinkedIn's Data Portability APIs; that consent is off by default.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "impersonator": {
                "display_name": "Impersonator URN",
                "short_desc": "The URN of the entity that acted on the actor's behalf",
                "desc": "The URN of the entity that acted on the actor's behalf",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInAuditStampDataType = new LinkedInAuditStampDataType();

#! A LinkedIn organization
public class LinkedInOrganizationDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "Organization ID",
                "short_desc": "The organization's numeric ID",
                "desc": "The organization's numeric ID, which is what the other actions take",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "urn": {
                "display_name": "Organization URN",
                "short_desc": "The organization's URN",
                "desc": "The organization's URN (`urn:li:organization:{id}`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "organizational_page": {
                "display_name": "Organizational Page URN",
                "short_desc": "The URN of the organization's page",
                "desc": "The URN of the organization's page (`urn:li:organizationalPage:{id}`), "
                    "which is what the analytics and follows APIs address rather than the "
                    "organization URN",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The organization's name",
                "desc": "The organization's name",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "The organization's description",
                "desc": "The organization's description",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tagline": {
                "display_name": "Tagline",
                "short_desc": "The organization's tagline",
                "desc": "The organization's tagline",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "website": {
                "display_name": "Website",
                "short_desc": "The organization's website",
                "desc": "The organization's website",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "vanity_name": {
                "display_name": "Vanity Name",
                "short_desc": "The organization's vanity name",
                "desc": "The organization's vanity name, the last segment of its public page URL",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "url": {
                "display_name": "Page URL",
                "short_desc": "The organization's public page URL",
                "desc": "The organization's public page URL",
                "type": AbstractDataProviderTypeMap."*uri",
            },
            "organization_type": {
                "display_name": "Organization Type",
                "short_desc": "The kind of organization",
                "desc": "The kind of organization, e.g. `PRIVATELY_HELD`, `PUBLIC_COMPANY`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "staff_count_range": {
                "display_name": "Company Size",
                "short_desc": "The organization's size band",
                "desc": "The organization's size band, e.g. `SIZE_1001_TO_5000`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "entity_status": {
                "display_name": "Status",
                "short_desc": "Whether the organization page is active",
                "desc": "Whether the organization page is active",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "founded_year": {
                "display_name": "Founded",
                "short_desc": "The year the organization was founded",
                "desc": "The year the organization was founded",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "industries": {
                "display_name": "Industries",
                "short_desc": "The organization's industry URNs",
                "desc": "The organization's industry URNs",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "specialties": {
                "display_name": "Specialties",
                "short_desc": "The organization's specialties",
                "desc": "The organization's specialties",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "logo_url": {
                "display_name": "Logo URL",
                "short_desc": "A URL of the organization's logo",
                "desc": "A URL of the organization's logo; the URL expires",
                "type": AbstractDataProviderTypeMap."*uri",
            },
            "cover_photo_url": {
                "display_name": "Cover Photo URL",
                "short_desc": "A URL of the organization's cover photo",
                "desc": "A URL of the organization's cover photo; the URL expires",
                "type": AbstractDataProviderTypeMap."*uri",
            },
            "call_to_action_url": {
                "display_name": "Call-To-Action URL",
                "short_desc": "The URL the page's call-to-action button opens",
                "desc": "The URL the page's call-to-action button opens",
                "type": AbstractDataProviderTypeMap."*uri",
            },
            "call_to_action_type": {
                "display_name": "Call-To-Action Type",
                "short_desc": "The label on the page's call-to-action button",
                "desc": "The label on the page's call-to-action button",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "phone_number": {
                "display_name": "Phone Number",
                "short_desc": "The organization's primary phone number",
                "desc": "The organization's primary phone number",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "auto_created": {
                "display_name": "Auto Created",
                "short_desc": "True if LinkedIn created the page automatically",
                "desc": "True if LinkedIn created the page automatically rather than an "
                    "administrator creating it",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "created": {
                "display_name": "Created",
                "short_desc": "When the page was created and by whom",
                "desc": "When the page was created and by whom",
                "type": LinkedInAuditStampDataType,
            },
            "last_modified": {
                "display_name": "Last Modified",
                "short_desc": "When an administrator last modified the page",
                "desc": "When an administrator last modified the page",
                "type": LinkedInAuditStampDataType,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInOrganizationDataType = new LinkedInOrganizationDataType();

#! A list of organizations
public class LinkedInOrganizationListDataType inherits HashDataType {
    private {
        const Fields = {
            "organizations": {
                "display_name": "Organizations",
                "short_desc": "The organizations",
                "desc": "The organizations",
                "type": new ListDataType("LinkedInOrganizations", LinkedInOrganizationDataType, True),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInOrganizationListDataType = new LinkedInOrganizationListDataType();

#! An organization's post
public class LinkedInOrgPostDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "Post URN",
                "short_desc": "The post's URN",
                "desc": "The post's URN, e.g. `urn:li:share:1188488118`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "url": {
                "display_name": "Post URL",
                "short_desc": "The public URL of the post",
                "desc": "The URL a browser can open the post at",
                "type": AbstractDataProviderTypeMap."*uri",
            },
            "commentary": {
                "display_name": "Text",
                "short_desc": "The post's text",
                "desc": "The post's text",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "visibility": {
                "display_name": "Visibility",
                "short_desc": "Who can see the post",
                "desc": "Who can see the post",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "lifecycle_state": {
                "display_name": "Lifecycle State",
                "short_desc": "The publication state of the post",
                "desc": "The publication state of the post: `PUBLISHED`, `DRAFT`, "
                    "`PUBLISH_REQUESTED` or `PUBLISH_FAILED`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "content": {
                "display_name": "Content",
                "short_desc": "The post's attached content, if any",
                "desc": "The post's attached content as LinkedIn returns it: one of `media`, "
                    "`article`, `multiImage`, `poll`, `carousel`, `celebration` or `reference`",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "feed_distribution": {
                "display_name": "Feed Distribution",
                "short_desc": "Where the post is distributed",
                "desc": "Where the post is distributed",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "third_party_distribution_channels": {
                "display_name": "Third-Party Distribution Channels",
                "short_desc": "External channels the post is distributed to",
                "desc": "External channels the post is distributed to",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "container": {
                "display_name": "Container URN",
                "short_desc": "The URN of the container the post belongs to",
                "desc": "The URN of the container the post belongs to, if any",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "is_reshare_disabled_by_author": {
                "display_name": "Reshare Disabled",
                "short_desc": "True if the author disabled resharing",
                "desc": "True if the author disabled resharing",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "is_edited_by_author": {
                "display_name": "Edited By Author",
                "short_desc": "True if the author edited the post after publishing it",
                "desc": "True if the author edited the post after publishing it",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "is_sponsored": {
                "display_name": "Sponsored",
                "short_desc": "True if the post exists solely as sponsored content",
                "desc": "True if the post exists solely as sponsored content (a dark post)",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "content_call_to_action_label": {
                "display_name": "Call-To-Action Label",
                "short_desc": "The call-to-action label shown with the content",
                "desc": "The call-to-action label shown with the content",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "content_landing_page": {
                "display_name": "Landing Page",
                "short_desc": "The URL the call to action opens",
                "desc": "The URL the call to action opens",
                "type": AbstractDataProviderTypeMap."*uri",
            },
            "reshare_parent": {
                "display_name": "Reshared Post URN",
                "short_desc": "The URN of the post this post reshares",
                "desc": "The URN of the post this post reshares",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "reshare_root": {
                "display_name": "Reshare Root URN",
                "short_desc": "The URN of the original post at the root of the reshare chain",
                "desc": "The URN of the original post at the root of the reshare chain",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "published_at": {
                "display_name": "Published",
                "short_desc": "When the post was published",
                "desc": "When the post was published",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "scheduled_at": {
                "display_name": "Scheduled",
                "short_desc": "When the post is scheduled to be published",
                "desc": "When the post is scheduled to be published.  Only set for a scheduled "
                    "post, and only visible when the post is read with the `AUTHOR` view context",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "created": {
                "display_name": "Created",
                "short_desc": "When the post was created and by whom",
                "desc": "When the post was created and by whom",
                "type": LinkedInAuditStampDataType,
            },
            "last_modified": {
                "display_name": "Last Modified",
                "short_desc": "When the post was last modified and by whom",
                "desc": "When the post was last modified and by whom",
                "type": LinkedInAuditStampDataType,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInOrgPostDataType = new LinkedInOrgPostDataType();

#! A cursor-paged list of an organization's posts
public class LinkedInOrgPostListDataType inherits HashDataType {
    private {
        const Fields = {
            "posts": {
                "display_name": "Posts",
                "short_desc": "The posts, newest first",
                "desc": "The posts, newest first",
                "type": new ListDataType("LinkedInOrgPosts", LinkedInOrgPostDataType, True),
            },
            "cursor": {
                "display_name": "Next Cursor",
                "short_desc": "The cursor that resumes this listing",
                "desc": "Pass this back in `cursor` to retrieve the next page.  It is **absent** at "
                    "the end of the collection, which is the only reliable way to know the listing "
                    "is finished — a short page does not mean there are no more items",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInOrgPostListDataType = new LinkedInOrgPostListDataType();

#! A comment on an organization's content
public class LinkedInCommentDataType inherits HashDataType {
    private {
        const Fields = {
            "urn": {
                "display_name": "Comment URN",
                "short_desc": "The comment's URN",
                "desc": "The comment's composite URN, e.g. "
                    "`urn:li:comment:(urn:li:activity:123,456)`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "id": {
                "display_name": "Comment ID",
                "short_desc": "The comment's numeric ID",
                "desc": "The comment's numeric ID",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "object": {
                "display_name": "Commented Entity URN",
                "short_desc": "The URN of the post or comment being commented on",
                "desc": "The URN of the post or comment being commented on",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "parent_comment": {
                "display_name": "Parent Comment URN",
                "short_desc": "The URN of the comment this replies to",
                "desc": "The URN of the comment this replies to; absent for a top-level comment",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "text": {
                "display_name": "Text",
                "short_desc": "The comment's text",
                "desc": "The comment's text.\n\n"
                    "**Absent** when the commenting member has not consented to sharing their data "
                    "through LinkedIn's Data Portability APIs.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "content": {
                "display_name": "Content",
                "short_desc": "Media attached to the comment",
                "desc": "Media attached to the comment, such as an image",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "created": {
                "display_name": "Created",
                "short_desc": "When the comment was created and by whom",
                "desc": "When the comment was created and by whom",
                "type": LinkedInAuditStampDataType,
            },
            "last_modified": {
                "display_name": "Last Modified",
                "short_desc": "When the comment was last modified and by whom",
                "desc": "When the comment was last modified and by whom",
                "type": LinkedInAuditStampDataType,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInCommentDataType = new LinkedInCommentDataType();

#! A cursor-paged list of comments
public class LinkedInCommentListDataType inherits HashDataType {
    private {
        const Fields = {
            "comments": {
                "display_name": "Comments",
                "short_desc": "The comments, newest first",
                "desc": "The comments, newest first",
                "type": new ListDataType("LinkedInComments", LinkedInCommentDataType, True),
            },
            "cursor": {
                "display_name": "Next Cursor",
                "short_desc": "The cursor that resumes this listing",
                "desc": "Pass this back in `cursor` to retrieve the next page; absent at the end "
                    "of the collection",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInCommentListDataType = new LinkedInCommentListDataType();

#! A reaction on an organization's content
public class LinkedInReactionDataType inherits HashDataType {
    private {
        const Fields = {
            "urn": {
                "display_name": "Reaction URN",
                "short_desc": "The reaction's URN",
                "desc": "The reaction's composite URN, which embeds the reacting entity: "
                    "`urn:li:reaction:(urn:li:person:ABC,urn:li:activity:123)`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "entity": {
                "display_name": "Entity URN",
                "short_desc": "The URN of the content reacted to",
                "desc": "The URN of the content reacted to",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "reaction_type": {
                "display_name": "Reaction",
                "short_desc": "Which reaction was given",
                "desc": "Which reaction was given: `LIKE` (Like), `PRAISE` (Celebrate), `EMPATHY` "
                    "(Love), `INTEREST` (Insightful), `APPRECIATION` (Support) or `ENTERTAINMENT` "
                    "(Funny)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "created": {
                "display_name": "Created",
                "short_desc": "When the reaction was given and by whom",
                "desc": "When the reaction was given and by whom",
                "type": LinkedInAuditStampDataType,
            },
            "last_modified": {
                "display_name": "Last Modified",
                "short_desc": "When the reaction was last changed",
                "desc": "When the reaction was last changed",
                "type": LinkedInAuditStampDataType,
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInReactionDataType = new LinkedInReactionDataType();

#! A cursor-paged list of reactions
public class LinkedInReactionListDataType inherits HashDataType {
    private {
        const Fields = {
            "reactions": {
                "display_name": "Reactions",
                "short_desc": "The reactions, newest first",
                "desc": "The reactions, newest first",
                "type": new ListDataType("LinkedInReactions", LinkedInReactionDataType, True),
            },
            "cursor": {
                "display_name": "Next Cursor",
                "short_desc": "The cursor that resumes this listing",
                "desc": "Pass this back in `cursor` to retrieve the next page; absent at the end "
                    "of the collection",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInReactionListDataType = new LinkedInReactionListDataType();

#! One reaction type's count on a piece of content
public class LinkedInReactionCountDataType inherits HashDataType {
    private {
        const Fields = {
            "reaction_type": {
                "display_name": "Reaction",
                "short_desc": "The reaction type",
                "desc": "The reaction type",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "count": {
                "display_name": "Count",
                "short_desc": "How many of this reaction the content has",
                "desc": "How many of this reaction the content has",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInReactionCountDataType = new LinkedInReactionCountDataType();

#! The engagement summary of a piece of content
public class LinkedInSocialMetadataDataType inherits HashDataType {
    private {
        const Fields = {
            "entity": {
                "display_name": "Entity URN",
                "short_desc": "The URN of the content",
                "desc": "The URN of the content",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "url": {
                "display_name": "URL",
                "short_desc": "The public URL of the content",
                "desc": "The public URL of the content",
                "type": AbstractDataProviderTypeMap."*uri",
            },
            "reaction_count": {
                "display_name": "Total Reactions",
                "short_desc": "The total number of reactions of every type",
                "desc": "The total number of reactions of every type",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "reactions": {
                "display_name": "Reactions By Type",
                "short_desc": "The count of each reaction type",
                "desc": "The count of each reaction type; only types with at least one reaction "
                    "appear",
                "type": new ListDataType("LinkedInReactionCounts", LinkedInReactionCountDataType,
                    True),
            },
            "comment_count": {
                "display_name": "Comments",
                "short_desc": "The number of comments, including replies",
                "desc": "The number of comments, including replies",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "top_level_comment_count": {
                "display_name": "Top-Level Comments",
                "short_desc": "The number of comments, excluding replies",
                "desc": "The number of comments, excluding replies",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "comments_state": {
                "display_name": "Comments State",
                "short_desc": "Whether the comment thread is open",
                "desc": "Whether the comment thread is `OPEN`, `CLOSED`, `FROZEN`, `PROCESSING` or "
                    "`DELETED`",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInSocialMetadataDataType = new LinkedInSocialMetadataDataType();

#! A list of engagement summaries
public class LinkedInSocialMetadataListDataType inherits HashDataType {
    private {
        const Fields = {
            "entities": {
                "display_name": "Entities",
                "short_desc": "The engagement summary of each requested entity",
                "desc": "The engagement summary of each requested entity",
                "type": new ListDataType("LinkedInSocialMetadataList",
                    LinkedInSocialMetadataDataType, True),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInSocialMetadataListDataType = new LinkedInSocialMetadataListDataType();

#! A follow relationship
public class LinkedInFollowDataType inherits HashDataType {
    private {
        const Fields = {
            "follower": {
                "display_name": "Follower URN",
                "short_desc": "The URN of the follower",
                "desc": "The URN of the following member or organizational page.\n\n"
                    "**Absent** when the follower is a member who has not consented to sharing "
                    "their data through LinkedIn's Data Portability APIs; that consent is off by "
                    "default, so many entries carry only the timestamp.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "followee": {
                "display_name": "Followee URN",
                "short_desc": "The URN of the page being followed",
                "desc": "The URN of the organizational page being followed",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "edge_type": {
                "display_name": "Edge Type",
                "short_desc": "Whether the follower is a member or an organizational page",
                "desc": "Whether the follower is a member (`MEMBER_FOLLOWS_ORGANIZATIONAL_PAGE`) or "
                    "another page (`ORGANIZATIONAL_PAGE_FOLLOWS_ORGANIZATIONAL_PAGE`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "last_modified_at": {
                "display_name": "Last Modified",
                "short_desc": "When the follow relationship last changed",
                "desc": "When the follow relationship last changed",
                "type": AbstractDataProviderTypeMap."*date",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInFollowDataType = new LinkedInFollowDataType();

#! A cursor-paged list of follow relationships
public class LinkedInFollowListDataType inherits HashDataType {
    private {
        const Fields = {
            "follows": {
                "display_name": "Follows",
                "short_desc": "The follow relationships",
                "desc": "The follow relationships",
                "type": new ListDataType("LinkedInFollows", LinkedInFollowDataType, True),
            },
            "cursor": {
                "display_name": "Next Cursor",
                "short_desc": "The cursor that resumes this listing",
                "desc": "Pass this back in `cursor` to retrieve the next page; absent at the end "
                    "of the collection",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInFollowListDataType = new LinkedInFollowListDataType();

#! One row of a page analytics breakdown
public class LinkedInAnalyticsRowDataType inherits HashDataType {
    private {
        const Fields = {
            "dimension": {
                "display_name": "Dimension",
                "short_desc": "The value this row counts",
                "desc": "The value this row counts: a URN for most breakdowns, a size band for "
                    "company size, or a relationship for `RELATIONSHIP`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "dimension_type": {
                "display_name": "Dimension Type",
                "short_desc": "The breakdown this row belongs to",
                "desc": "The breakdown this row belongs to",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "count": {
                "display_name": "Count",
                "short_desc": "The number of members in this row",
                "desc": "The number of members in this row.\n\n"
                    "LinkedIn deliberately perturbs and floors these values to protect member "
                    "privacy: a count below an undisclosed threshold is reported as `0`, and the "
                    "same request repeated can return slightly different numbers.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "analytics_type": {
                "display_name": "Analytics Type",
                "short_desc": "Whether the row counts followers or visitors",
                "desc": "Whether the row counts followers or visitors",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "source_type": {
                "display_name": "Page Section",
                "short_desc": "The page section the visits were made to",
                "desc": "The page section the visits were made to; visitor analytics only",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "organic_count": {
                "display_name": "Organic",
                "short_desc": "The part of the count that was not sponsored",
                "desc": "The part of the count that was not sponsored; follower analytics only",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "sponsored_count": {
                "display_name": "Sponsored",
                "short_desc": "The part of the count that came from sponsored content",
                "desc": "The part of the count that came from sponsored content; follower analytics "
                    "only",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "desktop_count": {
                "display_name": "Desktop",
                "short_desc": "The part of the count made from a desktop browser",
                "desc": "The part of the count made from a desktop browser; visitor analytics only",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "mobile_count": {
                "display_name": "Mobile",
                "short_desc": "The part of the count made from a mobile device",
                "desc": "The part of the count made from a mobile device; visitor analytics only",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "organizational_page": {
                "display_name": "Organizational Page URN",
                "short_desc": "The page the row belongs to",
                "desc": "The page the row belongs to",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInAnalyticsRowDataType = new LinkedInAnalyticsRowDataType();

#! A page analytics breakdown
public class LinkedInAnalyticsDataType inherits HashDataType {
    private {
        const Fields = {
            "rows": {
                "display_name": "Rows",
                "short_desc": "The breakdown, one row per dimension value",
                "desc": "The breakdown, one row per dimension value",
                "type": new ListDataType("LinkedInAnalyticsRows", LinkedInAnalyticsRowDataType, True),
            },
            "total": {
                "display_name": "Total",
                "short_desc": "The sum of every row's count",
                "desc": "The sum of every row's count.  Because LinkedIn floors small counts to "
                    "zero, this is a lower bound rather than the true total",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInAnalyticsDataType = new LinkedInAnalyticsDataType();

#! The authenticated member, as the DMA APIs see them
public class LinkedInDmaMemberDataType inherits HashDataType {
    private {
        const Fields = {
            "urn": {
                "display_name": "Member URN",
                "short_desc": "The authenticated member's URN",
                "desc": "The authenticated member's URN",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "id": {
                "display_name": "Member ID",
                "short_desc": "The authenticated member's ID",
                "desc": "The authenticated member's ID",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "first_name": {
                "display_name": "First Name",
                "short_desc": "The member's first name",
                "desc": "The member's first name",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "last_name": {
                "display_name": "Last Name",
                "short_desc": "The member's last name",
                "desc": "The member's last name",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "headline": {
                "display_name": "Headline",
                "short_desc": "The member's profile headline",
                "desc": "The member's profile headline",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "raw": {
                "display_name": "Raw Profile",
                "short_desc": "The profile exactly as LinkedIn returned it",
                "desc": "The profile exactly as LinkedIn returned it, for fields this type does not "
                    "name",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInDmaMemberDataType = new LinkedInDmaMemberDataType();
}
