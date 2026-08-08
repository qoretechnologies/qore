# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinkedInDataProvider module definition

/** LinkedInDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the LinkedInDataProvider module
public namespace LinkedInDataProvider {
#! Static helpers shared by the LinkedIn providers
public class LinkedInUtil {
    #! Converts a LinkedIn timestamp to a %Qore date
    /** LinkedIn reports every timestamp as **milliseconds** since the epoch — \c publishedAt,
        \c createdAt, \c lastModifiedAt and \c downloadUrlExpiresAt are all 13-digit values.  Passing
        that integer through, as the TypeScript application this module replaces did, leaves every
        consumer to divide by 1000 and hope it guessed the unit right.

        @param ms milliseconds since 1970-01-01 UTC, or @ref nothing

        @return the corresponding absolute date in UTC, or @ref nothing if \a ms was not set
    */
    static *date msToDate(auto ms) {
        return LinkedInRestClient::linkedin_ms_to_date(ms);
    }

    #! Converts a %Qore date to a LinkedIn timestamp
    /** The inverse of @ref msToDate(); LinkedIn takes milliseconds since the epoch wherever it
        accepts a timestamp.

        @param d the date, or @ref nothing

        @return milliseconds since 1970-01-01 UTC, or @ref nothing if \a d was not set
    */
    static *int dateToMs(*date d) {
        return LinkedInRestClient::linkedin_date_to_ms(d);
    }

    #! Returns the public LinkedIn URL of a post
    /** @param urn the post's URN, e.g. \c "urn:li:share:6844785523593134080"

        @return the URL a browser can open the post at, or @ref nothing if \a urn is not a URN this
        can be built from
    */
    static *string postUrl(*string urn) {
        if (!urn.val() || urn !~ /^urn:li:/) {
            return;
        }
        return "https://www.linkedin.com/feed/update/" + urn + "/";
    }

    #! Normalizes a LinkedIn Post resource into this module's post hash
    /** LinkedIn's wire form is camel-cased and carries its timestamps as millisecond integers; this
        converts both, and flattens the content union into the three shapes a member post can
        actually have.

        @param post the Post resource as returned by the Posts API

        @return the normalized post hash matching @ref LinkedInPostDataType
    */
    static hash<auto> normalizePost(hash<auto> post) {
        hash<auto!> rv = {
            "id": post.id,
            "url": LinkedInUtil::postUrl(post.id),
            "author": post.author,
            "commentary": post.commentary,
            "visibility": post.visibility,
            "lifecycle_state": post.lifecycleState,
            "is_reshare_disabled_by_author": post.isReshareDisabledByAuthor,
            "is_edited_by_author": post.lifecycleStateInfo.isEditedByAuthor,
            "feed_distribution": post.distribution.feedDistribution,
            "third_party_distribution_channels": post.distribution.thirdPartyDistributionChannels,
            "content_call_to_action_label": post.contentCallToActionLabel,
            "content_landing_page": post.contentLandingPage,
            "published_at": LinkedInUtil::msToDate(post.publishedAt),
            "created_at": LinkedInUtil::msToDate(post.createdAt),
            "last_modified_at": LinkedInUtil::msToDate(post.lastModifiedAt),
        };
        if (post.reshareContext.typeCode() == NT_HASH) {
            rv.reshare_parent = post.reshareContext.parent;
            rv.reshare_root = post.reshareContext.root;
        }
        if (post.content.typeCode() == NT_HASH) {
            rv.content = LinkedInUtil::normalizePostContent(post.content);
        }
        return rv;
    }

    #! Normalizes the content union of a Post resource
    static hash<auto> normalizePostContent(hash<auto> content) {
        hash<auto> rv = {};
        if (content.article.typeCode() == NT_HASH) {
            rv.article = {
                "source": content.article.source,
                "title": content.article.title,
                "description": content.article.description,
                "thumbnail": content.article.thumbnail,
            };
        }
        if (content.media.typeCode() == NT_HASH) {
            rv.media = {
                "id": content.media.id,
                "title": content.media.title,
                "alt_text": content.media.altText,
            };
        }
        if (content.multiImage.images.typeCode() == NT_LIST) {
            list<auto> images = ();
            foreach hash<auto> image in (content.multiImage.images) {
                push images, {
                    "id": image.id,
                    "alt_text": image.altText,
                };
            }
            rv.multi_image = images;
        }
        return rv;
    }

    #! Normalizes an Images API or Videos API asset resource
    static hash<auto> normalizeMediaAsset(hash<auto> asset) {
        hash<auto!> rv = {
            "id": asset.id,
            "owner": asset.owner,
            "status": asset.status,
            "download_url": asset.downloadUrl,
            "download_url_expires_at": LinkedInUtil::msToDate(asset.downloadUrlExpiresAt),
        };
        if (exists asset.processingFailureReason) {
            rv.processing_failure_reason = asset.processingFailureReason;
        }
        if (exists asset.duration) {
            rv.duration_ms = asset.duration;
        }
        if (exists asset.thumbnail) {
            rv.thumbnail = asset.thumbnail;
        }
        if (exists asset.aspectRatioWidth) {
            rv.aspect_ratio_width = asset.aspectRatioWidth;
        }
        if (exists asset.aspectRatioHeight) {
            rv.aspect_ratio_height = asset.aspectRatioHeight;
        }
        return rv;
    }
}

#! The article content of a post
public class LinkedInPostArticleDataType inherits HashDataType {
    private {
        const Fields = {
            "source": {
                "display_name": "Article URL",
                "short_desc": "The URL the article links to",
                "desc": "The URL the article links to",
                "type": AbstractDataProviderTypeMap."*uri",
            },
            "title": {
                "display_name": "Title",
                "short_desc": "The article's title as shown in the post",
                "desc": "The article's title as shown in the post",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "description": {
                "display_name": "Description",
                "short_desc": "The article's description as shown in the post",
                "desc": "The article's description as shown in the post",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "thumbnail": {
                "display_name": "Thumbnail Image URN",
                "short_desc": "The image URN used as the article's thumbnail",
                "desc": "The image URN used as the article's thumbnail",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInPostArticleDataType = new LinkedInPostArticleDataType();

#! The single-media content of a post
public class LinkedInPostMediaDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "Media URN",
                "short_desc": "The image or video URN",
                "desc": "The image (`urn:li:image:...`) or video (`urn:li:video:...`) URN",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "title": {
                "display_name": "Title",
                "short_desc": "The media's title",
                "desc": "The media's title",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "alt_text": {
                "display_name": "Alt Text",
                "short_desc": "The alternative text for screen readers",
                "desc": "The alternative text for screen readers",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInPostMediaDataType = new LinkedInPostMediaDataType();

#! One image of a multi-image post
public class LinkedInPostMultiImageEntryDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "Image URN",
                "short_desc": "The image URN",
                "desc": "The image URN (`urn:li:image:...`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "alt_text": {
                "display_name": "Alt Text",
                "short_desc": "The alternative text for screen readers",
                "desc": "The alternative text for screen readers",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInPostMultiImageEntryDataType = new LinkedInPostMultiImageEntryDataType();

#! The content of a post
public class LinkedInPostContentDataType inherits HashDataType {
    private {
        const Fields = {
            "article": {
                "display_name": "Article",
                "short_desc": "The linked article, for an article post",
                "desc": "The linked article, for an article post",
                "type": LinkedInPostArticleDataType,
            },
            "media": {
                "display_name": "Media",
                "short_desc": "The image or video, for a single-media post",
                "desc": "The image or video, for a single-media post",
                "type": LinkedInPostMediaDataType,
            },
            "multi_image": {
                "display_name": "Images",
                "short_desc": "The images, for a multi-image post",
                "desc": "The images, for a multi-image post",
                "type": new ListDataType("LinkedInPostMultiImages",
                    LinkedInPostMultiImageEntryDataType, True),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInPostContentDataType = new LinkedInPostContentDataType();

#! A LinkedIn post
public class LinkedInPostDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "Post URN",
                "short_desc": "The post's URN",
                "desc": "The post's URN, e.g. `urn:li:share:6844785523593134080`; this is the value "
                    "every other post action takes",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "url": {
                "display_name": "Post URL",
                "short_desc": "The public URL of the post",
                "desc": "The URL a browser can open the post at",
                "type": AbstractDataProviderTypeMap."*uri",
            },
            "author": {
                "display_name": "Author URN",
                "short_desc": "The URN of the member or organization that authored the post",
                "desc": "The URN of the member or organization that authored the post",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "commentary": {
                "display_name": "Commentary",
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
                "desc": "The post's attached content, if any",
                "type": LinkedInPostContentDataType,
            },
            "feed_distribution": {
                "display_name": "Feed Distribution",
                "short_desc": "Where the post is distributed",
                "desc": "Where the post is distributed: `MAIN_FEED` or `NONE`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "third_party_distribution_channels": {
                "display_name": "Third-Party Distribution Channels",
                "short_desc": "Third-party channels the post is distributed to",
                "desc": "Third-party channels the post is distributed to",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "is_reshare_disabled_by_author": {
                "display_name": "Reshare Disabled",
                "short_desc": "True if the author disabled resharing",
                "desc": "True if the author disabled resharing",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "is_edited_by_author": {
                "display_name": "Edited By Author",
                "short_desc": "True if the author has edited the post since publishing it",
                "desc": "True if the author has edited the post since publishing it",
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
                "desc": "When the post was published; LinkedIn reports this as milliseconds since "
                    "the epoch and it is converted to a date here",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "created_at": {
                "display_name": "Created",
                "short_desc": "When the post was created",
                "desc": "When the post was created",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "last_modified_at": {
                "display_name": "Last Modified",
                "short_desc": "When the post was last modified",
                "desc": "When the post was last modified",
                "type": AbstractDataProviderTypeMap."*date",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInPostDataType = new LinkedInPostDataType();

#! The result of creating a post
public class LinkedInCreatedPostDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "Post URN",
                "short_desc": "The created post's URN",
                "desc": "The created post's URN, e.g. `urn:li:share:6844785523593134080`.\n\n"
                    "LinkedIn answers a post creation with `201` and an **empty body**, returning "
                    "the URN in the `x-restli-id` response header only; it is read from there.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "url": {
                "display_name": "Post URL",
                "short_desc": "The public URL of the created post",
                "desc": "The URL a browser can open the created post at",
                "type": AbstractDataProviderTypeMap."*uri",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInCreatedPostDataType = new LinkedInCreatedPostDataType();

#! A LinkedIn media asset (image or video)
public class LinkedInMediaAssetDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "Media URN",
                "short_desc": "The asset's URN",
                "desc": "The asset's URN (`urn:li:image:...` or `urn:li:video:...`); this is the "
                    "value a post's content takes",
                "type": AbstractDataProviderTypeMap."string",
            },
            "owner": {
                "display_name": "Owner URN",
                "short_desc": "The URN of the entity that owns the asset",
                "desc": "The URN of the entity that owns the asset",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "status": {
                "display_name": "Status",
                "short_desc": "The asset's processing status",
                "desc": "The asset's processing status: `WAITING_UPLOAD`, `PROCESSING`, "
                    "`AVAILABLE` or `PROCESSING_FAILED`.\n\n"
                    "An asset can be attached to a post while it is still `PROCESSING`; LinkedIn "
                    "publishes the post once processing completes.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "download_url": {
                "display_name": "Download URL",
                "short_desc": "A URL the asset can be downloaded from",
                "desc": "A URL the asset can be downloaded from; only present once the asset is "
                    "available",
                "type": AbstractDataProviderTypeMap."*uri",
            },
            "download_url_expires_at": {
                "display_name": "Download URL Expiry",
                "short_desc": "When the download URL stops working",
                "desc": "When the download URL stops working",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "processing_failure_reason": {
                "display_name": "Processing Failure Reason",
                "short_desc": "Why processing failed",
                "desc": "Why processing failed; only present when the status is `PROCESSING_FAILED`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "duration_ms": {
                "display_name": "Duration (ms)",
                "short_desc": "The video's length in milliseconds",
                "desc": "The video's length in milliseconds; videos only",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "thumbnail": {
                "display_name": "Thumbnail URL",
                "short_desc": "A URL of the video's thumbnail",
                "desc": "A URL of the video's thumbnail; videos only",
                "type": AbstractDataProviderTypeMap."*uri",
            },
            "aspect_ratio_width": {
                "display_name": "Aspect Ratio Width",
                "short_desc": "The aspect ratio width (16 for 16:9)",
                "desc": "The aspect ratio width (16 for 16:9)",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "aspect_ratio_height": {
                "display_name": "Aspect Ratio Height",
                "short_desc": "The aspect ratio height (9 for 16:9)",
                "desc": "The aspect ratio height (9 for 16:9)",
                "type": AbstractDataProviderTypeMap."*float",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInMediaAssetDataType = new LinkedInMediaAssetDataType();

#! The authenticated member's profile, from the OpenID Connect userinfo endpoint
public class LinkedInUserInfoDataType inherits HashDataType {
    private {
        const Fields = {
            "urn": {
                "display_name": "Member URN",
                "short_desc": "The member's URN",
                "desc": "The member's URN in `urn:li:person:{id}` form, which is what LinkedIn APIs "
                    "take as an author or owner; it is built from `sub`",
                "type": AbstractDataProviderTypeMap."string",
            },
            "sub": {
                "display_name": "Subject Identifier",
                "short_desc": "The member's stable OpenID Connect subject identifier",
                "desc": "The member's stable OpenID Connect subject identifier",
                "type": AbstractDataProviderTypeMap."string",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The member's full name",
                "desc": "The member's full name",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "given_name": {
                "display_name": "Given Name",
                "short_desc": "The member's given name",
                "desc": "The member's given name",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "family_name": {
                "display_name": "Family Name",
                "short_desc": "The member's family name",
                "desc": "The member's family name",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "email": {
                "display_name": "Email",
                "short_desc": "The member's primary email address",
                "desc": "The member's primary email address; requires the `email` scope",
                "type": AbstractDataProviderTypeMap."*email",
            },
            "email_verified": {
                "display_name": "Email Verified",
                "short_desc": "True if LinkedIn has verified the email address",
                "desc": "True if LinkedIn has verified the email address",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "picture": {
                "display_name": "Picture URL",
                "short_desc": "A URL of the member's profile picture",
                "desc": "A URL of the member's profile picture",
                "type": AbstractDataProviderTypeMap."*uri",
            },
            "locale": {
                "display_name": "Locale",
                "short_desc": "The member's locale",
                "desc": "The member's locale, e.g. `en-US`",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInUserInfoDataType = new LinkedInUserInfoDataType();

#! The result of an action that has no response body
public class LinkedInAckDataType inherits HashDataType {
    private {
        const Fields = {
            "success": {
                "display_name": "Success",
                "short_desc": "Always True; the action throws on failure",
                "desc": "Always `True`; the action throws an exception if the operation failed",
                "type": AbstractDataProviderTypeMap."bool",
            },
            "id": {
                "display_name": "Post URN",
                "short_desc": "The URN the action was applied to",
                "desc": "The URN the action was applied to",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInAckDataType = new LinkedInAckDataType();
}
