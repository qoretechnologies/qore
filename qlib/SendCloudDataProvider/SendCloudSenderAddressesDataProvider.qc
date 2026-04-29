# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudSenderAddressesDataProvider.qc SendCloud sender-addresses collection data provider

/*  SendCloudSenderAddressesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Collection provider for SendCloud sender addresses
public class SendCloudSenderAddressesDataProvider inherits SendCloudDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "sender-addresses",
            "desc": "List SendCloud sender addresses on the account",
            "type": "SendCloudSenderAddressesDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        const SearchOptions = {};
    }

    constructor(*RestClientIo::RestClientIo rest) : SendCloudDataProviderBase(rest) {
    }

    string getName() {
        return "sender-addresses";
    }

    *string getDesc() {
        return "List SendCloud sender addresses";
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new SendCloudSenderAddressDataType().getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> response = doGet("user/addresses/sender");
        list<auto> records = response.sender_addresses ?? ();
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SendCloudDataProvider
