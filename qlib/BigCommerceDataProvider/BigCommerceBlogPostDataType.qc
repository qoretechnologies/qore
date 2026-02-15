# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceBlogPostDataType.qc defines the BigCommerceBlogPostDataType class

/*  BigCommerceBlogPostDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BigCommerceDataProvider module
public namespace BigCommerceDataProvider {

#! Data type for BigCommerce blog post records
public class BigCommerceBlogPostDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "type": IntType,
                "desc": "Unique numeric ID of the blog post",
            },
            "title": {
                "display_name": "Title",
                "type": StringType,
                "desc": "Title of the blog post",
            },
            "body": {
                "display_name": "Body",
                "type": StringType,
                "desc": "Body content of the blog post (HTML)",
            },
            "url": {
                "display_name": "URL",
                "type": SoftStringOrNothingType,
                "desc": "URL slug for the blog post",
            },
            "tags": {
                "display_name": "Tags",
                "type": AutoListOrNothingType,
                "desc": "Array of tag strings for categorization",
            },
            "is_published": {
                "display_name": "Is Published",
                "type": SoftBoolOrNothingType,
                "desc": "Whether the blog post is published and visible",
            },
            "published_date": {
                "display_name": "Published Date",
                "type": SoftDateOrNothingType,
                "desc": "Date/time the blog post was published",
            },
            "author": {
                "display_name": "Author",
                "type": SoftStringOrNothingType,
                "desc": "Author name of the blog post",
            },
            "thumbnail_path": {
                "display_name": "Thumbnail Path",
                "type": SoftStringOrNothingType,
                "desc": "Path to the thumbnail image",
            },
            "meta_description": {
                "display_name": "Meta Description",
                "type": SoftStringOrNothingType,
                "desc": "SEO meta description",
            },
            "meta_keywords": {
                "display_name": "Meta Keywords",
                "type": SoftStringOrNothingType,
                "desc": "SEO meta keywords",
            },
            "summary": {
                "display_name": "Summary",
                "type": SoftStringOrNothingType,
                "desc": "Auto-generated summary (read-only)",
            },
            "preview_url": {
                "display_name": "Preview URL",
                "type": SoftStringOrNothingType,
                "desc": "Preview URL of the blog post (read-only)",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("BigCommerceBlogPost") {
        addQoreFields(Fields);
    }
}

} # namespace BigCommerceDataProvider
