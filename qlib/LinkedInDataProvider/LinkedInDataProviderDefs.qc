# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinkedInDataProvider module definition

/** LinkedInDataProviderDefs.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! LinkedIn app name
public const AppName = "LinkedIn";

#! Logo file
/** Read as a separate file rather than inlined so that the doc build and the app catalog share one
    source of truth for the image
*/
const LinkedInLogo = File::readTextFile(get_script_dir() + "/linkedin-logo.svg");

#! The path of the Posts API
public const PostsPath = LinkedInRestClient::LinkedInRestPrefix + "/posts";

#! The path of the Images API
public const ImagesPath = LinkedInRestClient::LinkedInRestPrefix + "/images";

#! The path of the Videos API
public const VideosPath = LinkedInRestClient::LinkedInRestPrefix + "/videos";

#! The path of the OpenID Connect userinfo endpoint
/** Unversioned: it takes neither a \c LinkedIn-Version nor an \c X-Restli-Protocol-Version header
*/
public const UserInfoPath = LinkedInRestClient::LinkedInV2Prefix + "/userinfo";

#! The header carrying the ID of an entity a versioned write created
/** A LinkedIn write returns \c 201 with an **empty body**; the created entity's URN is in this
    response header and nowhere else.
*/
public const RestliIdHeader = "x-restli-id";

#! The header naming the Rest.li method for requests whose method is ambiguous
public const RestliMethodHeader = "X-RestLi-Method";

#! The part size LinkedIn's Videos API divides an upload into
/** LinkedIn's \c uploadInstructions always describe 4 MiB parts (\c "0 - 4194303",
    \c "4194304 - 8388607", ...), and the byte ranges are authoritative — this constant only sizes
    the sanity check that a returned instruction is one of them.
*/
public const VideoUploadPartSize = 4 * 1024 * 1024;

#! The content type declared for a video upload when the file value does not name one
/** LinkedIn accepts MP4 only, so this is both the default and the only useful value
*/
public const VideoContentType = "video/mp4";

#! Maximum number of images LinkedIn accepts in a multi-image post
public const MaxMultiImageCount = 20;

#! Minimum number of images LinkedIn requires in a multi-image post
public const MinMultiImageCount = 2;

#! The visibility values a member post can be created with
public const PostVisibilityValues = (
    <AllowedValueInfo>{
        "value": "PUBLIC",
        "display_name": "Public",
        "short_desc": "Visible to anyone on or off LinkedIn",
        "desc": "Visible to anyone on or off LinkedIn",
    },
    <AllowedValueInfo>{
        "value": "CONNECTIONS",
        "display_name": "Connections Only",
        "short_desc": "Visible only to the member's connections",
        "desc": "Visible only to the authenticated member's connections",
    },
);

#! The feed distribution values a member post can be created with
public const FeedDistributionValues = (
    <AllowedValueInfo>{
        "value": "MAIN_FEED",
        "display_name": "Main Feed",
        "short_desc": "Distributed to the main LinkedIn feed",
        "desc": "The post is distributed to the main LinkedIn feed",
    },
    <AllowedValueInfo>{
        "value": "NONE",
        "display_name": "None",
        "short_desc": "Not distributed to any feed",
        "desc": "The post is not distributed to any feed; it is reachable only by its URN",
    },
);
}
