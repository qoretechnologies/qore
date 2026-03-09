# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareGetInventoryCountDataProvider.qc Square get inventory count action

/*  SquareGetInventoryCountDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for getting inventory counts
public class SquareGetInventoryCountRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "catalog_object_id": {
                "type": StringType,
                "display_name": "Catalog Object ID",
                "short_desc": "The item variation ID (required)",
                "desc": "The ID of the catalog item variation to get inventory counts for",
                "required": True,
            },
            "location_ids": {
                "type": AutoListOrNothingType,
                "display_name": "Location IDs",
                "short_desc": "Filter by locations",
                "desc": "Optional list of location IDs to filter counts by",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for getting inventory counts (list of counts)
public class SquareGetInventoryCountResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "counts": {
                "type": AutoListOrNothingType,
                "display_name": "Counts",
                "short_desc": "The inventory counts",
                "desc": "List of inventory count records for the requested catalog object",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Square get inventory count action data provider
public class SquareGetInventoryCountDataProvider inherits SquareDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-counts",
            "desc": "Get inventory counts for a catalog item variation in Square",
            "type": "SquareGetInventoryCountDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(SquareRestClient::SquareRestClient rest) : SquareDataProviderBase(rest) {
    }

    string getName() {
        return "get-counts";
    }

    *string getDesc() {
        return "Get inventory counts for a catalog item variation in Square";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SquareGetInventoryCountRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new SquareGetInventoryCountResponseDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {
            "catalog_object_ids": (req.catalog_object_id,),
        };
        if (req.location_ids) {
            body.location_ids = req.location_ids;
        }

        hash<auto> result = doPost("inventory/counts/batch-retrieve", body);
        return {
            "counts": result.counts ?? (),
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquareDataProvider
