# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerProductImageSlotDataType.qc single image slot sub-type
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

public namespace BaseLinkerDataProvider {

#! One BaseLinker inventory-product image slot
/** BaseLinker products support up to 16 image slots (indices `0`–`15`). Each
    slot holds either an HTTPS URL (prefixed with `url:`), a base64-encoded
    image (prefixed with `data:`), or an empty string to clear the slot.
*/
public class BaseLinkerProductImageSlotDataType inherits HashDataType {
    public {
        const Fields = {
            "slot": {
                "display_name": "Slot Index",
                "short_desc": "Image slot index (0-15)",
                "type": SoftIntType,
                "required": True,
                "desc": "Image slot index; BaseLinker supports slots `0` through `15`.",
            },
            "value": {
                "display_name": "Value",
                "short_desc": "URL, base64 data, or empty string to clear",
                "type": SoftStringType,
                "required": True,
                "desc": "Slot content. Use `url:<https-url>` to reference an external image, "
                    "`data:<base64>` to upload inline, or `` (empty string) to clear the slot.",
            },
        };
    }
    constructor() : HashDataType("BaseLinkerProductImageSlotDataType", Fields) {}
}

} # namespace BaseLinkerDataProvider
