# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore EasyPostDataProvider module definition

#! Contains all public definitions in the EasyPostDataProvider module
public namespace EasyPostDataProvider {

#! EasyPost fee
public class EasyPostFeeType inherits HashDataType {
    public {
        const Fields = {
            "type": {"display_name": "Type", "type": SoftStringOrNothingType, "desc": "Fee type (e.g. `LabelFee`, `InsuranceFee`)"},
            "amount": {"display_name": "Amount", "type": SoftStringOrNothingType, "desc": "Fee amount as a string"},
            "charged": {"display_name": "Charged", "type": SoftBoolOrNothingType, "desc": "Whether the fee was charged"},
            "refunded": {"display_name": "Refunded", "type": SoftBoolOrNothingType, "desc": "Whether the fee was refunded"},
        };
    }
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace EasyPostDataProvider
