# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchUpdatedProductsDataProvider.qc Polling watch for updated products

/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Watch provider: fires when a product is updated.
public class KatanaWatchUpdatedProductsDataProvider inherits KatanaWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_UPD_PRODUCTS,
            "desc": "Fires when a product is updated in Katana.",
            "type": "KatanaWatchUpdatedProductsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWatchDataProviderBase(rest, options) {
    }

    string getName() { return ProviderInfo.name; }
    string getEndpoint() { return "products"; }
    string getEventName() { return EVENT_UPD_PRODUCTS; }

    AbstractDataProviderType getRecordDataType() {
        return new KatanaProductDataType();
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 5001,
            "name": "Sample Widget",
            "uom": "pcs",
            "category_name": "Widgets",
            "created_at": "2026-05-01T12:00:00Z",
            "updated_at": "2026-05-03T09:00:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
