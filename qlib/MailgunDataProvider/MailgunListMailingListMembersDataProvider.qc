# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MailgunDataProvider module definition

/*  MailgunListMailingListMembersDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the MailgunDataProvider module
public namespace MailgunDataProvider {

#! Mailgun mailing list members collection data provider
public class MailgunListMailingListMembersDataProvider inherits MailgunDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-mailing-list-members",
            "desc": "List members of a Mailgun mailing list",
            "type": "MailgunListMailingListMembersDataProvider",
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
            "list_address": <DataProviderOptionInfo>{
                "display_name": "Mailing List",
                "short_desc": "The mailing list address",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The email address of the mailing list to get members from",
                "required": True,
            },
            "subscribed": <DataProviderOptionInfo>{
                "display_name": "Subscribed",
                "short_desc": "Filter by subscription status",
                "type": AbstractDataProviderType::get(BoolOrNothingType),
                "desc": "Filter members by subscription status",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Maximum number of members to return",
            },
        };
    }

    private {
        #! Record type
        static MailgunMailingListMemberDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(MailgunRestClient::MailgunRestClient rest, *string api_domain)
            : MailgunDataProviderBase(rest, api_domain) {
    }

    #! Creates the object with a connection
    constructor(MailgunRestClient::MailgunRestConnection conn) : MailgunDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "list-mailing-list-members";
    }

    #! Returns the provider description
    *string getDesc() {
        return "List members of a Mailgun mailing list";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for mailing list members
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        string list_address = search_options.list_address.value ?? search_options.list_address;
        if (!list_address.val()) {
            throw "MAILGUN-ERROR", "list_address is required to list members";
        }

        hash<auto> params;

        if (exists search_options.subscribed) {
            params.subscribed = search_options.subscribed ? "yes" : "no";
        }
        if (search_options.limit) {
            params.limit = search_options.limit;
        } else {
            params.limit = DefaultPageSize;
        }

        @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: list_address=%s params=%y", list_address, params));

        string path = sprintf("%s/lists/%s/members/pages", ApiV3, list_address);
        hash<auto> response = doGet(path, params);
        list<hash<auto>> records = response.items ?? ();

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace MailgunDataProvider
