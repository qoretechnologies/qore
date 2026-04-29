# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudServicePointsDataProvider.qc SendCloud service-points collection provider (cross-host)

/*  SendCloudServicePointsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Collection provider for SendCloud service points (cross-host)
public class SendCloudServicePointsDataProvider inherits SendCloudDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "service-points",
            "desc": "Search SendCloud service points (cross-host to servicepoints.sendcloud.sc)",
            "type": "SendCloudServicePointsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        const SearchOptions = {
            "country": <DataProviderOptionInfo>{
                "display_name": "Country",
                "short_desc": "ISO 3166-1 alpha-2 country code (required)",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "Two-letter ISO country code to search service points within (required)",
                "required": True,
                "ref_data": "countries",
            },
            "postal_code": <DataProviderOptionInfo>{
                "display_name": "Postal Code",
                "short_desc": "Postal code to search near",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Postal code used as a search origin",
            },
            "city": <DataProviderOptionInfo>{
                "display_name": "City",
                "short_desc": "City name filter",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter service points by city name",
            },
            "carrier": <DataProviderOptionInfo>{
                "display_name": "Carrier",
                "short_desc": "Carrier code filter",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter service points by carrier code (e.g., postnl, dhl, ups)",
            },
            "radius": <DataProviderOptionInfo>{
                "display_name": "Radius (m)",
                "short_desc": "Search radius in meters",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Radius in meters around the search origin (max 50000)",
            },
            "weight": <DataProviderOptionInfo>{
                "display_name": "Weight (kg)",
                "short_desc": "Parcel weight to filter by capability",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Parcel weight (kg, string-encoded decimal); filters to points that accept this weight",
            },
            "latitude": <DataProviderOptionInfo>{
                "display_name": "Latitude",
                "short_desc": "Latitude filter",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Latitude used as a search origin",
            },
            "longitude": <DataProviderOptionInfo>{
                "display_name": "Longitude",
                "short_desc": "Longitude filter",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Longitude used as a search origin",
            },
            "near_postal_code": <DataProviderOptionInfo>{
                "display_name": "Near Postal Code",
                "short_desc": "Postal code near-search origin",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Postal code to search service points near (with distance returned)",
            },
            "near_latitude": <DataProviderOptionInfo>{
                "display_name": "Near Latitude",
                "short_desc": "Latitude near-search origin",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Latitude to search service points near (with distance returned)",
            },
            "near_longitude": <DataProviderOptionInfo>{
                "display_name": "Near Longitude",
                "short_desc": "Longitude near-search origin",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Longitude to search service points near (with distance returned)",
            },
            "house_number": <DataProviderOptionInfo>{
                "display_name": "House Number",
                "short_desc": "House number near search origin",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "House number used together with the search origin",
            },
        };
    }

    constructor(*RestClientIo::RestClientIo rest) : SendCloudDataProviderBase(rest) {
    }

    string getName() {
        return "service-points";
    }

    *string getDesc() {
        return "Search SendCloud service points";
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new SendCloudServicePointDataType().getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params;
        foreach string k in (keys SearchOptions) {
            auto v = search_options{k};
            if (exists v) {
                params{k} = v;
            }
        }
        auto response = doAbsoluteGet(SendCloudDataProvider::ServicePointsBaseUrl + "/service-points", params);
        list<auto> records;
        if (response.typeCode() == NT_LIST) {
            records = response;
        } else if (response.typeCode() == NT_HASH) {
            records = response.service_points ?? ();
        } else {
            records = ();
        }
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SendCloudDataProvider
