# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceBlogPostsDataProvider.qc BigCommerce blog posts collection data provider

/*  BigCommerceBlogPostsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! BigCommerce blog posts collection data provider (V2 API)
public class BigCommerceBlogPostsDataProvider inherits BigCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "blog-posts",
            "desc": "BigCommerce blog posts data provider for listing and searching blog posts",
            "type": "BigCommerceBlogPostsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "is_published": <DataProviderOptionInfo>{
                "display_name": "Is Published",
                "short_desc": "Filter by published status",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "Filter blog posts by published status",
            },
            "tag": <DataProviderOptionInfo>{
                "display_name": "Tag",
                "short_desc": "Filter by tag",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter blog posts by tag",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results to return",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
            "page": <DataProviderOptionInfo>{
                "display_name": "Page",
                "short_desc": "Page number for pagination",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Page number for paginated results",
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("BigCommerceCreateBlogPostDataProvider"),
            "delete": Class::forName("BigCommerceDeleteBlogPostDataProvider"),
            "get": Class::forName("BigCommerceGetBlogPostDataProvider"),
            "update": Class::forName("BigCommerceUpdateBlogPostDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(BigCommerceRestClient::BigCommerceRestClient rest)
            : BigCommerceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "blog-posts";
    }

    #! Returns the provider description
    *string getDesc() {
        return "BigCommerce blog posts data provider for listing and searching blog posts";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new BigCommerceBlogPostDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Build query params for V2 Blog Posts API
        hash<auto> params;

        if (exists search_options.is_published) {
            params.is_published = search_options.is_published ? "true" : "false";
        }
        if (search_options.tag) {
            params.tag = search_options.tag;
        }
        if (search_options.page) {
            params.page = search_options.page;
        }

        list<hash<auto>> all_records = fetchAllPagesV2("blog/posts", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BigCommerceDataProvider
