# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinkedInDataProvider module definition

/** LinkedInPostDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The request fields every post-creation action shares
public const LinkedInPostCommonFields = {
    "commentary": {
        "display_name": "Text",
        "short_desc": "The post's text",
        "desc": "The post's text.\n\n"
            "Mentions and hashtags are written **inline**: `@[Qore Technologies]"
            "(urn:li:organization:2414183)` mentions an organization — the display text must match "
            "the entity's name exactly, case included, or it stays plain text — and `#coding` is a "
            "hashtag.",
        "type": AbstractDataProviderTypeMap."string",
        "required": True,
        "example_value": "Pleased to share what we have been working on.",
    },
    "visibility": {
        "display_name": "Visibility",
        "short_desc": "Who can see the post",
        "desc": "Who can see the post",
        "type": AbstractDataProviderTypeMap."*string",
        "default_value": "PUBLIC",
        "allowed_values": PostVisibilityValues,
    },
    "feed_distribution": {
        "display_name": "Feed Distribution",
        "short_desc": "Where the post is distributed",
        "desc": "Where the post is distributed.  `NONE` creates a post that is reachable only by "
            "its URN and does not appear in any feed",
        "type": AbstractDataProviderTypeMap."*string",
        "default_value": "MAIN_FEED",
        "allowed_values": FeedDistributionValues,
    },
    "disable_resharing": {
        "display_name": "Disable Resharing",
        "short_desc": "Prevent others from resharing the post",
        "desc": "Prevent others from resharing the post",
        "type": AbstractDataProviderTypeMap."*bool",
        "default_value": False,
    },
};

#! Static helpers shared by the post providers
public class LinkedInPostUtil {
    #! Builds the Posts API request body common to every post-creation action
    /** @param req the action request
        @param author the URN of the post's author

        @return the request body, to which the caller adds a \c content member
    */
    static hash<auto> buildPostBody(hash<auto> req, string author) {
        return {
            "author": author,
            "commentary": req.commentary,
            "visibility": req.visibility ?? "PUBLIC",
            "lifecycleState": "PUBLISHED",
            "isReshareDisabledByAuthor": req.disable_resharing ?? False,
            "distribution": {
                "feedDistribution": req.feed_distribution ?? "MAIN_FEED",
                "targetEntities": (),
                "thirdPartyDistributionChannels": (),
            },
        };
    }
}

#! Common base for the post-creation providers
public class LinkedInCreatePostDataProviderBase inherits LinkedInDataProviderBase {
    constructor(LinkedInRestClient::LinkedInRestClientIo rest) : LinkedInDataProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInDataProviderBase(options) {}

    #! Creates a post and returns its URN and public URL
    /** @param body the Posts API request body

        @return the created post hash matching @ref LinkedInCreatedPostDataType
    */
    private hash<auto> createPost(hash<auto> body) {
        hash<auto> resp = doLinkedInRequest("POST", PostsPath, body);
        string urn = getCreatedUrn(resp, "post");
        return {
            "id": urn,
            "url": LinkedInUtil::postUrl(urn),
        };
    }
}

#! Request type for creating a text post
public class LinkedInCreateTextPostRequestDataType inherits HashDataType {
    public {
        const Fields = LinkedInPostCommonFields;
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInCreateTextPostRequestDataType = new LinkedInCreateTextPostRequestDataType();

#! Data provider for creating a text-only post as the authenticated member
public class LinkedInCreateTextPostDataProvider inherits LinkedInCreatePostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-text-post",
            "type": "LinkedInCreateTextPostDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = LinkedInCreateTextPostRequestDataType;
        const ResponseType = LinkedInCreatedPostDataType;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest)
            : LinkedInCreatePostDataProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInCreatePostDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return createPost(LinkedInPostUtil::buildPostBody(req, getMemberUrn()));
    }
}

#! Request type for creating an article post
public class LinkedInCreateArticlePostRequestDataType inherits HashDataType {
    public {
        const Fields = LinkedInPostCommonFields + {
            "article_url": {
                "display_name": "Article URL",
                "short_desc": "The URL the article links to",
                "desc": "The URL the article links to",
                "type": AbstractDataProviderTypeMap."uri",
                "required": True,
                "example_value": "https://qoretechnologies.com/blog/example",
            },
            "article_title": {
                "display_name": "Article Title",
                "short_desc": "The title shown on the article card",
                "desc": "The title shown on the article card.\n\n"
                    "LinkedIn does **not** scrape the URL for a title, so an article posted without "
                    "one shows a card with no heading.",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "How we cut our integration build times in half",
            },
            "article_description": {
                "display_name": "Article Description",
                "short_desc": "The description shown on the article card",
                "desc": "The description shown on the article card; LinkedIn does not scrape the "
                    "URL for one",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "article_thumbnail": {
                "display_name": "Thumbnail Image URN",
                "short_desc": "An uploaded image URN to show as the article's thumbnail",
                "desc": "An uploaded image URN (`urn:li:image:...`) to show as the article's "
                    "thumbnail; upload one with the **Upload Image** action first.  LinkedIn does "
                    "not scrape the URL for an image",
                "type": AbstractDataProviderTypeMap."*string",
                "example_value": "urn:li:image:C4E10AQFoyyAjHPMQuQ",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInCreateArticlePostRequestDataType =
    new LinkedInCreateArticlePostRequestDataType();

#! Data provider for creating a post that links to an article
public class LinkedInCreateArticlePostDataProvider inherits LinkedInCreatePostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-article-post",
            "type": "LinkedInCreateArticlePostDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = LinkedInCreateArticlePostRequestDataType;
        const ResponseType = LinkedInCreatedPostDataType;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest)
            : LinkedInCreatePostDataProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInCreatePostDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto!> article = {
            "source": req.article_url,
        };
        if (req.article_title.val()) {
            article.title = req.article_title;
        }
        if (req.article_description.val()) {
            article.description = req.article_description;
        }
        if (req.article_thumbnail.val()) {
            article.thumbnail = req.article_thumbnail;
        }
        return createPost(LinkedInPostUtil::buildPostBody(req, getMemberUrn()) + {
            "content": {
                "article": article,
            },
        });
    }
}

#! Request type for creating an image post
public class LinkedInCreateImagePostRequestDataType inherits HashDataType {
    public {
        const Fields = LinkedInPostCommonFields + {
            "image": {
                "display_name": "Image",
                "short_desc": "The image to post",
                "desc": "The image to post; LinkedIn accepts JPG, PNG and GIF with fewer than "
                    "36,152,320 pixels",
                "type": FileDataType,
                "required": True,
            },
            "alt_text": {
                "display_name": "Alt Text",
                "short_desc": "The alternative text describing the image",
                "desc": "The alternative text describing the image, used by screen readers; up to "
                    "4,086 characters, and best kept under 120",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "title": {
                "display_name": "Image Title",
                "short_desc": "The title shown with the image",
                "desc": "The title shown with the image",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInCreateImagePostRequestDataType = new LinkedInCreateImagePostRequestDataType();

#! Data provider for creating a post with one image
public class LinkedInCreateImagePostDataProvider inherits LinkedInCreatePostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-image-post",
            "type": "LinkedInCreateImagePostDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = LinkedInCreateImagePostRequestDataType;
        const ResponseType = LinkedInCreatedPostDataType;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest)
            : LinkedInCreatePostDataProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInCreatePostDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string author = getMemberUrn();
        hash<auto> file = req.image;
        hash<auto> asset = uploadImageAsset(LinkedInDataProviderBase::getFileContent(file),
            LinkedInDataProviderBase::getFileContentType(file, MimeTypeJpeg), author);
        hash<auto!> media = {
            "id": asset.id,
        };
        if (req.alt_text.val()) {
            media.altText = req.alt_text;
        }
        if (req.title.val()) {
            media.title = req.title;
        }
        return createPost(LinkedInPostUtil::buildPostBody(req, author) + {
            "content": {
                "media": media,
            },
        });
    }
}

#! Request type for creating a video post
public class LinkedInCreateVideoPostRequestDataType inherits HashDataType {
    public {
        const Fields = LinkedInPostCommonFields + {
            "video": {
                "display_name": "Video",
                "short_desc": "The video to post",
                "desc": "The video to post; LinkedIn accepts MP4 between 75 KB and 500 MB and "
                    "between 3 seconds and 30 minutes long",
                "type": FileDataType,
                "required": True,
            },
            "title": {
                "display_name": "Video Title",
                "short_desc": "The title shown with the video",
                "desc": "The title shown with the video",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInCreateVideoPostRequestDataType = new LinkedInCreateVideoPostRequestDataType();

#! Data provider for creating a post with a video
public class LinkedInCreateVideoPostDataProvider inherits LinkedInCreatePostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-video-post",
            "type": "LinkedInCreateVideoPostDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = LinkedInCreateVideoPostRequestDataType;
        const ResponseType = LinkedInCreatedPostDataType;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest)
            : LinkedInCreatePostDataProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInCreatePostDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string author = getMemberUrn();
        hash<auto> file = req.video;
        hash<auto> asset = uploadVideoAsset(LinkedInDataProviderBase::getFileContent(file),
            LinkedInDataProviderBase::getFileContentType(file, VideoContentType), author);
        hash<auto!> media = {
            "id": asset.id,
        };
        if (req.title.val()) {
            media.title = req.title;
        }
        return createPost(LinkedInPostUtil::buildPostBody(req, author) + {
            "content": {
                "media": media,
            },
        });
    }
}

#! Request type for creating a multi-image post
public class LinkedInCreateMultiImagePostRequestDataType inherits HashDataType {
    public {
        const Fields = LinkedInPostCommonFields + {
            "images": {
                "display_name": "Images",
                "short_desc": "The images to post, in the order they should appear",
                "desc": "The images to post, in the order they should appear; LinkedIn requires "
                    "between " + MinMultiImageCount + " and " + MaxMultiImageCount + " of them",
                "type": new ListDataType("LinkedInMultiImageFiles", FileDataType),
                "required": True,
            },
            "alt_texts": {
                "display_name": "Alt Texts",
                "short_desc": "Alternative text for each image, matched by position",
                "desc": "Alternative text describing each image, used by screen readers and matched "
                    "to `images` **by position**; a shorter list leaves the remaining images "
                    "without alt text",
                "type": new ListDataType("LinkedInMultiImageAltTexts", SoftStringType, True),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInCreateMultiImagePostRequestDataType =
    new LinkedInCreateMultiImagePostRequestDataType();

#! Data provider for creating a post with several images
public class LinkedInCreateMultiImagePostDataProvider inherits LinkedInCreatePostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-multi-image-post",
            "type": "LinkedInCreateMultiImagePostDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = LinkedInCreateMultiImagePostRequestDataType;
        const ResponseType = LinkedInCreatedPostDataType;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest)
            : LinkedInCreatePostDataProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInCreatePostDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        list<auto> files = req.images;
        if (files.size() < MinMultiImageCount || files.size() > MaxMultiImageCount) {
            throw "LINKEDIN-POST-ERROR", sprintf("a multi-image post needs between %d and %d "
                "images; %d supplied.  Use the \"Create Image Post\" action for a single image",
                MinMultiImageCount, MaxMultiImageCount, files.size());
        }
        string author = getMemberUrn();
        *list<auto> alt_texts = req.alt_texts;

        # NOTE: an explicit loop, not "map": map merges hash-valued results into ONE hash rather
        # than building a list of them, so every image would collapse onto the same keys and only
        # the last would be posted
        list<auto> images = ();
        foreach hash<auto> file in (files) {
            hash<auto> asset = uploadImageAsset(LinkedInDataProviderBase::getFileContent(file),
                LinkedInDataProviderBase::getFileContentType(file, MimeTypeJpeg), author);
            hash<auto!> entry = {
                "id": asset.id,
            };
            *string alt_text = alt_texts[$#];
            if (alt_text.val()) {
                entry.altText = alt_text;
            }
            push images, entry;
        }

        return createPost(LinkedInPostUtil::buildPostBody(req, author) + {
            "content": {
                "multiImage": {
                    "images": images,
                },
            },
        });
    }
}

#! Request type for resharing a post
public class LinkedInResharePostRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "post": {
                "display_name": "Post URN",
                "short_desc": "The URN of the post to reshare",
                "desc": "The URN of the post to reshare, e.g. "
                    "`urn:li:share:6957408550713184256`",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "urn:li:share:6957408550713184256",
            },
        } + LinkedInPostCommonFields + {
            "commentary": LinkedInPostCommonFields.commentary + {
                "required": False,
                "desc": "The text to add above the reshared post; a reshare may carry no text of "
                    "its own.\n\n"
                    "Mentions and hashtags are written **inline**: `@[Qore Technologies]"
                    "(urn:li:organization:2414183)` mentions an organization and `#coding` is a "
                    "hashtag.",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInResharePostRequestDataType = new LinkedInResharePostRequestDataType();

#! Data provider for resharing an existing post as the authenticated member
public class LinkedInResharePostDataProvider inherits LinkedInCreatePostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "reshare-post",
            "type": "LinkedInResharePostDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = LinkedInResharePostRequestDataType;
        const ResponseType = LinkedInCreatedPostDataType;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest)
            : LinkedInCreatePostDataProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInCreatePostDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = LinkedInPostUtil::buildPostBody(req, getMemberUrn()) + {
            "reshareContext": {
                "parent": req.post,
            },
        };
        # "commentary" is required by buildPostBody()'s shape but optional for a reshare, and
        # LinkedIn rejects a null one
        if (!req.commentary.val()) {
            body -= "commentary";
        }
        return createPost(body);
    }
}

#! Request type for updating a post
public class LinkedInUpdatePostRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "post": {
                "display_name": "Post URN",
                "short_desc": "The URN of the post to update",
                "desc": "The URN of the post to update",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "urn:li:share:6844785523593134080",
            },
            "commentary": {
                "display_name": "Text",
                "short_desc": "The post's new text",
                "desc": "The post's new text, replacing the existing text.\n\n"
                    "Mentions and hashtags are written **inline**: `@[Qore Technologies]"
                    "(urn:li:organization:2414183)` mentions an organization and `#coding` is a "
                    "hashtag.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "Pleased to share what we have been working on. (edited)",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInUpdatePostRequestDataType = new LinkedInUpdatePostRequestDataType();

#! Data provider for changing the text of an existing post
public class LinkedInUpdatePostDataProvider inherits LinkedInDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-post",
            "type": "LinkedInUpdatePostDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = LinkedInUpdatePostRequestDataType;
        const ResponseType = LinkedInAckDataType;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest) : LinkedInDataProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path = PostsPath + "/" + LinkedInRestClient::linkedin_encode_urn(req.post);
        # a Posts API update is a Rest.li partial update, which is sent as a POST distinguished from
        # a creation only by this header
        doLinkedInRequest("POST", path, {
            "patch": {
                "$set": {
                    "commentary": req.commentary,
                },
            },
        }, {RestliMethodHeader: "PARTIAL_UPDATE"});
        return {
            "success": True,
            "id": req.post,
        };
    }
}

#! Request type for deleting a post
public class LinkedInDeletePostRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "post": {
                "display_name": "Post URN",
                "short_desc": "The URN of the post to delete",
                "desc": "The URN of the post to delete",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "urn:li:share:6844785523593134080",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInDeletePostRequestDataType = new LinkedInDeletePostRequestDataType();

#! Data provider for deleting a post
public class LinkedInDeletePostDataProvider inherits LinkedInDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-post",
            "type": "LinkedInDeletePostDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = LinkedInDeletePostRequestDataType;
        const ResponseType = LinkedInAckDataType;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest) : LinkedInDataProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path = PostsPath + "/" + LinkedInRestClient::linkedin_encode_urn(req.post);
        doLinkedInRequest("DELETE", path, NOTHING, {RestliMethodHeader: "DELETE"});
        return {
            "success": True,
            "id": req.post,
        };
    }
}

#! Request type for retrieving a post
public class LinkedInGetPostRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "post": {
                "display_name": "Post URN",
                "short_desc": "The URN of the post to retrieve",
                "desc": "The URN of the post to retrieve",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "urn:li:share:6844785523593134080",
            },
            "view_context": {
                "display_name": "View Context",
                "short_desc": "Whether to read the post as its author or as a reader",
                "desc": "Whether to read the post as its author or as a reader.  `AUTHOR` returns "
                    "the latest version, which may still be a draft or processing; `READER` returns "
                    "the published version visible to a general audience",
                "type": AbstractDataProviderTypeMap."*string",
                "default_value": "READER",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "READER",
                        "display_name": "Reader",
                        "short_desc": "The published version visible to a general audience",
                        "desc": "The published version visible to a general audience",
                    },
                    <AllowedValueInfo>{
                        "value": "AUTHOR",
                        "display_name": "Author",
                        "short_desc": "The latest version, including drafts and processing posts",
                        "desc": "The latest version, which may be `DRAFT` or `PROCESSING` rather "
                            "than published",
                    },
                ),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const LinkedInGetPostRequestDataType = new LinkedInGetPostRequestDataType();

#! Data provider for retrieving one post by URN
public class LinkedInGetPostDataProvider inherits LinkedInDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-post",
            "type": "LinkedInGetPostDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = LinkedInGetPostRequestDataType;
        const ResponseType = LinkedInPostDataType;
    }

    constructor(LinkedInRestClient::LinkedInRestClientIo rest) : LinkedInDataProviderBase(rest) {}
    constructor(*hash<auto> options) : LinkedInDataProviderBase(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path = sprintf("%s/%s?viewContext=%s", PostsPath,
            LinkedInRestClient::linkedin_encode_urn(req.post),
            encode_url(req.view_context ?? "READER", True));
        hash<auto> resp = doLinkedInRequest("GET", path);
        if (resp.body.typeCode() != NT_HASH) {
            throw "LINKEDIN-RESPONSE-ERROR", sprintf("LinkedIn returned a %s rather than a post for "
                "%y", resp.body.type(), req.post);
        }
        return LinkedInUtil::normalizePost(resp.body);
    }
}
}
