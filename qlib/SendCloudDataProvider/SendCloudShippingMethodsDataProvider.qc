# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudShippingMethodsDataProvider.qc SendCloud shipping-methods collection data provider

/*  SendCloudShippingMethodsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Collection provider for SendCloud shipping methods
public class SendCloudShippingMethodsDataProvider inherits SendCloudDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "shipping-methods",
            "desc": "List SendCloud shipping methods (optionally filtered by sender address, destination, etc.)",
            "type": "SendCloudShippingMethodsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        const SearchOptions = {
            "sender_address": <DataProviderOptionInfo>{
                "display_name": "Sender Address",
                "short_desc": "Filter methods by sender address",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Only return shipping methods available for the given sender address ID",
                "ref_data": "sender_addresses",
            },
            "to_country": <DataProviderOptionInfo>{
                "display_name": "To Country",
                "short_desc": "Destination country filter",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "ISO 3166-1 alpha-2 destination country code; only methods shipping to this country are returned",
                "ref_data": "countries",
            },
            "service_point_input": <DataProviderOptionInfo>{
                "display_name": "Service Point Input",
                "short_desc": "Service-point input filter",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter methods by whether a service point is required, optional, or not used",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "none", "display_name": "None",
                        "desc": "Methods that don't accept a service point"},
                    <AllowedValueInfo>{"value": "required", "display_name": "Required",
                        "desc": "Methods that require a service point"},
                    <AllowedValueInfo>{"value": "optional", "display_name": "Optional",
                        "desc": "Methods where a service point is optional"},
                ),
            },
            "is_return": <DataProviderOptionInfo>{
                "display_name": "Is Return",
                "short_desc": "Filter return methods",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "If `True`, only return-capable shipping methods are returned",
            },
            "price_breakdown": <DataProviderOptionInfo>{
                "display_name": "Price Breakdown",
                "short_desc": "Include detailed price breakdown",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "If `True`, include a detailed pricing breakdown on each method",
            },
        };
    }

    constructor(*RestClientIo::RestClientIo rest) : SendCloudDataProviderBase(rest) {
    }

    string getName() {
        return "shipping-methods";
    }

    *string getDesc() {
        return "List SendCloud shipping methods";
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new SendCloudShippingMethodDataType().getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params;
        if (search_options.sender_address) {
            params.sender_address = search_options.sender_address;
        }
        if (search_options.to_country) {
            params.to_country = search_options.to_country;
        }
        if (search_options.service_point_input) {
            params.service_point_input = search_options.service_point_input;
        }
        if (exists search_options.is_return) {
            params.is_return = search_options.is_return ? "true" : "false";
        }
        if (exists search_options.price_breakdown) {
            params.price_breakdown = search_options.price_breakdown ? "true" : "false";
        }
        hash<auto> response = doGet("shipping_methods", params);
        list<auto> records = response.shipping_methods ?? ();
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SendCloudDataProvider
