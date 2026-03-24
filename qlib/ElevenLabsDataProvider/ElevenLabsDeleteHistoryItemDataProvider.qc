# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElevenLabsDataProvider module definition

/** ElevenLabsDeleteHistoryItemDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ElevenLabsDataProvider module
public namespace ElevenLabsDataProvider {

#! Data provider for deleting a history item
public class ElevenLabsDeleteHistoryItemDataProvider inherits ElevenLabsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-history-item",
            "desc": "Delete an ElevenLabs history item",
            "type": "ElevenLabsDeleteHistoryItemDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElevenLabsDeleteHistoryItemRequestType();

        #! Response type
        const ResponseType = new ElevenLabsDeleteHistoryItemResponseType();
    }

    #! Creates the object from the REST client
    constructor(RestClientIo::RestClientIo rest) : ElevenLabsDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doRestCommand("DELETE", "v1/history/" + req.history_item_id);
        return {"status": "ok"};
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Request type for deleting a history item
public class ElevenLabsDeleteHistoryItemRequestType inherits HashDataType {
    public {
        const Fields = {
            "history_item_id": {
                "display_name": "History Item ID",
                "short_desc": "The ID of the history item to delete",
                "desc": "The unique identifier of the history item to delete",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "history",
            },
        };
    }

    constructor() : HashDataType("ElevenLabsDeleteHistoryItemRequest") {
        addQoreFields(Fields);
    }
}

#! Response type for deleting a history item
public class ElevenLabsDeleteHistoryItemResponseType inherits HashDataType {
    public {
        const Fields = {
            "status": {
                "display_name": "Status",
                "short_desc": "The status of the operation",
                "desc": "The status of the delete operation",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    constructor() : HashDataType("ElevenLabsDeleteHistoryItemResponse") {
        addQoreFields(Fields);
    }
}

}
