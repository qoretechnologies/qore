# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenAiDataProvider module definition

/** OpenAiDataModelProvider.qc Copyright 2024 - 2025 Qore Technologies, s.r.o.

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

#! contains all public definitions in the OpenAiDataProvider module
public namespace OpenAiDataProvider {
#! The OpenAi data provider class
public class OpenAiModelDataProvider inherits OpenAiDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "OpenAiModelDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
            "supports_children": True,
            "children_can_support_apis": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = AbstractDataProviderTypeMap."nothing";

        #! Response type
        const ResponseType = OpenAiModelDataType;
    }

    private {
        #! The model info
        hash<auto> model;

        #! Child data providers
        const ChildMap = {
            "create-response": Class::forName("OpenAiDataProvider::OpenAiModelCreateResponseDataProvider"),
            "delete-response": Class::forName("OpenAiDataProvider::OpenAiModelDeleteResponseDataProvider"),
            "get-response": Class::forName("OpenAiDataProvider::OpenAiModelGetResponseDataProvider"),
            "delete": Class::forName("OpenAiDataProvider::OpenAiModelDeleteDataProvider"),
        };
    }

    #! Creates the object from the arguments
    constructor(*RestClientIo::RestClientIo rest, hash<auto> model) : OpenAiDataProviderCommon(rest) {
        self.model = model;
        model.created = localtime(model.created);
    }

    #! Returns the data provider name
    string getName() {
        return model.id;
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return model;
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Return data provider summary info
    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(rest, model);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return cast<hash<DataProviderInfo>>({
            "name": model.id,
        }) + ProviderInfo;
    }
}

#! The model type
public class OpenAiModelDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
             "id": {
                "display_name": "Model ID",
                "short_desc": "Model ID",
                "desc": "Model ID",
                "type": AbstractDataProviderTypeMap."string",
            },
            "object": {
                "display_name": "Object Type",
                "short_desc": "The type of object",
                "desc": "The type of object",
                "type": AbstractDataProviderTypeMap."string",
            },
            "created": {
                "display_name": "Creation Timestamp",
                "short_desc": "The date and time the model was created",
                "desc": "The date and time the model was created",
                "type": AbstractDataProviderTypeMap."date",
            },
            "owned_by": {
                "display_name": "Owner",
                "short_desc": "The organization that owns the model",
                "desc": "The organization that owns the model",
                "type": AbstractDataProviderTypeMap."string",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiModel") {
        addQoreFields(Fields);
    }
}

#! Model type constant
public const OpenAiModelDataType = new OpenAiModelDataType();
}
