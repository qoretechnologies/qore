# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Batch event payload (`batch.created` / `batch.updated`)
public class EasyPostBatchEventType inherits HashDataType {
    public {
        const Fields = EasyPostBaseEventType::Fields - "result" + {
            "result": {"display_name": "Result", "type": new EasyPostBatchType(), "desc": "Batch payload"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
