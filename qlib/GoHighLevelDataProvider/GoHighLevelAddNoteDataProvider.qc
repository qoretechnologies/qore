# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GoHighLevelAddNoteDataProvider.qc GoHighLevel add note action data provider

/*  GoHighLevelAddNoteDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

%modern

#! Contains all public definitions in the GoHighLevelDataProvider module
public namespace GoHighLevelDataProvider {

#! Request data type for the add-note action
public class GoHighLevelAddNoteRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "contact_id": {
                "type": StringType,
                "display_name": "Contact ID",
                "short_desc": "The contact ID to add the note to",
                "desc": "The contact ID to add the note to",
                "required": True,
            },
            "body": {
                "type": StringType,
                "display_name": "Body",
                "short_desc": "The note body/content",
                "desc": "The note body/content",
                "required": True,
            },
            "userId": {
                "type": StringOrNothingType,
                "display_name": "User ID",
                "short_desc": "User ID of the note author",
                "desc": "User ID of the note author",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! GoHighLevel add note action data provider
public class GoHighLevelAddNoteDataProvider inherits GoHighLevelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "add-note",
            "desc": "Add a note to a contact in GoHighLevel",
            "type": "GoHighLevelAddNoteDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(GoHighLevelRestClient::GoHighLevelRestClient rest) : GoHighLevelDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "add-note";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Add a note to a contact in GoHighLevel";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new GoHighLevelAddNoteRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string contact_id = remove req.contact_id;
        return doPost("contacts/" + contact_id + "/notes", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GoHighLevelDataProvider
