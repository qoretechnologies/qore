# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file QlipBuildsDataProvider.qc Qlip builds collection data provider

/*  QlipBuildsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the QlipDataProvider module
public namespace QlipDataProvider {

#! Qlip builds collection data provider
public class QlipBuildsDataProvider inherits QlipDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "builds",
            "desc": "Qlip builds data provider for listing and searching builds",
            "type": "QlipBuildsDataProvider",
            "supports_read": True,
            "has_record": True,
            "supports_native_search": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "buildId": <DataProviderOptionInfo>{
                "display_name": "Build ID",
                "short_desc": "Fetch a single build by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Get one build by its ID (e.g. `20260730-105410`); when set, the "
                    "other filters are ignored",
            },
            "project": <DataProviderOptionInfo>{
                "display_name": "Project",
                "short_desc": "Filter by project name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Restrict the listing to a single project by name; when omitted, builds "
                    "are listed across every project the token can see",
            },
            "branch": <DataProviderOptionInfo>{
                "display_name": "Branch",
                "short_desc": "Filter by git branch",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter builds by the git branch they were captured from",
            },
            "status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by build status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter builds by status",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "pending", "desc": "Builds awaiting review"},
                    <AllowedValueInfo>{"value": "approved", "desc": "Fully approved builds"},
                    <AllowedValueInfo>{"value": "rejected", "desc": "Builds with rejections"},
                    <AllowedValueInfo>{"value": "mixed",
                        "desc": "Builds with a mix of accepted and rejected snapshots"},
                    <AllowedValueInfo>{"value": "uploading",
                        "desc": "Builds still transferring blobs"},
                ),
            },
            "startDate": <DataProviderOptionInfo>{
                "display_name": "Start Date",
                "short_desc": "Only builds created on/after this date",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Only return builds created on or after this ISO 8601 date/time "
                    "(e.g. `2026-02-01T00:00:00Z`)",
            },
            "endDate": <DataProviderOptionInfo>{
                "display_name": "End Date",
                "short_desc": "Only builds created on/before this date",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Only return builds created on or before this ISO 8601 date/time "
                    "(e.g. `2026-02-28T23:59:59Z`)",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of builds to return",
            },
        };
    }

    #! Creates the object with a REST client
    constructor(QlipRestClient::QlipRestClient rest) : QlipDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "builds";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Qlip builds data provider for listing and searching builds";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new QlipBuildDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Single build by ID → GET /api/builds/:buildId (returns {build, snapshots})
        if (search_options.buildId) {
            hash<auto> response = doGet("builds/" + encode_url(search_options.buildId, True));
            list<hash<auto>> data = response.build ? (response.build,) : ();
            return new DefaultRecordIterator<hash<auto>>(new ListHashIterator(data),
                where_cond, search_options);
        }

        # Build query params for the listing
        hash<auto> params;
        if (search_options.project) {
            params.project = search_options.project;
        }
        if (search_options.branch) {
            params.branch = search_options.branch;
        }
        if (search_options.status) {
            params.status = search_options.status;
        }
        if (search_options.startDate) {
            params.startDate = search_options.startDate;
        }
        if (search_options.endDate) {
            params.endDate = search_options.endDate;
        }

        list<hash<auto>> all_records = fetchAllPages("builds", "builds", params,
            search_options.limit);
        return new DefaultRecordIterator<hash<auto>>(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace QlipDataProvider
