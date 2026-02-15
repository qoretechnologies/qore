# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationShipmentsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShipStationDataProvider module
public namespace ShipStationDataProvider {

#! ShipStation shipments collection data provider
public class ShipStationShipmentsDataProvider inherits ShipStationDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "shipments",
            "desc": "ShipStation shipments data provider - list and search shipments",
            "type": "ShipStationShipmentsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "recipientName": <DataProviderOptionInfo>{
                "display_name": "Recipient Name",
                "short_desc": "Filter by recipient name",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter shipments by recipient name (partial match)",
            },
            "recipientCountryCode": <DataProviderOptionInfo>{
                "display_name": "Country Code",
                "short_desc": "Filter by country",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter shipments by two-letter country code",
            },
            "orderNumber": <DataProviderOptionInfo>{
                "display_name": "Order Number",
                "short_desc": "Filter by order number",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter shipments by order number",
            },
            "orderId": <DataProviderOptionInfo>{
                "display_name": "Order ID",
                "short_desc": "Filter by order ID",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter shipments by order ID",
            },
            "carrierCode": <DataProviderOptionInfo>{
                "display_name": "Carrier Code",
                "short_desc": "Filter by carrier",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter shipments by carrier code",
                "ref_data": "carriers",
            },
            "serviceCode": <DataProviderOptionInfo>{
                "display_name": "Service Code",
                "short_desc": "Filter by service",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter shipments by service code",
            },
            "trackingNumber": <DataProviderOptionInfo>{
                "display_name": "Tracking Number",
                "short_desc": "Filter by tracking number",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter shipments by tracking number (partial match)",
            },
            "shipDateStart": <DataProviderOptionInfo>{
                "display_name": "Ship Date Start",
                "short_desc": "Start of ship date range",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Filter shipments shipped on or after this date",
            },
            "shipDateEnd": <DataProviderOptionInfo>{
                "display_name": "Ship Date End",
                "short_desc": "End of ship date range",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Filter shipments shipped on or before this date",
            },
            "createDateStart": <DataProviderOptionInfo>{
                "display_name": "Create Date Start",
                "short_desc": "Start of create date range",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Filter shipments created on or after this date",
            },
            "createDateEnd": <DataProviderOptionInfo>{
                "display_name": "Create Date End",
                "short_desc": "End of create date range",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Filter shipments created on or before this date",
            },
            "voidDateStart": <DataProviderOptionInfo>{
                "display_name": "Void Date Start",
                "short_desc": "Start of void date range",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter voided shipments on or after this date",
            },
            "voidDateEnd": <DataProviderOptionInfo>{
                "display_name": "Void Date End",
                "short_desc": "End of void date range",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter voided shipments on or before this date",
            },
            "storeId": <DataProviderOptionInfo>{
                "display_name": "Store",
                "short_desc": "Filter by store",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter shipments by store ID",
                "ref_data": "stores",
            },
            "includeShipmentItems": <DataProviderOptionInfo>{
                "display_name": "Include Items",
                "short_desc": "Include shipment items",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "Include shipment items in the response",
            },
            "sortBy": <DataProviderOptionInfo>{
                "display_name": "Sort By",
                "short_desc": "Field to sort by",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Sort results by this field",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "ShipDate", "desc": "Sort by ship date"},
                    <AllowedValueInfo>{"value": "CreateDate", "desc": "Sort by create date"},
                ),
            },
            "sortDir": <DataProviderOptionInfo>{
                "display_name": "Sort Direction",
                "short_desc": "Sort direction",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Sort direction (ASC or DESC)",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "ASC", "desc": "Ascending"},
                    <AllowedValueInfo>{"value": "DESC", "desc": "Descending"},
                ),
            },
            "page": <DataProviderOptionInfo>{
                "display_name": "Page",
                "short_desc": "Page number",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Page number for pagination (starting at 1)",
            },
            "pageSize": <DataProviderOptionInfo>{
                "display_name": "Page Size",
                "short_desc": "Results per page",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Number of results per page (max 500)",
            },
        };
    }

    private {
        #! Record type
        static ShipStationShipmentDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(ShipStationRestClient::ShipStationRestClient rest)
            : ShipStationDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ShipStationRestClient::ShipStationRestConnection conn) : ShipStationDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "shipments";
    }

    #! Returns the provider description
    *string getDesc() {
        return "ShipStation shipments data provider - list and search shipments";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for shipments
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params;

        if (search_options.recipientName) {
            params.recipientName = search_options.recipientName;
        }
        if (search_options.recipientCountryCode) {
            params.recipientCountryCode = search_options.recipientCountryCode;
        }
        if (search_options.orderNumber) {
            params.orderNumber = search_options.orderNumber;
        }
        if (search_options.orderId) {
            params.orderId = search_options.orderId;
        }
        if (search_options.carrierCode) {
            params.carrierCode = search_options.carrierCode;
        }
        if (search_options.serviceCode) {
            params.serviceCode = search_options.serviceCode;
        }
        if (search_options.trackingNumber) {
            params.trackingNumber = search_options.trackingNumber;
        }
        if (search_options.shipDateStart) {
            params.shipDateStart = search_options.shipDateStart;
        }
        if (search_options.shipDateEnd) {
            params.shipDateEnd = search_options.shipDateEnd;
        }
        if (search_options.createDateStart) {
            params.createDateStart = search_options.createDateStart;
        }
        if (search_options.createDateEnd) {
            params.createDateEnd = search_options.createDateEnd;
        }
        if (search_options.voidDateStart) {
            params.voidDateStart = search_options.voidDateStart;
        }
        if (search_options.voidDateEnd) {
            params.voidDateEnd = search_options.voidDateEnd;
        }
        if (search_options.storeId) {
            params.storeId = search_options.storeId;
        }
        if (search_options.includeShipmentItems) {
            params.includeShipmentItems = search_options.includeShipmentItems ? "true" : "false";
        }
        if (search_options.sortBy) {
            params.sortBy = search_options.sortBy;
        }
        if (search_options.sortDir) {
            params.sortDir = search_options.sortDir;
        }
        if (search_options.page) {
            params.page = search_options.page;
        }
        if (search_options.pageSize) {
            params.pageSize = search_options.pageSize;
        } else {
            params.pageSize = DefaultPageSize;
        }

        @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: params=%y", params));

        hash<auto> response = doGet("/shipments", params);
        list<hash<auto>> records = response.shipments ?? ();

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ShipStationDataProvider
