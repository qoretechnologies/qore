# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FreshsalesAppointmentsDataProvider.qc Freshsales appointments collection data provider

/*  FreshsalesAppointmentsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshsalesDataProvider module
public namespace FreshsalesDataProvider {

#! Freshsales appointments collection data provider
public class FreshsalesAppointmentsDataProvider inherits FreshsalesDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "appointments",
            "desc": "Freshsales appointments data provider for listing and searching appointments",
            "type": "FreshsalesAppointmentsDataProvider",
            "has_record": True,
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
            "id": <DataProviderOptionInfo>{
                "display_name": "Appointment ID",
                "short_desc": "Get a specific appointment by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "The appointment ID to retrieve. When provided, returns a single record",
            },
            "filter": <DataProviderOptionInfo>{
                "display_name": "Filter",
                "short_desc": "Filter appointments by status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter appointments by time period",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "past", "display_name": "Past"},
                    <AllowedValueInfo>{"value": "upcoming", "display_name": "Upcoming"},
                ),
                "preselected": True,
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
            "create": Class::forName("FreshsalesCreateAppointmentDataProvider"),
            "update": Class::forName("FreshsalesUpdateAppointmentDataProvider"),
            "delete": Class::forName("FreshsalesDeleteAppointmentDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest) : FreshsalesDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "appointments";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Freshsales appointments data provider for listing and searching appointments";
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
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new FreshsalesAppointmentDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (search_options.id) {
            hash<auto> result = doGet("appointments/" + string(search_options.id));
            hash<auto> appointment = result.appointment ?? result;
            return new DefaultRecordIterator(new ListHashIterator((appointment,)),
                where_cond, search_options);
        }

        hash<auto> params;
        if (search_options.filter) {
            params.filter = search_options.filter;
        } else {
            params.filter = "upcoming";
        }

        list<hash<auto>> all_records = fetchAllPages("appointments", "appointments", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records),
            where_cond, search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace FreshsalesDataProvider
