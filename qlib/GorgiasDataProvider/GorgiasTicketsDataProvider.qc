# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GorgiasTicketsDataProvider.qc Gorgias tickets collection data provider

/*  GorgiasTicketsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GorgiasDataProvider module
public namespace GorgiasDataProvider {

#! Gorgias tickets collection data provider
public class GorgiasTicketsDataProvider inherits GorgiasDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "tickets",
            "desc": "Gorgias tickets data provider for listing and searching support tickets",
            "type": "GorgiasTicketsDataProvider",
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
            "ticket_id": <DataProviderOptionInfo>{
                "display_name": "Ticket ID",
                "short_desc": "Get a specific ticket by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "The ticket ID to retrieve. When provided, returns a single record",
                "ref_data": "tickets",
            },
            "status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter tickets by status",
                "allowed_values": StatusAllowedValues,
            },
            "assignee_user_id": <DataProviderOptionInfo>{
                "display_name": "Assignee User ID",
                "short_desc": "Filter by assigned agent",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter tickets by the assigned agent's ID",
                "ref_data": "users",
            },
            "customer_id": <DataProviderOptionInfo>{
                "display_name": "Customer ID",
                "short_desc": "Filter by customer",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter tickets by customer ID",
                "ref_data": "customers",
            },
            "channel": <DataProviderOptionInfo>{
                "display_name": "Channel",
                "short_desc": "Filter by channel",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter tickets by communication channel",
                "allowed_values": ChannelAllowedValues,
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
            "create": Class::forName("GorgiasCreateTicketDataProvider"),
            "update": Class::forName("GorgiasUpdateTicketDataProvider"),
            "delete": Class::forName("GorgiasDeleteTicketDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest) : GorgiasDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "tickets";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Gorgias tickets data provider for listing and searching support tickets";
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
        return new GorgiasTicketDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If ticket ID provided, get single record
        if (search_options.ticket_id) {
            hash<auto> result = doGet("tickets/" + search_options.ticket_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        # Build query params from search options
        hash<auto> params;
        if (search_options.status) {
            params.status = search_options.status;
        }
        if (search_options.assignee_user_id) {
            params.assignee_user_id = search_options.assignee_user_id;
        }
        if (search_options.customer_id) {
            params.customer_id = search_options.customer_id;
        }
        if (search_options.channel) {
            params.channel = search_options.channel;
        }

        list<hash<auto>> all_records = fetchAllPages("tickets", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GorgiasDataProvider
