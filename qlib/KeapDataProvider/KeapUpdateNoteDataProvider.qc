# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KeapUpdateNoteDataProvider.qc Keap update note action data provider

/*  KeapUpdateNoteDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a note
public class KeapUpdateNoteRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "contact_id": {
                "type": StringType,
                "display_name": "Contact ID",
                "short_desc": "The contact that owns the note",
                "desc": "The unique identifier of the contact that owns the note",
                "required": True,
            },
            "note_id": {
                "type": StringType,
                "display_name": "Note ID",
                "short_desc": "The note ID to update",
                "desc": "The unique identifier of the note to update",
                "required": True,
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The note title",
                "desc": "The new title of the note",
            },
            "body": {
                "type": StringOrNothingType,
                "display_name": "Body",
                "short_desc": "The note body",
                "desc": "The new body content of the note",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Keap update note action data provider
public class KeapUpdateNoteDataProvider inherits KeapDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing note for a contact in Keap",
            "type": "KeapUpdateNoteDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(KeapRestClient::KeapRestClient rest) : KeapDataProviderBase(rest) {
    }

    string getName() {
        return "update";
    }

    *string getDesc() {
        return "Update an existing note for a contact in Keap";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KeapUpdateNoteRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KeapNoteDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string cid = remove req.contact_id;
        string nid = remove req.note_id;
        return doPatch("contacts/" + cid + "/notes/" + nid, req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KeapDataProvider
