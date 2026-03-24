# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElevenLabsDataProvider module definition

/** ElevenLabsGetHistoryItemDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for getting a single history item
public class ElevenLabsGetHistoryItemDataProvider inherits ElevenLabsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-history-item",
            "desc": "Get details for a single ElevenLabs history item",
            "type": "ElevenLabsGetHistoryItemDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElevenLabsGetHistoryItemRequestType();

        #! Response type
        const ResponseType = new ElevenLabsGetHistoryItemResponseType();
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
        return doRestCommand("GET", "v1/history/" + req.history_item_id).body;
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

#! Request type for getting a history item
public class ElevenLabsGetHistoryItemRequestType inherits HashDataType {
    public {
        const Fields = {
            "history_item_id": {
                "display_name": "History Item ID",
                "short_desc": "The ID of the history item",
                "desc": "The unique identifier of the history item to retrieve",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "history",
            },
        };
    }

    constructor() : HashDataType("ElevenLabsGetHistoryItemRequest") {
        addQoreFields(Fields);
    }
}

#! Response type for getting a history item
public class ElevenLabsGetHistoryItemResponseType inherits HashDataType {
    public {
        const Fields = {
            "history_item_id": {
                "display_name": "History Item ID",
                "short_desc": "The unique identifier of the history item",
                "desc": "The unique identifier of the history item",
                "type": AbstractDataProviderTypeMap."string",
            },
            "voice_id": {
                "display_name": "Voice ID",
                "short_desc": "The ID of the voice used",
                "desc": "The unique identifier of the voice used for generation",
                "type": AbstractDataProviderTypeMap."string",
            },
            "voice_name": {
                "display_name": "Voice Name",
                "short_desc": "The name of the voice used",
                "desc": "The name of the voice used for generation",
                "type": AbstractDataProviderTypeMap."string",
            },
            "text": {
                "display_name": "Text",
                "short_desc": "The input text",
                "desc": "The text that was used for generation",
                "type": AbstractDataProviderTypeMap."string",
            },
            "date_unix": {
                "display_name": "Date (Unix)",
                "short_desc": "Unix timestamp of the generation",
                "desc": "The Unix timestamp of when the generation was created",
                "type": AbstractDataProviderTypeMap."int",
            },
            "state": {
                "display_name": "State",
                "short_desc": "The state of the history item",
                "desc": "The current state of the history item",
                "type": AbstractDataProviderTypeMap."string",
            },
            "content_type": {
                "display_name": "Content Type",
                "short_desc": "The MIME type of the generated audio",
                "desc": "The MIME type of the generated audio content",
                "type": AbstractDataProviderTypeMap."string",
            },
            "character_count_change_from": {
                "display_name": "Character Count Change From",
                "short_desc": "Character count before generation",
                "desc": "The character count before the generation",
                "type": AbstractDataProviderTypeMap."int",
            },
            "character_count_change_to": {
                "display_name": "Character Count Change To",
                "short_desc": "Character count after generation",
                "desc": "The character count after the generation",
                "type": AbstractDataProviderTypeMap."int",
            },
        };
    }

    constructor() : HashDataType("ElevenLabsGetHistoryItemResponse") {
        addQoreFields(Fields);
    }
}

}
