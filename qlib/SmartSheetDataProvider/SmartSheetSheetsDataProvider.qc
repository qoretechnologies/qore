# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SmartSheetDataProvider module definition

/*  SmartSheetSheetsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SmartSheetDataProvider module
public namespace SmartSheetDataProvider {

#! Smartsheet sheets (tables) container data provider
/** Lists all available sheets as child providers. Each sheet child is a dynamic table provider
    that builds its record type from the sheet's column definitions.
*/
public class SmartSheetSheetsDataProvider inherits SmartSheetDataProviderBase {
    public {
        #! Search options for the sheets list
        const SearchOptions = {
            "sheetId": <DataProviderOptionInfo>{
                "display_name": "Sheet ID",
                "short_desc": "Get a specific sheet by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The sheet ID to retrieve. When provided, returns a single sheet",
                "ref_data": "sheets",
            },
        };

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "sheets",
            "desc": "Smartsheet sheets container. Lists all available sheets as child "
                "providers with table-like record interfaces for rows",
            "type": "SmartSheetSheetsDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_records": True,
            "children_can_support_apis": True,
            "search_options": SearchOptions,
        };

        #! Static child map for CRUD operations on sheets themselves
        const StaticChildMap = {
            "create": Class::forName("SmartSheetCreateSheetDataProvider"),
            "update": Class::forName("SmartSheetUpdateSheetDataProvider"),
            "delete": Class::forName("SmartSheetDeleteSheetDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(RestClientIo::RestClientIo rest) : SmartSheetDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return "sheets";
    }

    #! Returns the data provider description
    *string getDesc() {
        return ProviderInfo.desc;
    }

    #! Returns the record type for sheets
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new SmartSheetSheetDataType().getFields();
    }

    #! Searches for sheets
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (search_options.sheetId) {
            hash<auto> result = doGet("sheets/" + search_options.sheetId);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }
        list<hash<auto>> records = fetchAllPages("sheets", "data");
        return new DefaultRecordIterator(new ListHashIterator(records),
            where_cond, search_options);
    }

    #! Returns a list of child data provider names
    /** Returns both static children (create, update, delete) and dynamic sheet names/IDs
    */
    private *list<string> getChildProviderNamesImpl() {
        list<string> rv = keys StaticChildMap;
        try {
            list<hash<auto>> sheets = fetchAllPages("sheets", "data", NOTHING, 1000);
            foreach hash<auto> sheet in (sheets) {
                rv += string(sheet.id);
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch sheet list: %s: %s", ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns a child provider by name
    /** @param name either a static child name (create/update/delete) or a sheet ID
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        # Check static children first
        *Class cls = StaticChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }

        # Try as a sheet ID
        try {
            hash<auto> sheet = doGet("sheets/" + name);
            return new SmartSheetSheetDataProvider(rest, sheet);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "REST-RESPONSE-ERROR" && ex.arg.status_code == 404) {
                return;
            }
            rethrow;
        }
    }

    #! Returns child provider summary info
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        list<hash<DataProviderSummaryInfo>> rv = ();
        try {
            list<hash<auto>> sheets = fetchAllPages("sheets", "data", NOTHING, 1000);
            sheets = sort(sheets, int sub(hash<auto> l, hash<auto> r) {
                return l.name <=> r.name;
            });
            foreach hash<auto> sheet in (sheets) {
                rv += <DataProviderSummaryInfo>{
                    "name": string(sheet.id),
                    "short_desc": sheet.name,
                    "desc": sprintf("Smartsheet sheet '%s' (ID: %s)", sheet.name, sheet.id),
                    "type": "SmartSheetSheetDataProvider",
                    "supports_read": True,
                    "supports_create": True,
                    "supports_update": True,
                    "supports_delete": True,
                    "supports_native_search": True,
                    "has_record": True,
                };
            }
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::WARN, "Failed to fetch sheet summary: %s: %s", ex.err, ex.desc);
        }
        return rv;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SmartSheetDataProvider
