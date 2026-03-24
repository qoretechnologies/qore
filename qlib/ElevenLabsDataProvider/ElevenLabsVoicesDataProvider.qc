# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElevenLabsDataProvider module definition

/** ElevenLabsVoicesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for listing voices
public class ElevenLabsVoicesDataProvider inherits ElevenLabsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "voices",
            "desc": "List available ElevenLabs voices",
            "type": "ElevenLabsVoicesDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElevenLabsVoicesRequestType();

        #! Response type
        const ResponseType = new ElevenLabsVoicesResponseType();
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
        string path = "v2/voices";
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

#! Request type for listing voices
public class ElevenLabsVoicesRequestType inherits HashDataType {
    public {
        const Fields = {
            "page_size": {
                "display_name": "Page Size",
                "short_desc": "Number of voices to return per page",
                "desc": "The number of voices to return per page",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "search": {
                "display_name": "Search",
                "short_desc": "Search term to filter voices",
                "desc": "A search term to filter voices by name",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "voice_type": {
                "display_name": "Voice Type",
                "short_desc": "Filter by voice type",
                "desc": "Filter voices by type",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Personal",
                        "value": "personal",
                        "desc": "Personal voices",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Community",
                        "value": "community",
                        "desc": "Community voices",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Default",
                        "value": "default",
                        "desc": "Default voices",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Workspace",
                        "value": "workspace",
                        "desc": "Workspace voices",
                    },
                ),
            },
            "category": {
                "display_name": "Category",
                "short_desc": "Filter by voice category",
                "desc": "Filter voices by category",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Premade",
                        "value": "premade",
                        "desc": "Premade voices",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Cloned",
                        "value": "cloned",
                        "desc": "Cloned voices",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Generated",
                        "value": "generated",
                        "desc": "Generated voices",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Professional",
                        "value": "professional",
                        "desc": "Professional voices",
                    },
                ),
            },
            "sort": {
                "display_name": "Sort",
                "short_desc": "Sort field for results",
                "desc": "The field to sort results by",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Created At",
                        "value": "created_at_unix",
                        "desc": "Sort by creation date",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Name",
                        "value": "name",
                        "desc": "Sort by voice name",
                    },
                ),
            },
            "sort_direction": {
                "display_name": "Sort Direction",
                "short_desc": "Sort direction for results",
                "desc": "The direction to sort results",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Ascending",
                        "value": "asc",
                        "desc": "Sort in ascending order",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Descending",
                        "value": "desc",
                        "desc": "Sort in descending order",
                    },
                ),
            },
        };
    }

    constructor() : HashDataType("ElevenLabsVoicesRequest") {
        addQoreFields(Fields);
    }
}

#! Response type for listing voices
public class ElevenLabsVoicesResponseType inherits HashDataType {
    public {
        const Fields = {
            "voices": {
                "display_name": "Voices",
                "short_desc": "List of voices",
                "desc": "The list of available voices",
                "type": new ListDataType("Voices", new ElevenLabsVoiceSummaryDataType(), True),
            },
            "has_more": {
                "display_name": "Has More",
                "short_desc": "Whether more results are available",
                "desc": "Indicates whether more results are available",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "next_page_token": {
                "display_name": "Next Page Token",
                "short_desc": "Token for the next page of results",
                "desc": "The token to use to retrieve the next page of results",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() : HashDataType("ElevenLabsVoicesResponse") {
        addQoreFields(Fields);
    }
}

}
