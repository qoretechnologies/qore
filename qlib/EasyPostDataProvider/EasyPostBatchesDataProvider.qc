# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Allowed values for the `sort` query parameter on the batches list endpoint
const BatchesSortAllowedValues = (
    <AllowedValueInfo>{"value": "asc", "desc": "Sort ascending"},
    <AllowedValueInfo>{"value": "desc", "desc": "Sort descending"},
);

#! EasyPost batches collection data provider
public class EasyPostBatchesDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "batches",
            "desc": "EasyPost batches collection - list and search batches",
            "type": "EasyPostBatchesDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = EasyPostPaginationType::SearchOptions + {
            "id": <DataProviderOptionInfo>{
                "display_name": "Batch ID",
                "short_desc": "Filter to a single batch by ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "If supplied, retrieves the single batch with this ID.",
            },
            "sort": <DataProviderOptionInfo>{
                "display_name": "Sort Order",
                "short_desc": "Sort order for results",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Sort order: `asc` or `desc`. Defaults to `desc`.",
                "allowed_values": BatchesSortAllowedValues,
            },
        };
    }

    private {
        static EasyPostBatchType recordType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "batches";
    }

    *string getDesc() {
        return "EasyPost batches collection - list and search batches";
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        list<hash<auto>> records = ();
        if (search_options.id) {
            auto rec = doGet(sprintf("batches/%s", search_options.id));
            if (rec) {
                records = (rec,);
            }
        } else {
            hash<auto> params = {};
            foreach string k in (("page_size", "before_id", "after_id", "start_datetime", "end_datetime", "sort")) {
                if (exists search_options{k}) {
                    params{k} = search_options{k};
                }
            }
            records = paginate("batches", "batches", params);
        }
        return new DefaultRecordIterator<hash<auto>>(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace EasyPostDataProvider
