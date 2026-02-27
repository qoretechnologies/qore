# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksImportWorkspaceDataProvider class definition

/*  DatabricksImportWorkspaceDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DatabricksDataProvider module
public namespace DatabricksDataProvider {

#! Request data type for the import-workspace action
public class DatabricksImportWorkspaceRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "path": {
                "type": StringType,
                "display_name": "Path",
                "short_desc": "The workspace path to import to",
                "desc": "The absolute path in the workspace where the object will be imported",
                "required": True,
            },
            "format": {
                "type": StringType,
                "display_name": "Format",
                "short_desc": "The import format",
                "desc": "The format of the content to import",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "SOURCE", "desc": "Source code format"},
                    <AllowedValueInfo>{"value": "HTML", "desc": "HTML format"},
                    <AllowedValueInfo>{"value": "JUPYTER", "desc": "Jupyter notebook format"},
                    <AllowedValueInfo>{"value": "DBC", "desc": "Databricks archive format"},
                    <AllowedValueInfo>{"value": "AUTO", "desc": "Auto-detect format"},
                ),
            },
            "language": {
                "type": StringOrNothingType,
                "display_name": "Language",
                "short_desc": "The notebook language",
                "desc": "The language of the notebook content; required when `format` is `SOURCE`",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "SCALA", "desc": "Scala language"},
                    <AllowedValueInfo>{"value": "PYTHON", "desc": "Python language"},
                    <AllowedValueInfo>{"value": "SQL", "desc": "SQL language"},
                    <AllowedValueInfo>{"value": "R", "desc": "R language"},
                ),
            },
            "content": {
                "type": StringOrNothingType,
                "display_name": "Content",
                "short_desc": "Base64-encoded content",
                "desc": "The base64-encoded content to import into the workspace",
            },
            "overwrite": {
                "type": BoolOrNothingType,
                "display_name": "Overwrite",
                "short_desc": "Overwrite existing object",
                "desc": "Whether to overwrite an existing object at the target path",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Databricks import workspace action data provider
public class DatabricksImportWorkspaceDataProvider inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "import-workspace",
            "desc": "Import a notebook or file into the Databricks workspace",
            "type": "DatabricksImportWorkspaceDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        const RequestType = new DatabricksImportWorkspaceRequestDataType();

        #! Response type
        const ResponseType = AbstractDataProviderType::get(AutoHashType);
    }

    #! Creates the object with a REST client
    constructor(DatabricksRestClient::DatabricksRestClient rest) : DatabricksDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return "import-workspace";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Import a notebook or file into the Databricks workspace";
    }

    #! Makes a request and returns the response
    /** @param req the request body
        @param request_options the request options

        @return an empty hash on success
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doPost("api/2.0/workspace/import", req);
        return {};
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DatabricksDataProvider
