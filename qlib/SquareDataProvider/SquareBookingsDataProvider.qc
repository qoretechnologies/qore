# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareBookingsDataProvider.qc Square bookings collection data provider

/*  SquareBookingsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquareDataProvider module
public namespace SquareDataProvider {

#! Square bookings collection data provider
public class SquareBookingsDataProvider inherits SquareDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "bookings",
            "desc": "Square bookings data provider for listing appointments",
            "type": "SquareBookingsDataProvider",
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
            "booking_id": <DataProviderOptionInfo>{
                "display_name": "Booking ID",
                "short_desc": "Get a specific booking by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by booking ID for single record lookup",
            },
            "location_id": <DataProviderOptionInfo>{
                "display_name": "Location ID",
                "short_desc": "Filter by location",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter bookings by location ID",
            },
            "start_at_min": <DataProviderOptionInfo>{
                "display_name": "Start At Min",
                "short_desc": "Earliest start time",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Return bookings starting at or after this timestamp",
            },
            "start_at_max": <DataProviderOptionInfo>{
                "display_name": "Start At Max",
                "short_desc": "Latest start time",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Return bookings starting before this timestamp",
            },
            "team_member_id": <DataProviderOptionInfo>{
                "display_name": "Team Member ID",
                "short_desc": "Filter by team member",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter bookings by team member ID",
            },
            "customer_id": <DataProviderOptionInfo>{
                "display_name": "Customer ID",
                "short_desc": "Filter by customer",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter bookings by customer ID",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("SquareCreateBookingDataProvider"),
            "cancel": Class::forName("SquareCancelBookingDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(SquareRestClient::SquareRestClient rest) : SquareDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "bookings";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Square bookings data provider for listing appointments";
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
    private *AbstractDataProviderType getRecordTypeImpl() {
        return new SquareBookingDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If booking_id provided, get single record
        if (search_options.booking_id) {
            hash<auto> result = doGet("bookings/" + search_options.booking_id);
            return new DefaultRecordIterator(new ListHashIterator((result.booking,)),
                where_cond, search_options);
        }

        # Build query params
        hash<auto> params;
        if (search_options.location_id) {
            params.location_id = search_options.location_id;
        }
        if (search_options.start_at_min) {
            params.start_at_min = formatDateTimeSquare(search_options.start_at_min);
        }
        if (search_options.start_at_max) {
            params.start_at_max = formatDateTimeSquare(search_options.start_at_max);
        }
        if (search_options.team_member_id) {
            params.team_member_id = search_options.team_member_id;
        }
        if (search_options.customer_id) {
            params.customer_id = search_options.customer_id;
        }

        list<hash<auto>> all_records = fetchAllPages("bookings", "bookings", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquareDataProvider
