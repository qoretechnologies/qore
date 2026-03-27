# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenAiDataProvider module definition

/** OpenAiFileUploadDataProvider.qc Copyright 2024 Qore Technologies, s.r.o.

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
public class OpenAiFileUploadDataProvider inherits OpenAiDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "upload-file",
            "type": "OpenAiFileUploadDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = OpenAiFileUploadRequestType;

        #! Response type
        const ResponseType = OpenAiFileDataType;
    }

    #! Creates the object from the arguments
    constructor(*RestClientIo::RestClientIo rest) : OpenAiDataProviderCommon(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string mpboundary = replace(make_base64_string(string(now_us())), "=", "");
        string ct = sprintf("%s; boundary=%s", MimeTypeMultipartFormData, mpboundary);
        string msg = sprintf("--%s\r\nContent-Disposition: form-data; name=\"purpose\"\r\n\r\n%s\r\n", mpboundary,
            req.purpose);
        msg += sprintf("--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"
            "Content-Type: %s\r\n\r\n", mpboundary, req.file.name,
            req.file.mime_type);
        msg += parse_base64_string_to_string(req.file.content);
        msg += sprintf("\r\n--%s--\r\n", mpboundary);

        hash<auto> hdr = {
            "Content-Type": ct,
        };

        hash<auto> h = rest.restDoRequest("POST", "files", msg, hdr);
        return h.body;
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

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! The file upload request type
public class OpenAiFileUploadRequestType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "purpose": {
                "display_name": "Purpose",
                "short_desc": "The purpose of the file",
                "desc": "The purpose of the file",
                "type": AbstractDataProviderTypeMap."string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Assistants",
                        "short_desc": "For assistants and message files",
                        "desc": "For assistants and message files",
                        "value": "assistants",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Vision",
                        "short_desc": "For assistants for image file inputs",
                        "desc": "For assistants for image file inputs",
                        "value": "vision",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Batch",
                        "short_desc": "For the batch API",
                        "desc": "For the batch API",
                        "value": "batch",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Fine-Tune",
                        "short_desc": "For fine tuning",
                        "desc": "For fine tuning",
                        "value": "fine-tune",
                    },
                ),
                "default_value": "fine-tune",
            },
            "file": {
                "display_name": "File Data",
                "short_desc": "The contents of the file",
                "desc": "The contents of the file",
                "type": FileDataType,
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("OpenAiFileUploadRequest") {
        addQoreFields(Fields);
    }
}

#! File upload request type constant
public const OpenAiFileUploadRequestType = new OpenAiFileUploadRequestType();
}
