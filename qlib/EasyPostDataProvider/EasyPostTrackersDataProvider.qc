# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! EasyPost trackers collection data provider
public class EasyPostTrackersDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "trackers",
            "desc": "EasyPost trackers collection - list and search trackers",
            "type": "EasyPostTrackersDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = EasyPostPaginationType::SearchOptions + {
            "id": <DataProviderOptionInfo>{
                "display_name": "Tracker ID",
                "short_desc": "Filter to a single tracker by ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "If supplied, retrieves the single tracker with this ID instead of listing.",
            },
            "tracking_codes": <DataProviderOptionInfo>{
                "display_name": "Tracking Codes",
                "short_desc": "Tracking codes to filter by",
                "type": AbstractDataProviderType::get(SoftAutoListOrNothingType),
                "desc": "Filter trackers by carrier tracking codes (list of strings)",
            },
            "carrier": <DataProviderOptionInfo>{
                "display_name": "Carrier",
                "short_desc": "Filter by carrier",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter trackers by carrier",
                "ref_data": "carrier_accounts",
            },
        };
    }

    private {
        static EasyPostTrackerType recordType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "trackers";
    }

    *string getDesc() {
        return "EasyPost trackers collection - list and search trackers";
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        list<hash<auto>> records = ();
        if (search_options.id) {
            auto rec = doGet(sprintf("trackers/%s", search_options.id));
            if (rec) {
                records = (rec,);
            }
        } else {
            hash<auto> params = {};
            foreach string k in (("page_size", "before_id", "after_id", "start_datetime",
                                  "end_datetime", "tracking_codes", "carrier")) {
                if (exists search_options{k}) {
                    params{k} = search_options{k};
                }
            }
            records = paginate("trackers", "trackers", params);
        }
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace EasyPostDataProvider
