# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! EasyPost postage label
public class EasyPostPostageLabelType inherits HashDataType {
    public {
        const Fields = {
            "id": {"display_name": "ID", "type": SoftStringOrNothingType, "desc": "Postage label ID (`pl_xxx`)"},
            "object": {"display_name": "Object Type", "type": SoftStringOrNothingType, "desc": "Always `PostageLabel`"},
            "mode": {"display_name": "Mode", "type": SoftStringOrNothingType, "desc": "`production` or `test`"},
            "label_date": {"display_name": "Label Date", "type": SoftDateOrNothingType, "desc": "Label print date"},
            "label_resolution": {"display_name": "Label Resolution", "type": SoftIntOrNothingType, "desc": "Label resolution in DPI"},
            "label_size": {"display_name": "Label Size", "type": SoftStringOrNothingType, "desc": "Label size (e.g. `4x6`)"},
            "label_type": {"display_name": "Label Type", "type": SoftStringOrNothingType, "desc": "Label type"},
            "label_url": {"display_name": "Label URL", "type": SoftStringOrNothingType, "desc": "Primary label URL"},
            "label_pdf_url": {"display_name": "Label PDF URL", "type": SoftStringOrNothingType, "desc": "PDF label URL"},
            "label_zpl_url": {"display_name": "Label ZPL URL", "type": SoftStringOrNothingType, "desc": "ZPL label URL"},
            "label_epl2_url": {"display_name": "Label EPL2 URL", "type": SoftStringOrNothingType, "desc": "EPL2 label URL"},
            "label_file_type": {"display_name": "Label File Type", "type": SoftStringOrNothingType, "desc": "Label MIME type"},
            "integrated_form": {"display_name": "Integrated Form", "type": SoftStringOrNothingType, "desc": "Integrated form type"},
            "created_at": {"display_name": "Created At", "type": SoftDateOrNothingType, "desc": "Creation timestamp"},
            "updated_at": {"display_name": "Updated At", "type": SoftDateOrNothingType, "desc": "Last-update timestamp"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
