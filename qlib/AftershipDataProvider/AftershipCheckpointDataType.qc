# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AftershipDataProvider module definition

/** AftershipCheckpointDataType.qc Copyright 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#! contains all public definitions in the AftershipDataProvider module
public namespace AftershipDataProvider {
#! Checkpoint data type
public class AftershipCheckpointDataType inherits HashDataType {
    public {
        const Fields = {
            "created_at": {
                "display_name": "Created At",
                "short_desc": "Record creation timestamp",
                "desc": "ISO 8601 timestamp when the checkpoint record was created",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "slug": {
                "display_name": "Courier Code",
                "short_desc": "Carrier code for this checkpoint",
                "desc": "Courier code (e.g., `fedex`, `ups`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "checkpoint_time": {
                "display_name": "Checkpoint Time",
                "short_desc": "Event timestamp",
                "desc": "ISO 8601 timestamp of the checkpoint event (with timezone in 2026-01)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "source": {
                "display_name": "Source",
                "short_desc": "Data source",
                "desc": "Source of the checkpoint data (e.g., `carrier`)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "location": {
                "display_name": "Location",
                "short_desc": "Full location description",
                "desc": "Full location description of the checkpoint event",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "city": {
                "display_name": "City",
                "short_desc": "City name",
                "desc": "City name of the checkpoint location",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "state": {
                "display_name": "State",
                "short_desc": "State or province",
                "desc": "State or province of the checkpoint location",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "zip": {
                "display_name": "ZIP Code",
                "short_desc": "Postal or ZIP code",
                "desc": "Postal or ZIP code of the checkpoint location",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "country_region": {
                "display_name": "Country Region",
                "short_desc": "ISO Alpha-3 country code",
                "desc": "ISO Alpha-3 country code (renamed from `country_iso3` in 2026-01)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "country_region_name": {
                "display_name": "Country Name",
                "short_desc": "Full country name",
                "desc": "Full country name (renamed from `country_name` in 2026-01)",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "message": {
                "display_name": "Message",
                "short_desc": "Carrier-provided status message",
                "desc": "Carrier-provided status message for this checkpoint",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tag": {
                "display_name": "Tag",
                "short_desc": "Standard status tag",
                "desc": "Standard status tag: `Pending`, `InfoReceived`, `InTransit`, `OutForDelivery`, "
                    "`AttemptFail`, `Delivered`, `AvailableForPickup`, `Exception`, `Expired`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "subtag": {
                "display_name": "Subtag",
                "short_desc": "Detailed status code",
                "desc": "Detailed status code providing more specific information about the checkpoint",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "subtag_message": {
                "display_name": "Subtag Message",
                "short_desc": "Status description",
                "desc": "Human-readable description of the status subtag",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "raw_tag": {
                "display_name": "Raw Tag",
                "short_desc": "Carrier's original tag",
                "desc": "Carrier's original unprocessed status tag",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Singleton instance
public const AftershipCheckpointDataType = new AftershipCheckpointDataType();
}
