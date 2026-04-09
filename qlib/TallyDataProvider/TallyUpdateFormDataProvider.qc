# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TallyDataProvider module definition

/** TallyUpdateFormDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the TallyDataProvider module
public namespace TallyDataProvider {
#! Request type for updating a form
public class TallyUpdateFormRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Form ID",
                "short_desc": "The form ID to update",
                "desc": "The unique identifier of the form to update",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Updated form name",
                "desc": "The new display name for the form",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Updated form status",
                "desc": "The new status for the form",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "BLANK", "display_name": "Blank"},
                    <AllowedValueInfo>{"value": "DRAFT", "display_name": "Draft"},
                    <AllowedValueInfo>{"value": "PUBLISHED", "display_name": "Published"},
                    <AllowedValueInfo>{"value": "DELETED", "display_name": "Deleted"},
                ),
            },
            "blocks": {
                "display_name": "Blocks",
                "short_desc": "Updated form blocks",
                "desc": "Updated list of form content blocks",
                "type": new ListDataType(new TallyBlockDataType(), True),
            },
            "settings": {
                "display_name": "Settings",
                "short_desc": "Updated form settings",
                "desc": "Updated form configuration",
                "type": new TallyFormSettingsDataType(AutoHashOrNothingType),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public const TallyUpdateFormRequestDataType = new TallyUpdateFormRequestDataType();

#! Data provider for updating a form
public class TallyUpdateFormDataProvider inherits TallyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-form",
            "type": "TallyUpdateFormDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        const RequestType = TallyUpdateFormRequestDataType;
        const ResponseType = TallyFormDataType;
    }

    constructor(RestClientIo::RestClientIo rest) : TallyDataProviderBase(rest) {}
    constructor(*hash<auto> options) : TallyDataProviderBase(options) {}
    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.id;
        hash<auto> body = req - "id";
        return doRestCommand("PATCH", "forms/" + id, body).body;
    }
}
}
