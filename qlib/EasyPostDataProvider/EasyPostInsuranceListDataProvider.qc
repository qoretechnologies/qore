# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! EasyPost insurances collection data provider
public class EasyPostInsuranceListDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "insurances",
            "desc": "EasyPost insurances collection - list and search insurance records",
            "type": "EasyPostInsuranceListDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = EasyPostPaginationType::SearchOptions + {
            "id": <DataProviderOptionInfo>{
                "display_name": "Insurance ID",
                "short_desc": "Filter to a single insurance by ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "If supplied, retrieves the single insurance record with this ID.",
            },
        };
    }

    private {
        static EasyPostInsuranceType recordType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "insurances";
    }

    *string getDesc() {
        return "EasyPost insurances collection - list and search insurance records";
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        list<hash<auto>> records = ();
        if (search_options.id) {
            auto rec = doGet(sprintf("insurances/%s", search_options.id));
            if (rec) {
                records = (rec,);
            }
        } else {
            hash<auto> params = {};
            foreach string k in (("page_size", "before_id", "after_id", "start_datetime",
                                  "end_datetime")) {
                if (exists search_options{k}) {
                    params{k} = search_options{k};
                }
            }
            records = paginate("insurances", "insurances", params);
        }
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace EasyPostDataProvider
