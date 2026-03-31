# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BitlyQrCodesDataProvider.qc Bitly QR codes collection data provider

/*  BitlyQrCodesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BitlyDataProvider module
public namespace BitlyDataProvider {

#! Bitly QR codes data provider
public class BitlyQrCodesDataProvider inherits BitlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "qr-codes",
            "desc": "Bitly QR codes data provider for creating and managing QR codes",
            "type": "BitlyQrCodesDataProvider",
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
            "qrcode_id": <DataProviderOptionInfo>{
                "display_name": "QR Code ID",
                "short_desc": "Get a specific QR code by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The QR code ID to retrieve. When provided, returns a single QR code",
                "ref_data": "qrcodes",
            },
            "group_guid": <DataProviderOptionInfo>{
                "display_name": "Group GUID",
                "short_desc": "Filter by group (required for listing)",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The group GUID to list QR codes for. Required when listing "
                    "QR codes (not needed when retrieving a single QR code by ID)",
                "ref_data": "groups",
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("BitlyCreateQrCodeDataProvider"),
            "update": Class::forName("BitlyUpdateQrCodeDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest) : BitlyDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "qr-codes";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Bitly QR codes data provider for creating and managing QR codes";
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
        return new BitlyQrCodeDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If qrcode_id provided, get single record
        if (search_options.qrcode_id) {
            hash<auto> result = doGet("qr-codes/" + search_options.qrcode_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }

        # QR codes listing is per-group: GET /v4/groups/{group_guid}/qr-codes
        *string group_guid = search_options.group_guid;
        if (!group_guid) {
            # Auto-detect default group
            hash<auto> groups = doGet("groups");
            if (groups.groups) {
                group_guid = groups.groups[0].guid;
            }
            if (!group_guid) {
                throw "BITLY-ERROR", "group_guid is required to list QR codes and no default "
                    "group could be detected";
            }
        }

        list<hash<auto>> all_records = fetchAllPages(
            "groups/" + group_guid + "/qr-codes", "qr_codes");
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BitlyDataProvider
