# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreAddSaleAttachmentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the Cin7CoreDataProvider module
public namespace Cin7CoreDataProvider {

#! Data provider for adding an attachment to a sale
public class Cin7CoreAddSaleAttachmentDataProvider inherits Cin7CoreDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "add-sale-attachment",
            "desc": "Adds an attachment to a sale in Cin7Core",
            "type": "Cin7CoreAddSaleAttachmentDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static Cin7CoreAddSaleAttachmentRequestDataType RequestType();

        #! Response type
        static Cin7CoreSaleAttachmentDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(Cin7CoreRestClient::Cin7CoreRestClient rest) : Cin7CoreDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(Cin7CoreRestClient::Cin7CoreRestConnection conn) : Cin7CoreDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : Cin7CoreDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "add-sale-attachment";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Adds an attachment to a sale in Cin7Core";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.SaleID.val(), "SaleID is required");
        @assert(req.FileName.val(), "FileName is required");
        @assert(req.ContentBase64.val(), "ContentBase64 is required");

        hash<auto> body = {
            "SaleID": req.SaleID,
            "FileName": req.FileName,
            "ContentBase64": req.ContentBase64,
        };

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: adding attachment %y to sale %y", req.FileName, req.SaleID));
        hash<auto> response = doPost("/SaleAttachment", body);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: added attachment %y", response.ID));
        return response;
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
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Request data type for adding a sale attachment
public class Cin7CoreAddSaleAttachmentRequestDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "SaleID": {
                "type": StringType,
                "display_name": "Sale ID",
                "short_desc": "The sale ID",
                "desc": "The unique identifier (GUID) of the sale to attach to",
                "required": True,
                "ref_data": "sales",
            },
            "FileName": {
                "type": StringType,
                "display_name": "File Name",
                "short_desc": "Attachment file name",
                "desc": "The name of the file being attached",
                "required": True,
            },
            "ContentBase64": {
                "type": StringType,
                "display_name": "Content (Base64)",
                "short_desc": "File content in Base64",
                "desc": "The file content encoded as Base64 string",
                "required": True,
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreAddSaleAttachmentRequestDataType") {
        addQoreFields(Fields);
    }
}

#! Response data type for sale attachment
public class Cin7CoreSaleAttachmentDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "ID": {
                "type": StringType,
                "display_name": "Attachment ID",
                "short_desc": "Unique attachment identifier",
                "desc": "The unique identifier (GUID) of the attachment",
            },
            "SaleID": {
                "type": StringOrNothingType,
                "display_name": "Sale ID",
                "short_desc": "Associated sale ID",
                "desc": "The sale this attachment belongs to",
            },
            "FileName": {
                "type": StringOrNothingType,
                "display_name": "File Name",
                "short_desc": "File name",
                "desc": "The name of the attached file",
            },
            "ContentType": {
                "type": StringOrNothingType,
                "display_name": "Content Type",
                "short_desc": "MIME type",
                "desc": "The MIME type of the attachment",
            },
            "ContentLength": {
                "type": IntOrNothingType,
                "display_name": "Content Length",
                "short_desc": "File size",
                "desc": "The size of the attachment in bytes",
            },
            "DownloadUrl": {
                "type": StringOrNothingType,
                "display_name": "Download URL",
                "short_desc": "URL to download",
                "desc": "URL to download the attachment",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreSaleAttachmentDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
