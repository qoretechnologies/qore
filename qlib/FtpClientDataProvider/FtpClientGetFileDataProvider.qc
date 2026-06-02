# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FtpClientDataProvider module definition

/** FtpClientGetFileDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the FtpClientDataProvider module
public namespace FtpClientDataProvider {
#! The FTP client get file data provider class
public class FtpClientGetFileDataProvider inherits AbstractDataProvider {
    public {
        #! FTP connection
        FtpClient ftp;

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-file",
            "desc": "FTP get file data provider; gets a file in the given location",
            "type": "FtpClientGetFileDataProvider",
            "constructor_options": FtpClientDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new FtpClientGetFileRequestDataType();

        #! Response type
        const ResponseType = new FtpClientGetFileResponseDataType();
    }

    #! Gets the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", FtpClientDataProvider::ConstructorOptions, options);
        ftp = FtpClientDataProvider::getFtpClientConnection(options);
    }

    #! Gets the object from an FTP connection
    constructor(FtpClient ftp) {
        self.ftp = ftp;
    }

    #! Returns the data provider name
    string getName() {
        return "get-file";
    }

    #! Makes a request and returns the response
    /** @param req the request info
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string path = FtpClientDataProvider::getPath(ftp, req.path);
        OutputStream out = req.text
            ? new StringOutputStream(req.encoding)
            : new BinaryOutputStream();
        ftp.get(remote_path: path, output_stream: out);
        return {
            "path": path,
            "data": req.text
                ? cast<StringOutputStream>(out).getData()
                : cast<BinaryOutputStream>(out).getData(),
        };
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
