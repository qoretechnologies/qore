# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinkedInOrganizationsDataProvider module definition

/** LinkedInOrganizationsDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Static helpers shared by the LinkedIn Organizations providers
public class LinkedInOrganizationsUtil {
    #! Returns the ID part of a simple URN
    /** @param urn a URN of the form \c "urn:li:organization:123"

        @return the trailing ID

        @throw LINKEDIN-URN-ERROR the value is not a simple URN; a **composite** URN such as
        \c "urn:li:comment:(urn:li:activity:1,2)" has no single trailing ID and is rejected rather
        than silently truncated to its last component
    */
    static string urnId(string urn) {
        if (urn !~ /^urn:li:[A-Za-z]+:[^():,]+$/) {
            throw "LINKEDIN-URN-ERROR", sprintf("%y is not a simple URN of the form "
                "\"urn:li:<type>:<id>\"", urn);
        }
        list<string> parts = split(":", urn);
        return parts[parts.size() - 1];
    }

    #! Splits a list into chunks of at most \a size entries
    /** @param items the items to split
        @param size the maximum chunk size

        @return a list of lists; an empty input yields an empty list
    */
    static list<auto> chunk(list<auto> items, int size) {
        if (size < 1) {
            throw "LINKEDIN-BATCH-ERROR", sprintf("chunk size %d is not positive", size);
        }
        list<auto> rv = ();
        int offset = 0;
        while (offset < items.size()) {
            int last = min(offset + size, items.size()) - 1;
            push rv, items[offset..last];
            offset += size;
        }
        return rv;
    }

    #! Returns the next pagination cursor from a DMA finder response
    /** The DMA APIs report the cursor in **two different places**: the feed finders nest it under
        \c metadata.paginationCursorMetdata (LinkedIn's own spelling, missing an \c "a"), while the
        follows finders put it directly on \c metadata.  Reading only one of them silently ends
        pagination after the first page on the other family.

        @param body the finder response body

        @return the next cursor, or @ref nothing at the end of the collection
    */
    static *string nextCursor(auto body) {
        if (body.typeCode() != NT_HASH) {
            return;
        }
        foreach auto candidate in ((body.metadata.paginationCursorMetdata.nextPaginationCursor,
            body.metadata.paginationCursorMetadata.nextPaginationCursor,
            body.metadata.nextPaginationCursor)) {
            if (candidate.typeCode() == NT_STRING && candidate.val()) {
                return candidate;
            }
        }
    }

    #! Returns True if a query parameter value is a Rest.li structural value
    /** Rest.li encodes structured query values as \c "(key:value)" and \c "List(a,b)", and those
        parentheses, colons and commas are **syntax**: percent-encoding them addresses a different
        query, which LinkedIn rejects.  Everything else is an ordinary value and is encoded.
    */
    static bool isRestliValue(string value) {
        return value =~ /^(List)?\(.*\)$/;
    }

    #! Returns the public LinkedIn URL of a post
    static *string postUrl(*string urn) {
        if (!urn.val() || urn !~ /^urn:li:/) {
            return;
        }
        return "https://www.linkedin.com/feed/update/" + urn + "/";
    }

    #! Normalizes a DMA audit stamp
    /** An audit stamp carries the acting member's URN, which LinkedIn **omits entirely** when that
        member has not consented to sharing their data through the DMA APIs; only \c time is
        reliably present.
    */
    static hash<auto> normalizeAuditStamp(auto stamp) {
        if (stamp.typeCode() != NT_HASH) {
            return {};
        }
        hash<auto!> rv = {};
        if (exists stamp.actor) {
            rv.actor = stamp.actor;
        }
        if (exists stamp.impersonator) {
            rv.impersonator = stamp.impersonator;
        }
        if (exists stamp.time) {
            rv.time = LinkedInRestClient::linkedin_ms_to_date(stamp.time);
        }
        return rv;
    }

    #! Normalizes a DMA Organization resource
    /** The DMA shape carries both a localized map and a \c localized* convenience field for each
        text; the convenience field is used, and the numeric \c id is turned back into the URN forms
        the other DMA APIs take, so a caller never has to build one by string concatenation.
    */
    static hash<auto> normalizeOrganization(hash<auto> org) {
        hash<auto!> rv = {
            "id": exists org.id ? org.id.toString() : NOTHING,
            "urn": exists org.id ? "urn:li:organization:" + org.id : NOTHING,
            "organizational_page": org.organizationalPage,
            "name": org.localizedName,
            "description": org.localizedDescription,
            "tagline": org.localizedTagline,
            "website": org.localizedWebsite,
            "vanity_name": org.vanityName,
            "url": org.vanityName.val()
                ? "https://www.linkedin.com/company/" + org.vanityName
                : NOTHING,
            "organization_type": org.organizationType,
            "staff_count_range": org.staffCountRange,
            "entity_status": org.entityStatus,
            "founded_year": org.foundedOn.year,
            "industries": org.industriesV2,
            "specialties": org.localizedSpecialties,
            "logo_url": org.logoV2.cropped.downloadUrl ?? org.logoV2.original.downloadUrl,
            "cover_photo_url": org.coverPhotoV2.cropped.downloadUrl
                ?? org.coverPhotoV2.original.downloadUrl,
            "auto_created": org.autoCreated,
        };
        if (org.callToAction.typeCode() == NT_HASH) {
            rv.call_to_action_url = org.callToAction.redirectUrl;
            rv.call_to_action_type = org.callToAction.ctaType;
        }
        if (exists org.primaryPhoneNumber.number) {
            rv.phone_number = org.primaryPhoneNumber.number;
        }
        if (org.created.typeCode() == NT_HASH) {
            rv.created = LinkedInOrganizationsUtil::normalizeAuditStamp(org.created);
        }
        if (org.lastModifiedByAdmin.typeCode() == NT_HASH) {
            rv.last_modified = LinkedInOrganizationsUtil::normalizeAuditStamp(org.lastModifiedByAdmin);
        }
        return rv;
    }

    #! Normalizes a DMA Post resource
    /** The DMA post shape differs from the member Posts API: there is no \c author field, and the
        timestamps arrive as audit stamps (\c created.time) rather than as \c createdAt.
    */
    static hash<auto> normalizePost(hash<auto> post) {
        hash<auto!> rv = {
            "id": post.id,
            "url": LinkedInOrganizationsUtil::postUrl(post.id),
            "commentary": post.commentary,
            "visibility": post.visibility,
            "lifecycle_state": post.lifecycleState,
            "is_reshare_disabled_by_author": post.isReshareDisabledByAuthor,
            "is_edited_by_author": post.lifecycleStateInfo.isEditedByAuthor,
            "feed_distribution": post.distribution.feedDistribution,
            "third_party_distribution_channels": post.distribution.thirdPartyDistributionChannels,
            "container": post.container,
            "content_call_to_action_label": post.contentCallToActionLabel,
            "content_landing_page": post.contentLandingPage,
            "published_at": LinkedInRestClient::linkedin_ms_to_date(post.publishedAt),
            "scheduled_at": LinkedInRestClient::linkedin_ms_to_date(post.scheduledAt),
            "created": LinkedInOrganizationsUtil::normalizeAuditStamp(post.created),
            "last_modified": LinkedInOrganizationsUtil::normalizeAuditStamp(post.lastModified),
        };
        if (post.reshareContext.typeCode() == NT_HASH) {
            rv.reshare_parent = post.reshareContext.parent;
            rv.reshare_root = post.reshareContext.root;
        }
        if (post.content.typeCode() == NT_HASH) {
            # the content union is passed through as-is: a DMA post can carry any of media, poll,
            # multiImage, article, carousel, celebration or reference, and flattening that into a
            # fixed set of fields would silently drop whichever shapes were not modelled
            rv.content = post.content;
        }
        if (post.adContext.typeCode() == NT_HASH) {
            rv.is_sponsored = post.adContext.isDsc;
        }
        return rv;
    }

    #! Normalizes a DMA Comment resource
    static hash<auto> normalizeComment(string urn, hash<auto> comment) {
        hash<auto!> rv = {
            "urn": urn,
            "id": exists comment.id ? comment.id.toString() : NOTHING,
            "object": comment.object,
            "parent_comment": comment.parentComment,
            "text": comment.commentary.text,
            "created": LinkedInOrganizationsUtil::normalizeAuditStamp(comment.created),
            "last_modified": LinkedInOrganizationsUtil::normalizeAuditStamp(comment.lastModified),
        };
        if (exists comment.content) {
            rv.content = comment.content;
        }
        return rv;
    }

    #! Normalizes a DMA Reaction resource
    static hash<auto> normalizeReaction(string urn, hash<auto> reaction) {
        return {
            "urn": urn,
            "entity": reaction.entity,
            "reaction_type": reaction.reactionType,
            "created": LinkedInOrganizationsUtil::normalizeAuditStamp(reaction.created),
            "last_modified": LinkedInOrganizationsUtil::normalizeAuditStamp(reaction.lastModified),
        };
    }

    #! Normalizes a DMA Social Metadata resource
    /** LinkedIn returns the reaction counts as a map keyed by reaction type; it is flattened into a
        list and totalled, because the useful questions ("how many reactions?", "which type won?")
        cannot be answered from a map without iterating it.
    */
    static hash<auto> normalizeSocialMetadata(string urn, hash<auto> meta) {
        list<auto> reactions = ();
        int total = 0;
        foreach hash<auto> i in ((meta.reactionSummaries ?? {}).pairIterator()) {
            int count = i.value.count.toInt();
            total += count;
            push reactions, {
                "reaction_type": i.value.reactionType ?? i.key,
                "count": count,
            };
        }
        return {
            "entity": meta.entity ?? urn,
            "url": LinkedInOrganizationsUtil::postUrl(meta.entity ?? urn),
            "comments_state": meta.commentsState,
            "comment_count": meta.commentSummary.count ?? 0,
            "top_level_comment_count": meta.commentSummary.topLevelCount ?? 0,
            "reaction_count": total,
            "reactions": reactions,
        };
    }

    #! Normalizes an Organizational Page Follows element
    static hash<auto> normalizeFollow(hash<auto> follow) {
        hash<auto!> rv = {
            "followee": follow.followee,
            "edge_type": follow.edgeType,
            "last_modified_at": LinkedInRestClient::linkedin_ms_to_date(follow.lastModifiedAt),
        };
        # the follower URN is absent when that member has not consented to sharing their data
        if (exists follow.follower) {
            rv.follower = follow.follower;
        }
        return rv;
    }

    #! Normalizes an Organizational Page Edge Analytics element
    /** The dimension value is a union — a URN for most breakdowns, a staff-count-range string for
        company size, a relationship for \c RELATIONSHIP — and the count is nested two levels deep
        under \c value.totalCount.long.
    */
    static hash<auto> normalizeAnalytics(hash<auto> element) {
        hash<auto!> rv = {
            "analytics_type": element.type,
            "organizational_page": element.organizationalPage,
            "dimension_type": element.dimension.type,
            "dimension": LinkedInOrganizationsUtil::dimensionValue(element.dimension.value),
            "count": element.value.totalCount.long ?? element.value.totalCount.bigDecimal,
        };
        if (exists element.sourceType) {
            rv.source_type = element.sourceType;
        }
        if (exists element.value.typeSpecificValue.followerEdgeAnalyticsValue) {
            rv.organic_count = element.value.typeSpecificValue.followerEdgeAnalyticsValue.organicValue;
            rv.sponsored_count =
                element.value.typeSpecificValue.followerEdgeAnalyticsValue.sponsoredValue;
        }
        if (exists element.value.typeSpecificValue.visitorEdgeAnalyticsValue) {
            hash<auto> v = element.value.typeSpecificValue.visitorEdgeAnalyticsValue;
            if (exists v.desktopCount) {
                rv.desktop_count = v.desktopCount;
            }
            if (exists v.mobileCount) {
                rv.mobile_count = v.mobileCount;
            }
        }
        if (element.timeIntervals.timeRange.typeCode() == NT_HASH) {
            rv.interval_start =
                LinkedInRestClient::linkedin_ms_to_date(element.timeIntervals.timeRange.start);
            rv.interval_end =
                LinkedInRestClient::linkedin_ms_to_date(element.timeIntervals.timeRange.end);
        }
        return rv;
    }

    #! Returns the single meaningful value out of an analytics dimension union
    static *string dimensionValue(auto value) {
        if (value.typeCode() == NT_STRING) {
            return value;
        }
        if (value.typeCode() != NT_HASH) {
            return;
        }
        foreach auto candidate in ((value.urn, value.staffRangeCount, value.relationship,
            value.reaction)) {
            if (candidate.typeCode() == NT_STRING && candidate.val()) {
                return candidate;
            }
        }
    }
}
}
