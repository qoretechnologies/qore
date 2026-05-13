# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! EasyPost refunds collection data provider
public class EasyPostRefundsDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "refunds",
            "desc": "EasyPost refunds collection - list and search refunds",
            "type": "EasyPostRefundsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = EasyPostPaginationType::SearchOptions + {
            "id": <DataProviderOptionInfo>{
                "display_name": "Refund ID",
                "short_desc": "Filter to a single refund by ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "If supplied, retrieves the single refund with this ID.",
            },
        };
    }

    private {
        static EasyPostRefundType recordType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "refunds";
    }

    *string getDesc() {
        return "EasyPost refunds collection - list and search refunds";
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        list<hash<auto>> records = ();
        if (search_options.id) {
            auto rec = doGet(sprintf("refunds/%s", search_options.id));
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
            records = paginate("refunds", "refunds", params);
        }
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace EasyPostDataProvider
