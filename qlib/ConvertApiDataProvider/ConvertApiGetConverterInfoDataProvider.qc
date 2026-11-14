# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConvertApiGetConverterInfoDataProvider.qc ConvertAPI get-converter-info action data provider

/*  ConvertApiGetConverterInfoDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ConvertApiDataProvider module
public namespace ConvertApiDataProvider {


#! Request type for the get-converter-info action
public class ConvertApiGetConverterInfoRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = ConvertApiFormatFields;
    }

    #! Creates the type
    constructor() : HashDataType("ConvertApiGetConverterInfoRequest") {
        addQoreFields(Fields);
    }
}

#! ConvertAPI get-converter-info action data provider
/** Returns the description and the parameters of a converter from the \c info endpoint.
*/
public class ConvertApiGetConverterInfoDataProvider inherits ConvertApiDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-converter-info",
            "desc": "Returns the description and the parameters of a ConvertAPI converter",
            "type": "ConvertApiGetConverterInfoDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! The request type
        static ConvertApiGetConverterInfoRequestDataType RequestType();

        #! The response type
        static ConvertApiConverterInfoResponseDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(ConvertApiRestClient::ConvertApiRestClientIo rest) : ConvertApiDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "get-converter-info";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Returns the description and the parameters of a ConvertAPI converter";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return ConvertApiGetConverterInfoDataProvider::processConverterInfo(getConverterInfo(req.from, req.to));
    }

    #! Returns the converter information in the format of the response type
    /** The parameter groups are flattened into one list of parameters, and the authentication parameters are left
        out, as the API token of the connection is used.

        @param info the converter hash returned by the \c info endpoint

        @return the converter information
    */
    static hash<auto> processConverterInfo(hash<auto> info) {
        list<auto> params = ();
        foreach hash<auto> group in (info.ConverterParameterGroups) {
            if (group.Name == "Authentication") {
                continue;
            }
            foreach hash<auto> param in (group.ConverterParameters) {
                params += {
                    "Name": param.Name,
                    "Label": param.Label,
                    "Description": param.DescriptionMarkdown ?? param.Description,
                    "GroupName": param.GroupName ?? group.Name,
                    "Type": param.Type,
                    "Required": param.Required,
                    "Array": param.Array,
                    "Default": param.Default,
                    "Values": param.Values,
                };
            }
        }
        return info{"Name", "Title", "Description", "ConverterGroup", "Tags", "SourceFileFormats",
            "SourceExtensions", "DestinationFileFormats"} + {
            "Parameters": params,
        };
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ConvertApiDataProvider
