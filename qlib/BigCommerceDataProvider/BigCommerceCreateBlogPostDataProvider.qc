# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceCreateBlogPostDataProvider.qc BigCommerce create blog post action data provider

/*  BigCommerceCreateBlogPostDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a blog post
public class BigCommerceCreateBlogPostRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "title": {
                "display_name": "Title",
                "type": StringType,
                "desc": "Title of the blog post",
                "required": True,
                "example_value": "My Blog Post",
            },
            "body": {
                "display_name": "Body",
                "type": StringType,
                "desc": "Body content of the blog post (HTML)",
                "required": True,
            },
            "url": {
                "display_name": "URL",
                "type": StringOrNothingType,
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
                "type": StringOrNothingType,
                "desc": "Author name of the blog post",
            },
            "thumbnail_path": {
                "display_name": "Thumbnail Path",
                "type": StringOrNothingType,
                "desc": "Path to the thumbnail image",
            },
            "meta_description": {
                "display_name": "Meta Description",
                "type": StringOrNothingType,
                "desc": "SEO meta description",
            },
            "meta_keywords": {
                "display_name": "Meta Keywords",
                "type": StringOrNothingType,
                "desc": "SEO meta keywords",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! BigCommerce create blog post action data provider
public class BigCommerceCreateBlogPostDataProvider inherits BigCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new blog post in BigCommerce",
            "type": "BigCommerceCreateBlogPostDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(BigCommerceRestClient::BigCommerceRestClient rest) : BigCommerceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "create";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new blog post in BigCommerce";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BigCommerceCreateBlogPostRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BigCommerceBlogPostDataType();
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # Convert date to RFC 2822 format for V2 API
        if (req.published_date) {
            req.published_date = formatDateTimeV2(req.published_date);
        }
        return doPostV2("blog/posts", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BigCommerceDataProvider
