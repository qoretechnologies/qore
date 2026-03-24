# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElevenLabsDataProvider module definition

/** ElevenLabsHistoryDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for listing history items
public class ElevenLabsHistoryDataProvider inherits ElevenLabsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "history",
            "desc": "List ElevenLabs generation history items",
            "type": "ElevenLabsHistoryDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElevenLabsHistoryRequestType();

        #! Response type
        const ResponseType = new ElevenLabsHistoryResponseType();
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
        string path = "v1/history";
        if (req) {
            # Remove empty values and build query string
            hash<auto> query = map {$1.key: $1.value}, req.pairIterator(), exists $1.value;
            if (query) {
                path += "?" + (foldl $1 + "&" + $2,
                    (map sprintf("%s=%s", $1.key, encode_url($1.value.toString(), True)),
                        query.pairIterator()));
            }
        }
        return doRestCommand("GET", path).body;
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

#! Request type for listing history
public class ElevenLabsHistoryRequestType inherits HashDataType {
    public {
        const Fields = {
            "page_size": {
                "display_name": "Page Size",
                "short_desc": "Number of history items to return",
                "desc": "The number of history items to return per page",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "start_after_history_item_id": {
                "display_name": "Start After History Item ID",
                "short_desc": "Pagination cursor",
                "desc": "The history item ID to start after for pagination",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() : HashDataType("ElevenLabsHistoryRequest") {
        addQoreFields(Fields);
    }
}

#! Response type for listing history
public class ElevenLabsHistoryResponseType inherits HashDataType {
    public {
        const Fields = {
            "history": {
                "display_name": "History",
                "short_desc": "List of history items",
                "desc": "The list of generation history items",
                "type": new SoftListDataType("HistoryItems", new ElevenLabsHistoryItemDataType(), False),
            },
            "has_more": {
                "display_name": "Has More",
                "short_desc": "Whether more results are available",
                "desc": "Indicates whether more history items are available",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "last_history_item_id": {
                "display_name": "Last History Item ID",
                "short_desc": "ID of the last history item",
                "desc": "The ID of the last history item in the current page, used for pagination",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() : HashDataType("ElevenLabsHistoryResponse") {
        addQoreFields(Fields);
    }
}

}
