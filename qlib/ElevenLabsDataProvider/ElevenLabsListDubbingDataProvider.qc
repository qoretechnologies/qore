# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElevenLabsDataProvider module definition

/** ElevenLabsListDubbingDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for listing dubbing projects
public class ElevenLabsListDubbingDataProvider inherits ElevenLabsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "list-dubbing",
            "desc": "List ElevenLabs dubbing projects",
            "type": "ElevenLabsListDubbingDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElevenLabsListDubbingRequestType();

        #! Response type
        const ResponseType = new ElevenLabsListDubbingResponseType();
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
        string path = "v1/dubbing";
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

#! Request type for listing dubbing projects
public class ElevenLabsListDubbingRequestType inherits HashDataType {
    public {
        const Fields = {
            "page_size": {
                "display_name": "Page Size",
                "short_desc": "Number of dubbing projects to return per page",
                "desc": "The number of dubbing projects to return per page (1-200, default: 100)",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "cursor": {
                "display_name": "Cursor",
                "short_desc": "Pagination cursor for the next page",
                "desc": "Cursor for fetching the next page of results",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "dubbing_status": {
                "display_name": "Dubbing Status",
                "short_desc": "Filter by dubbing status",
                "desc": "Filter dubbing projects by their processing status",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Dubbing",
                        "value": "dubbing",
                        "desc": "Currently being processed",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Dubbed",
                        "value": "dubbed",
                        "desc": "Successfully completed",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Failed",
                        "value": "failed",
                        "desc": "Processing failed",
                    },
                ),
            },
            "filter_by_creator": {
                "display_name": "Filter by Creator",
                "short_desc": "Filter by who created the dubbing project",
                "desc": "Filter dubbing projects by creator",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "All",
                        "value": "all",
                        "desc": "Show all dubbing projects",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Personal",
                        "value": "personal",
                        "desc": "Show only your dubbing projects",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Others",
                        "value": "others",
                        "desc": "Show only others' dubbing projects",
                    },
                ),
            },
        };
    }

    constructor() : HashDataType("ElevenLabsListDubbingRequest") {
        addQoreFields(Fields);
    }
}

#! Dubbing summary data type for list responses
public class ElevenLabsDubbingSummaryDataType inherits HashDataType {
    public {
        const Fields = {
            "dubbing_id": {
                "display_name": "Dubbing ID",
                "short_desc": "The unique identifier of the dubbing project",
                "desc": "The unique identifier of the dubbing project",
                "type": AbstractDataProviderTypeMap."string",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "The name of the dubbing project",
                "desc": "The name of the dubbing project",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "status": {
                "display_name": "Status",
                "short_desc": "The current status of the dubbing",
                "desc": "The current processing status of the dubbing project",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "target_languages": {
                "display_name": "Target Languages",
                "short_desc": "List of target languages",
                "desc": "The list of target languages for the dubbing project",
                "type": new SoftListDataType(AbstractDataProviderTypeMap."string", False),
            },
        };
    }

    constructor() : HashDataType("ElevenLabsDubbingSummary") {
        addQoreFields(Fields);
    }
}

#! Response type for listing dubbing projects
public class ElevenLabsListDubbingResponseType inherits HashDataType {
    public {
        const Fields = {
            "dubs": {
                "display_name": "Dubs",
                "short_desc": "List of dubbing projects",
                "desc": "The list of dubbing projects",
                "type": new SoftListDataType("DubbingProjects", new ElevenLabsDubbingSummaryDataType(), False),
            },
            "has_more": {
                "display_name": "Has More",
                "short_desc": "Whether more results are available",
                "desc": "Indicates whether more dubbing projects are available",
                "type": AbstractDataProviderTypeMap."*bool",
            },
            "next_cursor": {
                "display_name": "Next Cursor",
                "short_desc": "Cursor for the next page",
                "desc": "Cursor to use for fetching the next page of results",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() : HashDataType("ElevenLabsListDubbingResponse") {
        addQoreFields(Fields);
    }
}

}
