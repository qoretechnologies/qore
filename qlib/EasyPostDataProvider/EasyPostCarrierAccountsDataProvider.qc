# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! EasyPost carrier accounts collection data provider
public class EasyPostCarrierAccountsDataProvider inherits EasyPostDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "carrier-accounts",
            "desc": "EasyPost carrier accounts collection - list and search carrier accounts",
            "type": "EasyPostCarrierAccountsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = EasyPostPaginationType::SearchOptions + {
            "id": <DataProviderOptionInfo>{
                "display_name": "Carrier Account ID",
                "short_desc": "Filter to a single carrier account by ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "If supplied, retrieves the single carrier account with this ID.",
            },
        };
    }

    private {
        static EasyPostCarrierAccountType recordType();
    }

    constructor(*RestClientIo::RestClientIo rest) : EasyPostDataProviderBase(rest) {
    }

    string getName() {
        return "carrier-accounts";
    }

    *string getDesc() {
        return "EasyPost carrier accounts collection - list and search carrier accounts";
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        list<hash<auto>> records = ();
        if (search_options.id) {
            auto rec = doGet(sprintf("carrier_accounts/%s", search_options.id));
            if (rec) {
                records = (rec,);
            }
        } else {
            auto response = doGet("carrier_accounts");
            records = response.typeCode() == NT_LIST ? response : (response.carrier_accounts ?? ());
        }
        return new DefaultRecordIterator<hash<auto>>(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace EasyPostDataProvider
