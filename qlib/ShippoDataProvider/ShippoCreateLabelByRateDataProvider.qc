# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoCreateLabelByRateDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShippoDataProvider module
public namespace ShippoDataProvider {

#! Data provider for creating a shipping label from a rate
public class ShippoCreateLabelByRateDataProvider inherits ShippoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-label-by-rate",
            "desc": "Creates a shipping label based on a rate object",
            "type": "ShippoCreateLabelByRateDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ShippoCreateLabelByRateRequestDataType RequestType();

        #! Response type
        static ShippoTransactionDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(ShippoRestClient::ShippoRestClient rest) : ShippoDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ShippoRestClient::ShippoRestConnection conn) : ShippoDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : ShippoDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "create-label-by-rate";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Creates a shipping label based on a rate object";
    }

    #! Makes a request and returns the response
    /** @param req the request
        @param request_options the request options (ignored)

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {};

        # Required fields
        body.rate = req.rate;

        # Optional fields
        if (exists req.label_file_type) {
            body.label_file_type = req.label_file_type;
        }
        if (exists req.metadata) {
            body.metadata = req.metadata;
        }
        if (exists req.async) {
            body.async = req.async;
        }

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: creating label with rate %y", req.rate));
        hash<auto> response = doPost("/transactions", body);
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: created transaction %y with tracking %y",
            response.object_id, response.tracking_number));
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

#! Request data type for creating a label by rate
public class ShippoCreateLabelByRateRequestDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "rate": {
                "type": StringType,
                "display_name": "Rate ID",
                "short_desc": "The rate object ID",
                "desc": "The object ID of the rate to use for creating the label. "
                    "Get this from a shipment's rates array.",
                "required": True,
            },
            "label_file_type": {
                "type": SoftStringOrNothingType,
                "display_name": "Label File Type",
                "short_desc": "Label format",
                "desc": "The file format for the shipping label",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "PNG", "desc": "PNG image"},
                    <AllowedValueInfo>{"value": "PNG_2.3x7.5", "desc": "PNG 2.3x7.5 inches"},
                    <AllowedValueInfo>{"value": "PDF", "desc": "PDF document"},
                    <AllowedValueInfo>{"value": "PDF_2.3x7.5", "desc": "PDF 2.3x7.5 inches"},
                    <AllowedValueInfo>{"value": "PDF_4x6", "desc": "PDF 4x6 inches"},
                    <AllowedValueInfo>{"value": "PDF_4x8", "desc": "PDF 4x8 inches"},
                    <AllowedValueInfo>{"value": "PDF_A4", "desc": "PDF A4"},
                    <AllowedValueInfo>{"value": "PDF_A5", "desc": "PDF A5"},
                    <AllowedValueInfo>{"value": "PDF_A6", "desc": "PDF A6"},
                    <AllowedValueInfo>{"value": "ZPLII", "desc": "ZPL thermal printer format"},
                ),
            },
            "metadata": {
                "type": SoftStringOrNothingType,
                "display_name": "Metadata",
                "short_desc": "Custom metadata",
                "desc": "A string of up to 100 characters for storing additional information",
            },
            "async": {
                "type": SoftBoolOrNothingType,
                "display_name": "Async",
                "short_desc": "Asynchronous processing",
                "desc": "If true, the label will be generated asynchronously. Default is false.",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoCreateLabelByRateRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace ShippoDataProvider
