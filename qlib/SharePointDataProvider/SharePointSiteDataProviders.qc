# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SharePointDataProvider module definition

/** SharePointSiteDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SharePointDataProvider module
public namespace SharePointDataProvider {
#! Fields selected when reading sites
const SiteSelect = "$select=id,name,displayName,description,webUrl,createdDateTime,lastModifiedDateTime";

#! Search options for listing sites
public const SharePointSitesSearchOptions = {
    "search": <DataProviderOptionInfo>{
        "display_name": "Search",
        "short_desc": "Search term to match sites against",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "A term to search site titles and URLs for. When omitted, the tenant's root site and "
            "its subsites are returned, which is always available; a search is answered from the "
            "SharePoint search index, which returns nothing on a tenant whose sites have not been "
            "indexed",
        "preselected": True,
    },
};

#! Data provider for listing and searching sites (DPAT_FIND)
/** The TypeScript app this replaces listed sites with \c "/sites?search=*" alone.  That query is
    answered from the SharePoint search index, and on a tenant whose sites are not indexed it
    returns an empty list - leaving the site picker empty and every action unusable.  Listing the
    root site and its subsites always works, so it is the default here and a search is opt-in.
*/
public class SharePointSitesDataProvider inherits SharePointDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "sites",
            "type": "SharePointSitesDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_read": True,
            "has_record": True,
            "supports_native_search": True,
            "search_options": SharePointSitesSearchOptions,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RecordType = SharePointSiteDataType;
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointDataProviderBase(rest, options) {
    }

    constructor(*hash<auto> options) : SharePointDataProviderBase(options) {
    }

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return RecordType.getFields();
    }

    private *AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return new DefaultRecordIterator<hash<auto>>(
            new ListIterator<hash<auto>>(getSites(search_options.search)), where_cond);
    }

    #! Returns the sites matching the given search term, or the root site and its subsites
    /** @param search the search term, if any

        @return the matching sites
    */
    private list<hash<auto>> getSites(*string search) {
        if (search.val()) {
            # the term has to be URL-encoded: an unencoded space makes Graph answer with an HTML
            # error page, which then fails to deserialize rather than reporting anything useful
            return getAllPages("sites?search=" + encode_url(search, True) + "&" + SiteSelect, MaxPageSize);
        }
        list<hash<auto>> rv = ();
        rv += doRestCommand("GET", "sites/root?" + SiteSelect).body;
        rv += getAllPages("sites/root/sites?" + SiteSelect, MaxPageSize);
        return rv;
    }
}

#! Request type for getting a site
public class SharePointGetSiteRequestDataType inherits HashDataType {
    public {
        const Fields = SiteIdField;
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Request type object for getting a site
public const SharePointGetSiteRequestDataType = new SharePointGetSiteRequestDataType();

#! Data provider for getting a site by ID
public class SharePointGetSiteDataProvider inherits SharePointDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-site",
            "type": "SharePointGetSiteDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = SharePointGetSiteRequestDataType;
        const ResponseType = SharePointSiteDataType;
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointDataProviderBase(rest, options) {
    }

    constructor(*hash<auto> options) : SharePointDataProviderBase(options) {
    }

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("GET", getSiteBasePath(req)).body;
    }
}

#! Search options for listing document libraries
public const SharePointDrivesSearchOptions = SiteIdSearchOption;

#! Data provider for listing the document libraries in a site (DPAT_FIND)
public class SharePointDrivesDataProvider inherits SharePointDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "drives",
            "type": "SharePointDrivesDataProvider",
            "constructor_options": SharePointDataProviderBase::ConstructorOptions,
            "supports_read": True,
            "has_record": True,
            "supports_native_search": True,
            "search_options": SharePointDrivesSearchOptions,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RecordType = SharePointDriveDataType;
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SharePointDataProviderBase(rest, options) {
    }

    constructor(*hash<auto> options) : SharePointDataProviderBase(options) {
    }

    string getName() { return ProviderInfo.name; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return RecordType.getFields();
    }

    private *AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        string path = getSiteBasePath(search_options) + "/drives?$select=id,name,description,driveType,"
            "webUrl,createdDateTime,lastModifiedDateTime&$top=" + MaxPageSize;
        return new DefaultRecordIterator<hash<auto>>(
            new ListIterator<hash<auto>>(getAllPages(path)), where_cond);
    }
}
}
