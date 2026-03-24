# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElevenLabsDataProvider module definition

/** ElevenLabsAddPronunciationRulesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Data provider for adding rules to a pronunciation dictionary
public class ElevenLabsAddPronunciationRulesDataProvider inherits ElevenLabsDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "add-pronunciation-rules",
            "desc": "Add pronunciation rules to an existing ElevenLabs pronunciation dictionary",
            "type": "ElevenLabsAddPronunciationRulesDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElevenLabsAddPronunciationRulesRequestType();

        #! Response type
        const ResponseType = new ElevenLabsAddPronunciationRulesResponseType();
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
        string dictionary_id = remove req.dictionary_id;
        return doRestCommand("POST",
            "v1/pronunciation-dictionaries/" + dictionary_id + "/add-rules", req).body;
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

#! Request type for adding pronunciation rules
public class ElevenLabsAddPronunciationRulesRequestType inherits HashDataType {
    public {
        const Fields = {
            "dictionary_id": {
                "display_name": "Dictionary ID",
                "short_desc": "The ID of the pronunciation dictionary",
                "desc": "The unique identifier of the pronunciation dictionary to add rules to",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "ref_data": "pronunciation_dictionaries",
            },
            "rules": {
                "display_name": "Rules",
                "short_desc": "Pronunciation rules to add",
                "desc": "A list of pronunciation rules to add to the dictionary. Each rule has a `type` "
                    "(`alias` or `phoneme`), a `string_to_replace`, and either an `alias` or a `phoneme` "
                    "with `alphabet`",
                "type": new SoftListDataType("PronunciationRules",
                    new ElevenLabsPronunciationRuleType(), True),
                "required": True,
            },
        };
    }

    constructor() : HashDataType("ElevenLabsAddPronunciationRulesRequest") {
        addQoreFields(Fields);
    }
}

#! Response type for adding pronunciation rules
public class ElevenLabsAddPronunciationRulesResponseType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Dictionary ID",
                "short_desc": "The ID of the pronunciation dictionary",
                "desc": "The unique identifier of the pronunciation dictionary",
                "type": AbstractDataProviderTypeMap."string",
            },
            "version_id": {
                "display_name": "Version ID",
                "short_desc": "The new version ID after adding rules",
                "desc": "The new version ID of the pronunciation dictionary after adding rules",
                "type": AbstractDataProviderTypeMap."string",
            },
            "version_rules_num": {
                "display_name": "Total Rules",
                "short_desc": "Total number of rules in the new version",
                "desc": "The total number of rules in the new dictionary version",
                "type": AbstractDataProviderTypeMap."int",
            },
        };
    }

    constructor() : HashDataType("ElevenLabsAddPronunciationRulesResponse") {
        addQoreFields(Fields);
    }
}

}
