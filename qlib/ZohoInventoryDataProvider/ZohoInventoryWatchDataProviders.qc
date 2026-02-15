# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInventoryDataProvider module definition - Watch Providers

/*  ZohoInventoryWatchDataProviders.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace ZohoInventoryDataProvider {

#! Watch for new contacts
public class ZohoInventoryNewContactWatchDataProvider inherits ZohoInventoryWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-contact",
            "desc": "Watch for new contacts in Zoho Inventory",
            "type": "ZohoInventoryNewContactWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest, *hash<auto> options)
            : ZohoInventoryWatchDataProviderBase(rest, options) {}

    string getName() { return "new-contact"; }
    private string getResourcePath() { return "/contacts"; }
    private string getResourceKey() { return "contacts"; }
    private string getIdField() { return "contact_id"; }
    private string getEventName() { return EVENT_NEW_CONTACT; }
    private AbstractDataProviderType getEventDataType() { return new ZohoInventoryContactDataType(); }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private hash<auto> getFakeExampleData() {
        return ZohoInventoryWatchDataProviderBase::getFakeExampleData() + {
            "contact_name": "John Smith",
            "company_name": "Acme Corporation",
            "contact_type": "customer",
            "email": "john.smith@acme.com",
            "phone": "+1-555-123-4567",
            "status": "active",
        };
    }
}

#! Watch for new invoices
public class ZohoInventoryNewInvoiceWatchDataProvider inherits ZohoInventoryWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-invoice",
            "desc": "Watch for new invoices in Zoho Inventory",
            "type": "ZohoInventoryNewInvoiceWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest, *hash<auto> options)
            : ZohoInventoryWatchDataProviderBase(rest, options) {}

    string getName() { return "new-invoice"; }
    private string getResourcePath() { return "/invoices"; }
    private string getResourceKey() { return "invoices"; }
    private string getIdField() { return "invoice_id"; }
    private string getEventName() { return EVENT_NEW_INVOICE; }
    private AbstractDataProviderType getEventDataType() { return new ZohoInventoryInvoiceDataType(); }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private hash<auto> getFakeExampleData() {
        return ZohoInventoryWatchDataProviderBase::getFakeExampleData() + {
            "invoice_number": "INV-001",
            "customer_id": "987654321098765432",
            "customer_name": "Acme Corporation",
            "total": 1500.00,
            "status": "draft",
            "date": "2026-01-30",
            "due_date": "2026-02-28",
        };
    }
}

#! Watch for new inventory adjustments
public class ZohoInventoryNewItemAdjustmentWatchDataProvider inherits ZohoInventoryWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-item-adjustment",
            "desc": "Watch for new inventory adjustments in Zoho Inventory",
            "type": "ZohoInventoryNewItemAdjustmentWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest, *hash<auto> options)
            : ZohoInventoryWatchDataProviderBase(rest, options) {}

    string getName() { return "new-item-adjustment"; }
    private string getResourcePath() { return "/inventoryadjustments"; }
    private string getResourceKey() { return "inventory_adjustments"; }
    private string getIdField() { return "inventory_adjustment_id"; }
    private string getEventName() { return EVENT_NEW_ITEM_ADJUSTMENT; }
    private AbstractDataProviderType getEventDataType() { return new ZohoInventoryItemAdjustmentDataType(); }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private hash<auto> getFakeExampleData() {
        return ZohoInventoryWatchDataProviderBase::getFakeExampleData() + {
            "adjustment_type": "quantity",
            "reason": "Stock recount",
            "date": "2026-01-30",
            "total": 250.00,
        };
    }
}

#! Watch for new item groups
public class ZohoInventoryNewItemGroupWatchDataProvider inherits ZohoInventoryWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-item-group",
            "desc": "Watch for new item groups in Zoho Inventory",
            "type": "ZohoInventoryNewItemGroupWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest, *hash<auto> options)
            : ZohoInventoryWatchDataProviderBase(rest, options) {}

    string getName() { return "new-item-group"; }
    private string getResourcePath() { return "/itemgroups"; }
    private string getResourceKey() { return "itemgroups"; }
    private string getIdField() { return "group_id"; }
    private string getEventName() { return EVENT_NEW_ITEM_GROUP; }
    private AbstractDataProviderType getEventDataType() { return new ZohoInventoryItemGroupDataType(); }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private hash<auto> getFakeExampleData() {
        return ZohoInventoryWatchDataProviderBase::getFakeExampleData() + {
            "group_name": "Electronics",
            "unit": "pcs",
            "brand": "TechBrand",
            "manufacturer": "TechCorp Inc",
        };
    }
}

#! Watch for new items
public class ZohoInventoryNewItemWatchDataProvider inherits ZohoInventoryWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-item",
            "desc": "Watch for new items in Zoho Inventory",
            "type": "ZohoInventoryNewItemWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest, *hash<auto> options)
            : ZohoInventoryWatchDataProviderBase(rest, options) {}

    string getName() { return "new-item"; }
    private string getResourcePath() { return "/items"; }
    private string getResourceKey() { return "items"; }
    private string getIdField() { return "item_id"; }
    private string getEventName() { return EVENT_NEW_ITEM; }
    private AbstractDataProviderType getEventDataType() { return new ZohoInventoryItemDataType(); }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private hash<auto> getFakeExampleData() {
        return ZohoInventoryWatchDataProviderBase::getFakeExampleData() + {
            "name": "Widget Pro",
            "sku": "WDG-001",
            "rate": 49.99,
            "description": "Professional grade widget",
            "status": "active",
            "item_type": "inventory",
        };
    }
}

#! Watch for new packages
public class ZohoInventoryNewPackageWatchDataProvider inherits ZohoInventoryWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-package",
            "desc": "Watch for new packages in Zoho Inventory",
            "type": "ZohoInventoryNewPackageWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest, *hash<auto> options)
            : ZohoInventoryWatchDataProviderBase(rest, options) {}

    string getName() { return "new-package"; }
    private string getResourcePath() { return "/packages"; }
    private string getResourceKey() { return "packages"; }
    private string getIdField() { return "package_id"; }
    private string getEventName() { return EVENT_NEW_PACKAGE; }
    private AbstractDataProviderType getEventDataType() { return new ZohoInventoryPackageDataType(); }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private hash<auto> getFakeExampleData() {
        return ZohoInventoryWatchDataProviderBase::getFakeExampleData() + {
            "package_number": "PKG-001",
            "salesorder_id": "987654321098765432",
            "date": "2026-01-30",
            "customer_name": "Acme Corporation",
        };
    }
}

#! Watch for new purchase orders
public class ZohoInventoryNewPurchaseOrderWatchDataProvider inherits ZohoInventoryWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-purchase-order",
            "desc": "Watch for new purchase orders in Zoho Inventory",
            "type": "ZohoInventoryNewPurchaseOrderWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest, *hash<auto> options)
            : ZohoInventoryWatchDataProviderBase(rest, options) {}

    string getName() { return "new-purchase-order"; }
    private string getResourcePath() { return "/purchaseorders"; }
    private string getResourceKey() { return "purchaseorders"; }
    private string getIdField() { return "purchaseorder_id"; }
    private string getEventName() { return EVENT_NEW_PURCHASE_ORDER; }
    private AbstractDataProviderType getEventDataType() { return new ZohoInventoryPurchaseOrderDataType(); }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private hash<auto> getFakeExampleData() {
        return ZohoInventoryWatchDataProviderBase::getFakeExampleData() + {
            "purchaseorder_number": "PO-001",
            "vendor_id": "876543210987654321",
            "vendor_name": "Office Supplies Inc",
            "total": 750.00,
            "date": "2026-01-30",
            "status": "open",
        };
    }
}

#! Watch for new sales orders
public class ZohoInventoryNewSalesOrderWatchDataProvider inherits ZohoInventoryWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-sales-order",
            "desc": "Watch for new sales orders in Zoho Inventory",
            "type": "ZohoInventoryNewSalesOrderWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest, *hash<auto> options)
            : ZohoInventoryWatchDataProviderBase(rest, options) {}

    string getName() { return "new-sales-order"; }
    private string getResourcePath() { return "/salesorders"; }
    private string getResourceKey() { return "salesorders"; }
    private string getIdField() { return "salesorder_id"; }
    private string getEventName() { return EVENT_NEW_SALES_ORDER; }
    private AbstractDataProviderType getEventDataType() { return new ZohoInventorySalesOrderDataType(); }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private hash<auto> getFakeExampleData() {
        return ZohoInventoryWatchDataProviderBase::getFakeExampleData() + {
            "salesorder_number": "SO-001",
            "customer_id": "987654321098765432",
            "customer_name": "Acme Corporation",
            "total": 2500.00,
            "date": "2026-01-30",
            "status": "open",
        };
    }
}

} # namespace ZohoInventoryDataProvider
