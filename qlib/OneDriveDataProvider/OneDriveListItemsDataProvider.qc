# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OneDriveDataProvider module definition

/** OneDriveListItemsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the OneDriveDataProvider module
public namespace OneDriveDataProvider {
#! Search options for listing items
public const OneDriveListItemsSearchOptions = {
    "folder_id": <DataProviderOptionInfo>{
        "display_name": "Folder ID",
        "short_desc": "The folder to list items from",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "The ID of the folder to list items from. If omitted, lists items from the drive root",
        "preselected": True,
    },
    "search_query": <DataProviderOptionInfo>{
        "display_name": "Search Query",
        "short_desc": "Search query string",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "A search query string to filter items by name or content. When provided, searches "
            "across the entire drive",
        "preselected": True,
    },
    "top": <DataProviderOptionInfo>{
        "display_name": "Top",
        "short_desc": "Maximum items to return per page",
        "type": AbstractDataProviderTypeMap."*int",
        "desc": "Maximum number of items to return per page (default `200`, max `1000`)",
        "preselected": True,
    },
    "orderby": <DataProviderOptionInfo>{
        "display_name": "Order By",
        "short_desc": "Sort field and direction",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "Sort order for results (e.g., `name asc`, `size desc`, `lastModifiedDateTime desc`)",
    },
};

#! Data provider for listing/searching items (DPAT_FIND)
public class OneDriveListItemsDataProvider inherits OneDriveDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "items",
            "type": "OneDriveListItemsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_read": True,
            "has_record": True,
            "supports_native_search": True,
            "search_options": OneDriveListItemsSearchOptions,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    constructor(RestClientIo::RestClientIo rest) : OneDriveDataProviderBase(rest) {}
    constructor(*hash<auto> options) : OneDriveDataProviderBase(options) {}
    string getName() { return ProviderInfo.name; }

    private *AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        string path;
        if (search_options.search_query) {
            # Search across the drive
            path = "me/drive/root/search(q='" + encode_url(search_options.search_query, True) + "')";
        } else if (search_options.folder_id) {
            path = "me/drive/items/" + search_options.folder_id + "/children";
        } else {
            path = "me/drive/root/children";
        }

        # Add query parameters
        list<string> params = ();
        if (search_options.top) {
            params += "$top=" + string(search_options.top);
        }
        if (search_options.orderby) {
            params += "$orderby=" + encode_url(search_options.orderby, True);
        }
        if (params) {
            path += (path =~ /\?/ ? "&" : "?") + params.join("&");
        }

        # Fetch all pages
        list<hash<auto>> all_items = ();
        while (True) {
            hash<auto> response = doRestCommand("GET", path);
            if (response.body.value) {
                all_items += response.body.value;
            }
            # Follow @odata.nextLink for pagination
            if (*string next_link = response.body."@odata.nextLink") {
                # nextLink is a full URL; extract the path after the base URL
                # The RestClientIo already has the base URL set, so we need the relative path
                next_link =~ s/^https:\/\/graph\.microsoft\.com\/v1\.0\///;
                path = next_link;
            } else {
                break;
            }
        }

        return new DefaultRecordIterator(new ListIterator(all_items));
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return OneDriveDriveItemDataType.getFields();
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
}
