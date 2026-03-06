# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KeapCreateNoteDataProvider.qc Keap create note action data provider

/*  KeapCreateNoteDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Request type for creating a note
public class KeapCreateNoteRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "contact_id": {
                "type": StringType,
                "display_name": "Contact ID",
                "short_desc": "The contact to add the note to",
                "desc": "The unique identifier of the contact to add the note to",
                "required": True,
            },
            "title": {
                "type": StringType,
                "display_name": "Title",
                "short_desc": "The note title",
                "desc": "The title of the note",
                "required": True,
            },
            "body": {
                "type": StringOrNothingType,
                "display_name": "Body",
                "short_desc": "The note body",
                "desc": "The body content of the note",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The note type",
                "desc": "The type of note",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Appointment"},
                    <AllowedValueInfo>{"value": "Call"},
                    <AllowedValueInfo>{"value": "Email"},
                    <AllowedValueInfo>{"value": "Fax"},
                    <AllowedValueInfo>{"value": "Letter"},
                    <AllowedValueInfo>{"value": "Other"},
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Keap create note action data provider
public class KeapCreateNoteDataProvider inherits KeapDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new note for a contact in Keap",
            "type": "KeapCreateNoteDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(KeapRestClient::KeapRestClient rest) : KeapDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new note for a contact in Keap";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KeapCreateNoteRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KeapNoteDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string cid = remove req.contact_id;
        return doPost("contacts/" + cid + "/notes", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KeapDataProvider
