# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! ScanForm event payload (`scan_form.created` / `scan_form.updated`)
/** Reuses `EasyPostScanFormType` defined in `EasyPostBatchDataType.qc` for the `result` field. */
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
