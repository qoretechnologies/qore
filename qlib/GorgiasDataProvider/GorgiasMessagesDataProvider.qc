# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GorgiasMessagesDataProvider.qc Gorgias messages collection data provider

/*  GorgiasMessagesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Gorgias messages collection data provider
public class GorgiasMessagesDataProvider inherits GorgiasDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "messages",
            "desc": "Gorgias messages data provider for listing and creating ticket messages",
            "type": "GorgiasMessagesDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        const SearchOptions = {
            "ticket_id": <DataProviderOptionInfo>{
                "display_name": "Ticket ID",
                "short_desc": "The ticket to list messages for",
                "type": AbstractDataProviderType::get(SoftIntType),
                "desc": "The ID of the ticket to list messages for. Required for listing",
                "required": True,
                "ref_data": "tickets",
            },
            "message_id": <DataProviderOptionInfo>{
                "display_name": "Message ID",
                "short_desc": "Get a specific message by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "The message ID to retrieve. When provided with ticket_id, "
                    "returns a single message",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };

        const ChildMap = {
            "create": Class::forName("GorgiasCreateMessageDataProvider"),
            "update": Class::forName("GorgiasUpdateMessageDataProvider"),
            "delete": Class::forName("GorgiasDeleteMessageDataProvider"),
        };
    }

    constructor(RestClientIo::RestClientIo rest) : GorgiasDataProviderBase(rest) {
    }

    string getName() {
        return "messages";
    }

    *string getDesc() {
        return "Gorgias messages data provider for listing and creating ticket messages";
    }

    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new GorgiasMessageDataType().getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        int ticket_id = search_options.ticket_id;

        # Get a specific message
        if (search_options.message_id) {
            hash<auto> result = doGet("tickets/" + ticket_id + "/messages/"
                + search_options.message_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        # List all messages for the ticket
        list<hash<auto>> all_records = fetchAllPages(
            "tickets/" + ticket_id + "/messages", NOTHING, search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GorgiasDataProvider
