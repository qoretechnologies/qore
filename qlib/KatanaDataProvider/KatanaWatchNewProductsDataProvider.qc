# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KatanaWatchNewProductsDataProvider.qc Polling watch for newly created products

/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

#! Watch provider: fires once per newly created product.
public class KatanaWatchNewProductsDataProvider inherits KatanaWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_PRODUCTS,
            "desc": "Fires when a new product is created in Katana.",
            "type": "KatanaWatchNewProductsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(*RestClientIo::RestClientIo rest, *hash<auto> options)
            : KatanaWatchDataProviderBase(rest, options) {
    }

    string getName() { return ProviderInfo.name; }
    string getEndpoint() { return "products"; }
    string getEventName() { return EVENT_NEW_PRODUCTS; }
    *string getTimestampField() { return "created_at"; }

    AbstractDataProviderType getRecordDataType() {
        return new KatanaProductDataType();
    }

    list<hash<auto>> fetchRecords(date since) {
        hash<auto> params = {
            "created_at_min": KatanaDataProviderBase::formatUtcIso8601(since),
            "limit": MaxPageSize,
        };
        try {
            return doGetList(getEndpoint(), params);
        } catch (hash<ExceptionInfo> ex) {
            LoggerWrapper::error(sprintf("Error fetching new products: %s: %s", ex.err, ex.desc));
            return ();
        }
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 5001,
            "name": "Sample Widget",
            "uom": "pcs",
            "category_name": "Widgets",
            "is_producible": True,
            "is_sellable": True,
            "created_at": "2026-05-01T12:00:00Z",
            "updated_at": "2026-05-01T12:00:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
