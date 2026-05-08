# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! EasyPost shipments collection data provider
public class EasyPostShipmentsDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "shipments",
            "desc": "EasyPost shipments collection - list and search shipments",
            "type": "EasyPostShipmentsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = EasyPostPaginationType::SearchOptions + {
            "id": <DataProviderOptionInfo>{
                "display_name": "Shipment ID",
                "short_desc": "Filter to a single shipment by ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "If supplied, retrieves the single shipment with this ID instead of listing.",
            },
            "purchased": <DataProviderOptionInfo>{
                "display_name": "Purchased",
                "short_desc": "Filter to purchased shipments only",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "Set to `True` to return only purchased shipments",
                "preselected": True,
            },
            "include_children": <DataProviderOptionInfo>{
                "display_name": "Include Children",
                "short_desc": "Include child shipments",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "Whether to include child shipments in the result",
            },
        };
    }

    private {
        static EasyPostShipmentType recordType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "shipments";
    }

    *string getDesc() {
        return "EasyPost shipments collection - list and search shipments";
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        list<hash<auto>> records = ();
        if (search_options.id) {
            auto rec = doGet(sprintf("shipments/%s", search_options.id));
            if (rec) {
                records = (rec,);
            }
        } else {
            hash<auto> params = {};
            foreach string k in (("page_size", "before_id", "after_id", "start_datetime",
                                  "end_datetime", "purchased", "include_children")) {
                if (exists search_options{k}) {
                    params{k} = search_options{k};
                }
            }
            records = paginate("shipments", "shipments", params);
        }
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace EasyPostDataProvider
