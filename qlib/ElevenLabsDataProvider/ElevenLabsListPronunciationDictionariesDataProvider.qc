# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElevenLabsDataProvider module definition

/** ElevenLabsListPronunciationDictionariesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for listing pronunciation dictionaries
public class ElevenLabsListPronunciationDictionariesDataProvider inherits ElevenLabsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-pronunciation-dictionaries",
            "desc": "List available ElevenLabs pronunciation dictionaries",
            "type": "ElevenLabsListPronunciationDictionariesDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElevenLabsListPronunciationDictionariesRequestType();

        #! Response type
        const ResponseType = new ElevenLabsListPronunciationDictionariesResponseType();
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
        string path = "v1/pronunciation-dictionaries";
        if (req) {
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

#! Request type for listing pronunciation dictionaries
public class ElevenLabsListPronunciationDictionariesRequestType inherits HashDataType {
    public {
        const Fields = {
            "page_size": {
                "display_name": "Page Size",
                "short_desc": "Number of dictionaries to return per page",
                "desc": "The number of pronunciation dictionaries to return per page",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "cursor": {
                "display_name": "Cursor",
                "short_desc": "Cursor for pagination",
                "desc": "The cursor to use for pagination to retrieve the next page of results",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() : HashDataType("ElevenLabsListPronunciationDictionariesRequest") {
        addQoreFields(Fields);
    }
}

#! Response type for listing pronunciation dictionaries
public class ElevenLabsListPronunciationDictionariesResponseType inherits HashDataType {
    public {
        const Fields = {
            "pronunciation_dictionaries": {
                "display_name": "Pronunciation Dictionaries",
                "short_desc": "List of pronunciation dictionaries",
                "desc": "The list of pronunciation dictionaries",
                "type": new SoftListDataType("PronunciationDictionaries",
                    new ElevenLabsPronunciationDictionarySummaryDataType(), False),
            },
            "has_more": {
                "display_name": "Has More",
                "short_desc": "Whether more results are available",
                "desc": "Indicates whether more results are available",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "next_cursor": {
                "display_name": "Next Cursor",
                "short_desc": "Cursor for the next page",
                "desc": "The cursor to use to retrieve the next page of results",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() : HashDataType("ElevenLabsListPronunciationDictionariesResponse") {
        addQoreFields(Fields);
    }
}

}
