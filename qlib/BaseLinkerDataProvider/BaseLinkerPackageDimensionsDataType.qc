# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerPackageDimensionsDataType.qc package dimension entry
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! A single parcel's dimensions / insurance for createPackage
public class BaseLinkerPackageDimensionsDataType inherits HashDataType {
    public {
        const Fields = {
            "weight": {
                "display_name": "Weight (kg)",
                "short_desc": "Parcel weight",
                "type": SoftFloatType,
                "required": True,
                "desc": "Weight of the parcel in kilograms.",
            },
            "height": {
                "display_name": "Height (cm)",
                "short_desc": "Parcel height",
                "type": SoftFloatOrNothingType,
            },
            "width": {
                "display_name": "Width (cm)",
                "short_desc": "Parcel width",
                "type": SoftFloatOrNothingType,
            },
            "length": {
                "display_name": "Length (cm)",
                "short_desc": "Parcel length",
                "type": SoftFloatOrNothingType,
            },
            "insurance": {
                "display_name": "Insurance Amount",
                "short_desc": "Declared value for insurance",
                "type": SoftFloatOrNothingType,
                "desc": "Insured value in the order currency.",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerPackageDimensionsDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
