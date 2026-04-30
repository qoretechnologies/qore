# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudParcelsDataProvider.qc SendCloud parcels collection data provider

/*  SendCloudParcelsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Collection provider for SendCloud parcels
public class SendCloudParcelsDataProvider inherits SendCloudDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "parcels",
            "desc": "List and search SendCloud parcels",
            "type": "SendCloudParcelsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Parcel ID",
                "short_desc": "Retrieve a single parcel by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "If supplied, fetches a single parcel with this numeric ID instead of listing",
                "ref_data": "parcels",
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
            "order_number": <DataProviderOptionInfo>{
                "display_name": "Order Number",
                "short_desc": "Filter by merchant order number",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter parcels by the merchant-provided order number",
            },
            "tracking_number": <DataProviderOptionInfo>{
                "display_name": "Tracking Number",
                "short_desc": "Filter by carrier tracking number",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter parcels by the carrier tracking number",
            },
            "external_reference": <DataProviderOptionInfo>{
                "display_name": "External Reference",
                "short_desc": "Filter by external reference",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter parcels by the merchant-supplied `external_reference` field",
            },
            "parcel_status": <DataProviderOptionInfo>{
                "display_name": "Parcel Status",
                "short_desc": "Filter by parcel status ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Numeric SendCloud parcel-status ID to filter on (see SendCloud's status IDs)",
            },
            "announced_after": <DataProviderOptionInfo>{
                "display_name": "Announced After",
                "short_desc": "Earliest announcement date",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Only return parcels announced to the carrier on or after this timestamp",
            },
            "updated_after": <DataProviderOptionInfo>{
                "display_name": "Updated After",
                "short_desc": "Earliest update date",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Only return parcels updated on or after this timestamp",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum total records",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of aggregated records to return across pages",
            },
        };
    }

    #! Creates the object with a REST client
    constructor(*RestClientIo::RestClientIo rest) : SendCloudDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "parcels";
    }

    #! Returns the provider description
    *string getDesc() {
        return "List and search SendCloud parcels";
    }

    #! Returns the record type for parcels
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new SendCloudParcelDataType().getFields();
    }

    #! Searches for parcels
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If id supplied, fetch a single record
        if (search_options.id) {
            hash<auto> resp = doGet(sprintf("parcels/%d", search_options.id));
            hash<auto> record = resp.parcel ?? resp;
            return new DefaultRecordIterator(new ListHashIterator((record,)),
                where_cond, search_options);
        }
        hash<auto> params;
        if (search_options.order_number) {
            params.order_number = search_options.order_number;
        }
        if (search_options.tracking_number) {
            params.tracking_number = search_options.tracking_number;
        }
        if (exists search_options.external_reference) {
            params.external_reference = search_options.external_reference;
        }
        if (exists search_options.parcel_status) {
            params.parcel_status = search_options.parcel_status;
        }
        if (search_options.announced_after) {
            params.announced_after = SendCloudDataProviderBase::formatDateTime(
                search_options.announced_after);
        }
        if (search_options.updated_after) {
            params.updated_after = SendCloudDataProviderBase::formatDateTime(
                search_options.updated_after);
        }
        if (search_options.page_size) {
            params.page_size = search_options.page_size;
        }
        if (search_options.cursor) {
            params.cursor = search_options.cursor;
        }
        list<auto> records = fetchAllCursorPages("parcels", "parcels", params, search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SendCloudDataProvider
