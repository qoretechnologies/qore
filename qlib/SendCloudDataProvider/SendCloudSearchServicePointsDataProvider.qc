# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudSearchServicePointsDataProvider.qc SendCloud search-service-points action provider

/*  SendCloudSearchServicePointsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Search-service-points request type
public class SendCloudSearchServicePointsRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "country": {
                "type": SoftStringType,
                "display_name": "Country",
                "short_desc": "ISO 3166-1 alpha-2 country code (required)",
                "desc": "Two-letter ISO country code to search service points within",
                "required": True,
            },
            "postal_code": {
                "type": SoftStringOrNothingType,
                "display_name": "Postal Code",
                "short_desc": "Postal code to search near",
                "desc": "Postal code used as a search origin",
            },
            "city": {
                "type": SoftStringOrNothingType,
                "display_name": "City",
                "short_desc": "City name filter",
                "desc": "Filter service points by city name",
            },
            "carrier": {
                "type": SoftStringOrNothingType,
                "display_name": "Carrier",
                "short_desc": "Carrier code filter",
                "desc": "Filter service points by carrier code (e.g., postnl, dhl, ups)",
            },
            "radius": {
                "type": SoftIntOrNothingType,
                "display_name": "Radius (m)",
                "short_desc": "Search radius in meters",
                "desc": "Radius in meters around the search origin (max 50000)",
            },
            "weight": {
                "type": SoftStringOrNothingType,
                "display_name": "Weight (kg)",
                "short_desc": "Parcel weight to filter by capability",
                "desc": "Parcel weight in kilograms (string-encoded decimal)",
            },
            "latitude": {
                "type": SoftStringOrNothingType,
                "display_name": "Latitude",
                "short_desc": "Latitude filter",
                "desc": "Latitude used as a search origin",
            },
            "longitude": {
                "type": SoftStringOrNothingType,
                "display_name": "Longitude",
                "short_desc": "Longitude filter",
                "desc": "Longitude used as a search origin",
            },
            "near_postal_code": {
                "type": SoftStringOrNothingType,
                "display_name": "Near Postal Code",
                "short_desc": "Postal code near-search origin",
                "desc": "Postal code to search service points near (with distance returned)",
            },
            "near_latitude": {
                "type": SoftStringOrNothingType,
                "display_name": "Near Latitude",
                "short_desc": "Latitude near-search origin",
                "desc": "Latitude to search service points near",
            },
            "near_longitude": {
                "type": SoftStringOrNothingType,
                "display_name": "Near Longitude",
                "short_desc": "Longitude near-search origin",
                "desc": "Longitude to search service points near",
            },
            "house_number": {
                "type": SoftStringOrNothingType,
                "display_name": "House Number",
                "short_desc": "House number near the search origin",
                "desc": "House number used together with the search origin",
            },
        };
    }

    constructor() : HashDataType("SendCloudSearchServicePointsRequestDataType") {
        addQoreFields(Fields);
    }
}

#! Search-service-points action provider (GET https://servicepoints.sendcloud.sc/api/v2/service-points)
public class SendCloudSearchServicePointsDataProvider inherits SendCloudDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "search-service-points",
            "desc": "Search SendCloud service points (returns a list of matching points)",
            "type": "SendCloudSearchServicePointsDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType RequestType = new SendCloudSearchServicePointsRequestDataType();
        static *AbstractDataProviderType ResponseType = new SoftListDataType(
            new SendCloudServicePointDataType(), True);
    }

    constructor(*RestClientIo::RestClientIo rest) : SendCloudDataProviderBase(rest) {
    }

    string getName() {
        return "search-service-points";
    }

    *string getDesc() {
        return "Search SendCloud service points";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> params;
        foreach string k in (keys SendCloudSearchServicePointsRequestDataType::Fields) {
            auto v = req{k};
            if (exists v) {
                params{k} = v;
            }
        }
        auto response = doAbsoluteGet(SendCloudDataProvider::ServicePointsBaseUrl + "/service-points", params);
        if (response.typeCode() == NT_LIST) {
            return response;
        } else if (response.typeCode() == NT_HASH) {
            return response.service_points ?? ();
        }
        return ();
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SendCloudDataProvider
