# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! ScanForm event payload (<tt>scan_form.created</tt> / <tt>scan_form.updated</tt>)
/** Reuses <tt>EasyPostScanFormType</tt> defined in <tt>EasyPostBatchDataType.qc</tt> for the <tt>result</tt> field. */
public class EasyPostScanFormEventType inherits HashDataType {
    public {
        const Fields = EasyPostBaseEventType::Fields - "result" + {
            "result": {"display_name": "Result", "type": new EasyPostScanFormType(), "desc": "ScanForm payload"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
