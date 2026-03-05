# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreFindAssemblyDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the Cin7CoreDataProvider module
public namespace Cin7CoreDataProvider {

#! Cin7Core assemblies/finished goods collection data provider
public class Cin7CoreFindAssemblyDataProvider inherits Cin7CoreDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "find-assembly",
            "desc": "Cin7Core assemblies data provider - find finished goods/assembly tasks",
            "type": "Cin7CoreFindAssemblyDataProvider",
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
            "TaskID": <DataProviderOptionInfo>{
                "display_name": "Task ID",
                "short_desc": "Filter by task ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Return a specific assembly task by ID",
                "preselected": True,
            },
            "AssemblyNumber": <DataProviderOptionInfo>{
                "display_name": "Assembly Number",
                "short_desc": "Filter by assembly number",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Return assembly by number",
                "preselected": True,
            },
            "Status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by status",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter assemblies by status",
                "preselected": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Draft", "desc": "Draft assembly"},
                    <AllowedValueInfo>{"value": "Authorized", "desc": "Authorized assembly"},
                    <AllowedValueInfo>{"value": "Void", "desc": "Voided assembly"},
                ),
            },
            "Location": <DataProviderOptionInfo>{
                "display_name": "Location",
                "short_desc": "Filter by location",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter by warehouse location",
            },
            "ModifiedSince": <DataProviderOptionInfo>{
                "display_name": "Modified Since",
                "short_desc": "Filter by modification date",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Return assemblies modified after this date/time",
            },
            "Page": <DataProviderOptionInfo>{
                "display_name": "Page",
                "short_desc": "Page number",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Page number for pagination (starting at 1)",
            },
            "Limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Results per page",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Number of results per page (default 100, max 1000)",
            },
        };
    }

    private {
        #! Record type
        static Cin7CoreAssemblyDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(Cin7CoreRestClient::Cin7CoreRestClient rest) : Cin7CoreDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(Cin7CoreRestClient::Cin7CoreRestConnection conn) : Cin7CoreDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : Cin7CoreDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "find-assembly";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Cin7Core assemblies data provider - find finished goods/assembly tasks";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for assemblies
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params;

        # Build query parameters from search options
        if (search_options.TaskID) {
            params.TaskID = search_options.TaskID;
        }
        if (search_options.AssemblyNumber) {
            params.AssemblyNumber = search_options.AssemblyNumber;
        }
        if (search_options.Status) {
            params.Status = search_options.Status;
        }
        if (search_options.Location) {
            params.Location = search_options.Location;
        }
        if (search_options.ModifiedSince) {
            params.ModifiedSince = search_options.ModifiedSince;
        }
        if (search_options.Page) {
            params.Page = search_options.Page;
        } else {
            params.Page = 1;
        }
        if (search_options.Limit) {
            params.Limit = search_options.Limit;
        } else {
            params.Limit = DefaultPageSize;
        }

        @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: params=%y", params));

        hash<auto> response = doGet("/FinishedGood", params);
        list<hash<auto>> records = response.FinishedGoodList ?? ();

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Assembly/finished goods data type
public class Cin7CoreAssemblyDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "TaskID": {
                "type": StringOrNothingType,
                "display_name": "Task ID",
                "short_desc": "Assembly task ID",
                "desc": "The unique identifier for the assembly task",
            },
            "AssemblyNumber": {
                "type": StringOrNothingType,
                "display_name": "Assembly Number",
                "short_desc": "Assembly number",
                "desc": "The assembly document number",
            },
            "Status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Assembly status",
                "desc": "The status of the assembly",
            },
            "Location": {
                "type": StringOrNothingType,
                "display_name": "Location",
                "short_desc": "Warehouse location",
                "desc": "The warehouse location for this assembly",
            },
            "AssemblyDate": {
                "type": DateOrNothingType,
                "display_name": "Assembly Date",
                "short_desc": "Date of assembly",
                "desc": "The date of the assembly",
            },
            "ProductFamily": {
                "type": StringOrNothingType,
                "display_name": "Product Family",
                "short_desc": "Product family",
                "desc": "The product family being assembled",
            },
            "FinishedGood": {
                "type": new Cin7CoreAssemblyProductDataType(AutoHashOrNothingType),
                "display_name": "Finished Good",
                "short_desc": "Finished product",
                "desc": "The finished product being created",
            },
            "Components": {
                "type": new ListDataType("Components", new Cin7CoreAssemblyProductDataType(), True),
                "display_name": "Components",
                "short_desc": "Component list",
                "desc": "List of components used in the assembly",
            },
            "Quantity": {
                "type": FloatOrNothingType,
                "display_name": "Quantity",
                "short_desc": "Quantity assembled",
                "desc": "The quantity being assembled",
            },
            "Memo": {
                "type": StringOrNothingType,
                "display_name": "Memo",
                "short_desc": "Notes",
                "desc": "Notes about the assembly",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreAssemblyDataType") {
        addQoreFields(Fields);
    }
}

#! Cin7Core assembly product data type (used for finished goods and components)
public class Cin7CoreAssemblyProductDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "ProductID": {
                "type": StringOrNothingType,
                "display_name": "Product ID",
                "short_desc": "Product identifier",
                "desc": "The unique identifier of the product",
            },
            "SKU": {
                "type": StringOrNothingType,
                "display_name": "SKU",
                "short_desc": "Stock keeping unit",
                "desc": "The product SKU",
            },
            "Name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Product name",
                "desc": "The product name",
            },
            "Quantity": {
                "type": FloatOrNothingType,
                "display_name": "Quantity",
                "short_desc": "Quantity",
                "desc": "The quantity of this product in the assembly",
            },
            "UnitCost": {
                "type": FloatOrNothingType,
                "display_name": "Unit Cost",
                "short_desc": "Cost per unit",
                "desc": "The unit cost of this product",
            },
            "Wastage": {
                "type": FloatOrNothingType,
                "display_name": "Wastage",
                "short_desc": "Wastage quantity",
                "desc": "The expected wastage quantity",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreAssemblyProductDataType") {
        addQoreFields(Fields);
    }

    #! Creates the object with a base type
    constructor(Type base_type) : HashDataType(base_type, "Cin7CoreAssemblyProductDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
