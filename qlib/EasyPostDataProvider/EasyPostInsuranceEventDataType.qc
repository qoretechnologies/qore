# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! Insurance event payload (<tt>insurance.purchased</tt>)
public class EasyPostInsuranceEventType inherits HashDataType {
    public {
        const Fields = EasyPostBaseEventType::Fields - "result" + {
            "result": {"display_name": "Result", "type": new EasyPostInsuranceType(), "desc": "Insurance payload"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
