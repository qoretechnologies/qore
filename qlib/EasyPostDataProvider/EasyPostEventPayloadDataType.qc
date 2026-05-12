# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! EasyPost webhook delivery payload record
public class EasyPostEventPayloadType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "ID", "type": SoftStringOrNothingType, "desc": "Payload ID (`payload_xxx`)"},
            "object": {"display_name": "Object Type", "type": SoftStringOrNothingType, "desc": "Always `Payload`"},
            "mode": {"display_name": "Mode", "type": SoftStringOrNothingType, "desc": "`production` or `test`"},
            "request_url": {"display_name": "Request URL", "type": SoftStringOrNothingType, "desc": "URL the webhook request was sent to"},
            "request_headers": {"display_name": "Request Headers", "type": AutoHashOrNothingType, "desc": "Headers sent with the webhook request"},
            "request_body": {"display_name": "Request Body", "type": SoftStringOrNothingType, "desc": "Body sent with the webhook request"},
            "response_code": {"display_name": "Response Code", "type": SoftIntOrNothingType, "desc": "HTTP status code returned by the webhook server"},
            "response_headers": {"display_name": "Response Headers", "type": AutoHashOrNothingType, "desc": "Headers returned by the webhook server"},
            "response_body": {"display_name": "Response Body", "type": SoftStringOrNothingType, "desc": "Body returned by the webhook server"},
            "total_time": {"display_name": "Total Time", "type": SoftIntOrNothingType, "desc": "Total time the webhook delivery took (ms)"},
            "created_at": {"display_name": "Created At", "type": SoftDateOrNothingType, "desc": "Creation timestamp"},
            "updated_at": {"display_name": "Updated At", "type": SoftDateOrNothingType, "desc": "Last-update timestamp"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }

    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! Response type for `GET /events/:id/payloads` (list wrapper)
public class EasyPostEventPayloadsResponseType inherits HashDataType {
    public {
        const Fields = {
            "payloads": {
                "display_name": "Payloads",
                "type": new ListDataType("EasyPostEventPayloadList", new EasyPostEventPayloadType(), True),
                "desc": "List of webhook delivery payloads for the event",
            },
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
