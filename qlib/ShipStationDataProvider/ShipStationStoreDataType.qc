# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationStoreDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShipStationDataProvider module
public namespace ShipStationDataProvider {

#! ShipStation store record type
public class ShipStationStoreDataType inherits HashDataType {
    private {
        const Fields = {
            "storeId": {
                "type": IntType,
                "display_name": "Store ID",
                "short_desc": "Unique store identifier",
                "desc": "The unique identifier for this store",
            },
            "storeName": {
                "type": StringType,
                "display_name": "Store Name",
                "short_desc": "Name of the store",
                "desc": "The display name of the store",
                "example_value": "My Shopify Store",
            },
            "marketplaceId": {
                "type": IntOrNothingType,
                "display_name": "Marketplace ID",
                "short_desc": "Marketplace type ID",
                "desc": "The ID of the marketplace type (e.g., Shopify, Amazon)",
            },
            "marketplaceName": {
                "type": StringOrNothingType,
                "display_name": "Marketplace Name",
                "short_desc": "Marketplace type name",
                "desc": "The name of the marketplace platform",
                "example_value": "Shopify",
            },
            "accountName": {
                "type": StringOrNothingType,
                "display_name": "Account Name",
                "short_desc": "Account name",
                "desc": "The account name for this store",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "Store email",
                "desc": "The email address associated with this store",
            },
            "integrationUrl": {
                "type": StringOrNothingType,
                "display_name": "Integration URL",
                "short_desc": "Store URL",
                "desc": "The URL of the store",
            },
            "active": {
                "type": BoolOrNothingType,
                "display_name": "Active",
                "short_desc": "Is store active",
                "desc": "Whether this store is active",
            },
            "companyName": {
                "type": StringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "Company name",
                "desc": "The company name for this store",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "The phone number for this store",
            },
            "publicEmail": {
                "type": StringOrNothingType,
                "display_name": "Public Email",
                "short_desc": "Public email",
                "desc": "The public-facing email for this store",
            },
            "website": {
                "type": StringOrNothingType,
                "display_name": "Website",
                "short_desc": "Website URL",
                "desc": "The website URL for this store",
            },
            "refreshDate": {
                "type": DateOrNothingType,
                "display_name": "Refresh Date",
                "short_desc": "Last refresh date",
                "desc": "The date when the store was last refreshed",
            },
            "lastRefreshAttempt": {
                "type": DateOrNothingType,
                "display_name": "Last Refresh Attempt",
                "short_desc": "Last refresh attempt",
                "desc": "The date of the last refresh attempt",
            },
            "createDate": {
                "type": DateOrNothingType,
                "display_name": "Create Date",
                "short_desc": "When store was created",
                "desc": "The date this store was created in ShipStation",
            },
            "modifyDate": {
                "type": DateOrNothingType,
                "display_name": "Modify Date",
                "short_desc": "When store was modified",
                "desc": "The date this store was last modified",
            },
            "autoRefresh": {
                "type": BoolOrNothingType,
                "display_name": "Auto Refresh",
                "short_desc": "Auto-refresh enabled",
                "desc": "Whether automatic refresh is enabled for this store",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ShipStationDataProvider
