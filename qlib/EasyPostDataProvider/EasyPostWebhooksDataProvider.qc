# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! EasyPost webhooks collection data provider
public class EasyPostWebhooksDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "webhooks",
            "desc": "EasyPost webhooks collection - list and search webhooks",
            "type": "EasyPostWebhooksDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = EasyPostPaginationType::SearchOptions + {
            "id": <DataProviderOptionInfo>{
                "display_name": "Webhook ID",
                "short_desc": "Filter to a single webhook by ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "If supplied, retrieves the single webhook with this ID.",
            },
        };
    }

    private {
        static EasyPostWebhookType recordType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "webhooks";
    }

    *string getDesc() {
        return "EasyPost webhooks collection - list and search webhooks";
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        list<hash<auto>> records = ();
        if (search_options.id) {
            auto rec = doGet(sprintf("webhooks/%s", search_options.id));
            if (rec) {
                records = (rec,);
            }
        } else {
            auto response = doGet("webhooks");
            records = response.webhooks ?? (response.typeCode() == NT_LIST ? response : ());
        }
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace EasyPostDataProvider
