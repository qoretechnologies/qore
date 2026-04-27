# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerOrderSourcesDataProvider.qc BaseLinker order sources collection
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker order sources data provider (`getOrderSources`)
/** BaseLinker returns sources as `{sources: {source_type: {id: name, ...}}}`,
    where `source_type` is `personal`, `shop`, `order_return`, or a per-marketplace
    code such as `allegro`, `ebay`, `amazon`, `ceneo`, `emag`, etc. This provider
    flattens each entry into a `{source_type, source_id, source_name}` record.
*/
public class BaseLinkerOrderSourcesDataProvider inherits BaseLinkerDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "order-sources",
            "desc": "List configured BaseLinker order sources (`getOrderSources`).",
            "type": "BaseLinkerOrderSourcesDataProvider",
            "has_record": True,
            "supports_read": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        const SearchOptions = {};
    }

    constructor(*RestClientIo::RestClientIo rest) : BaseLinkerDataProviderBase(rest) {
    }
    constructor(BaseLinkerRestClient::BaseLinkerRestConnection conn)
            : BaseLinkerDataProviderBase(conn) {
    }
    constructor(*hash<auto> options) : BaseLinkerDataProviderBase(options) {
    }

    string getName() { return "order-sources"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return (new BaseLinkerOrderSourceDataType()).getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> response = callMethod("getOrderSources", NOTHING);
        list<auto> records = ();
        *hash<auto> sources = response.sources;
        foreach string source_type in (keys sources) {
            auto by_id = sources{source_type};
            if (by_id.typeCode() != NT_HASH) {
                continue;
            }
            foreach string sid in (keys by_id) {
                records += {
                    "source_type": source_type,
                    "source_id": sid,
                    "source_name": string(by_id{sid}),
                };
            }
        }
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond,
            search_options, (new BaseLinkerOrderSourceDataType()).getFields());
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
