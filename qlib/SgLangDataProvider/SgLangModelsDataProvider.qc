# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SgLangDataProvider module definition

/** SgLangModelsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SgLangDataProvider module
public namespace SgLangDataProvider {
#! The SGLang models data provider
public class SgLangModelsDataProvider inherits SgLangDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "models",
            "type": "SgLangModelsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Response type
        const ResponseType = SgLangModelsResponseType;
    }

    #! Creates the object from the arguments
    constructor(*RestClientIo::RestClientIo rest) : SgLangDataProviderCommon(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doRestCommand("GET", "models").body;
    }

    #! Returns the description of a successful request message, if any
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return;
    }

    #! Returns the description of a response message, if this object represents a response message
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! The model data type
public class SgLangModelDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "id": {
                "display_name": "Model ID",
                "short_desc": "The model identifier",
                "desc": "The model identifier",
                "type": AbstractDataProviderTypeMap."string",
            },
            "object": {
                "display_name": "Object",
                "short_desc": "The object type",
                "desc": "The object type, always `model`",
                "type": AbstractDataProviderTypeMap."string",
            },
            "created": {
                "display_name": "Created",
                "short_desc": "The creation timestamp",
                "desc": "The Unix timestamp when the model was created",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "owned_by": {
                "display_name": "Owned By",
                "short_desc": "The model owner",
                "desc": "The organization or entity that owns the model",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("SgLangModel") {
        addQoreFields(Fields);
    }
}

#! Constant for the model data type
public const SgLangModelDataType = new SgLangModelDataType();

#! The models list response type
public class SgLangModelsResponseType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "object": {
                "display_name": "Object",
                "short_desc": "The object type",
                "desc": "The object type, always `list`",
                "type": AbstractDataProviderTypeMap."string",
            },
            "data": {
                "display_name": "Models",
                "short_desc": "List of available models",
                "desc": "List of available models on the SGLang server",
                "type": new ListDataType("SgLangModelsList", SgLangModelDataType),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("SgLangModelsResponse") {
        addQoreFields(Fields);
    }
}

#! The models response type constant
public const SgLangModelsResponseType = new SgLangModelsResponseType();
}
