# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LemlistDataProvider module definition

/*  LemlistContactsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LemlistDataProvider module
public namespace LemlistDataProvider {

#! Lemlist contacts collection data provider
public class LemlistContactsDataProvider inherits LemlistDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "contacts",
            "desc": "Lemlist contacts data provider - list and get contacts",
            "type": "LemlistContactsDataProvider",
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
            "email": <DataProviderOptionInfo>{
                "display_name": "Email",
                "short_desc": "Email address to look up",
                "type": AbstractDataProviderType::get(StringType),
                "desc": "The email address of the contact to retrieve.\n\n"
                    "The Lemlist v2 API requires a specific email address to look up contacts "
                    "(the `idsOrEmails` query parameter is required).",
                "required": True,
            },
        };
    }

    private {
        #! Record type
        static LemlistContactDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(LemlistRestClient::LemlistRestClient rest)
            : LemlistDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(LemlistRestClient::LemlistRestConnection conn) : LemlistDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "contacts";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Lemlist contacts data provider - list and get contacts";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for contacts
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (!search_options.email) {
            throw "LEMLIST-ERROR", "search option 'email' is required for listing contacts "
                "(Lemlist v2 API requires idsOrEmails query parameter)";
        }

        # Lemlist v2 contacts API requires idsOrEmails[] query parameter
        auto response = doGet("/contacts", {"idsOrEmails": search_options.email});
        list<hash<auto>> records;
        if (response.typeCode() == NT_LIST) {
            records = response;
        } else if (response.typeCode() == NT_HASH) {
            records = (response,);
        }

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, NOTHING,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace LemlistDataProvider
