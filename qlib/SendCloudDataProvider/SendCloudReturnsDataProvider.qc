# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudReturnsDataProvider.qc SendCloud returns collection data provider

/*  SendCloudReturnsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SendCloudDataProvider module
public namespace SendCloudDataProvider {

#! Collection provider for SendCloud returns
public class SendCloudReturnsDataProvider inherits SendCloudDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "returns",
            "desc": "List and search SendCloud returns",
            "type": "SendCloudReturnsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Return ID",
                "short_desc": "Retrieve a single return by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "If supplied, fetches a single return with this numeric ID instead of listing",
                "ref_data": "returns",
            },
            "cursor": <DataProviderOptionInfo>{
                "display_name": "Cursor",
                "short_desc": "Pagination cursor",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Opaque pagination cursor returned by a previous page",
            },
            "page_size": <DataProviderOptionInfo>{
                "display_name": "Page Size",
                "short_desc": "Results per page (max 100)",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Number of results per page; SendCloud caps this at 100",
            },
            "status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by return status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter returns by status",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "created", "display_name": "Created"},
                    <AllowedValueInfo>{"value": "announced", "display_name": "Announced"},
                    <AllowedValueInfo>{"value": "delivered", "display_name": "Delivered"},
                    <AllowedValueInfo>{"value": "cancelled", "display_name": "Cancelled"},
                    <AllowedValueInfo>{"value": "completed", "display_name": "Completed"},
                ),
            },
            "parcel_id": <DataProviderOptionInfo>{
                "display_name": "Parcel ID",
                "short_desc": "Filter returns by outgoing parcel ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter returns whose outgoing parcel matches the given ID",
                "ref_data": "parcels",
            },
            "created_at_from": <DataProviderOptionInfo>{
                "display_name": "Created From",
                "short_desc": "Earliest creation date",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Only return records created on or after this timestamp",
            },
            "created_at_to": <DataProviderOptionInfo>{
                "display_name": "Created To",
                "short_desc": "Latest creation date",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Only return records created on or before this timestamp",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum total records",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of aggregated records to return across pages",
            },
        };
    }

    constructor(*RestClientIo::RestClientIo rest) : SendCloudDataProviderBase(rest) {
    }

    string getName() {
        return "returns";
    }

    *string getDesc() {
        return "List and search SendCloud returns";
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new SendCloudReturnDataType().getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (search_options.id) {
            hash<auto> resp = doGet(sprintf("returns/%d", search_options.id));
            hash<auto> record = resp."return" ?? resp;
            return new DefaultRecordIterator(new ListHashIterator((record,)),
                where_cond, search_options);
        }
        hash<auto> params;
        if (search_options.status) {
            params.status = search_options.status;
        }
        if (search_options.parcel_id) {
            params.parcel_id = search_options.parcel_id;
        }
        if (search_options.created_at_from) {
            params.created_at_from = SendCloudDataProviderBase::formatDateTime(
                search_options.created_at_from);
        }
        if (search_options.created_at_to) {
            params.created_at_to = SendCloudDataProviderBase::formatDateTime(
                search_options.created_at_to);
        }
        if (search_options.page_size) {
            params.page_size = search_options.page_size;
        }
        if (search_options.cursor) {
            params.cursor = search_options.cursor;
        }
        list<auto> records = fetchAllCursorPages("returns", "returns", params, search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SendCloudDataProvider
