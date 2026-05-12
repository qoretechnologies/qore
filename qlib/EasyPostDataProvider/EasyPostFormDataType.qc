# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! EasyPost form (e.g. customs forms)
public class EasyPostFormType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "ID", "type": SoftStringOrNothingType, "desc": "Form ID (`form_xxx`)"},
            "object": {"display_name": "Object Type", "type": SoftStringOrNothingType, "desc": "Always `Form`"},
            "mode": {"display_name": "Mode", "type": SoftStringOrNothingType, "desc": "`production` or `test`"},
            "form_type": {"display_name": "Form Type", "type": SoftStringOrNothingType, "desc": "Type of form (e.g. `commercial_invoice`)"},
            "form_url": {"display_name": "Form URL", "type": SoftStringOrNothingType, "desc": "URL of the form PDF"},
            "submitted_electronically": {"display_name": "Submitted Electronically", "type": SoftBoolOrNothingType, "desc": "Whether the form was submitted electronically"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
