# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Common pagination/search options used by EasyPost list endpoints
public class EasyPostPaginationType inherits HashDataType {
    public {
        const Fields = {
            "page_size": {
                "display_name": "Page Size",
                "short_desc": "Number of records per page (max 100)",
                "desc": "Number of records to retrieve per page (1-100). Defaults to 25.",
                "type": SoftIntOrNothingType,
            },
            "before_id": {
                "display_name": "Before ID",
                "short_desc": "Return records created before this ID",
                "desc": "Return records created before the record with this ID (descending pagination)",
                "type": SoftStringOrNothingType,
            },
            "after_id": {
                "display_name": "After ID",
                "short_desc": "Return records created after this ID",
                "desc": "Return records created after the record with this ID (ascending pagination)",
                "type": SoftStringOrNothingType,
            },
            "start_datetime": {
                "display_name": "Start Datetime",
                "short_desc": "Earliest creation datetime",
                "desc": "Filter records created on or after this datetime (ISO 8601)",
                "type": SoftDateOrNothingType,
            },
            "end_datetime": {
                "display_name": "End Datetime",
                "short_desc": "Latest creation datetime",
                "desc": "Filter records created on or before this datetime (ISO 8601)",
                "type": SoftDateOrNothingType,
            },
        };

        #! Common pagination search options (DataProviderOptionInfo form)
        const SearchOptions = {
            "page_size": <DataProviderOptionInfo>{
                "display_name": "Page Size",
                "short_desc": "Number of records per page (max 100)",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Number of records to retrieve per page (1-100). Defaults to 25.",
            },
            "before_id": <DataProviderOptionInfo>{
                "display_name": "Before ID",
                "short_desc": "Return records created before this ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Return records created before the record with this ID (descending pagination)",
            },
            "after_id": <DataProviderOptionInfo>{
                "display_name": "After ID",
                "short_desc": "Return records created after this ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Return records created after the record with this ID (ascending pagination)",
            },
            "start_datetime": <DataProviderOptionInfo>{
                "display_name": "Start Datetime",
                "short_desc": "Earliest creation datetime",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Filter records created on or after this datetime (ISO 8601)",
            },
            "end_datetime": <DataProviderOptionInfo>{
                "display_name": "End Datetime",
                "short_desc": "Latest creation datetime",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Filter records created on or before this datetime (ISO 8601)",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
