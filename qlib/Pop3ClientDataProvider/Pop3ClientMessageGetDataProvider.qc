# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Pop3ClientMessageGetDataProvider class definition

/** Pop3ClientMessageGetDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the Pop3ClientDataProvider module
public namespace Pop3ClientDataProvider {
#! The POP3 message parent data provider
public class Pop3ClientMessageGetDataProvider inherits Pop3ClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "Pop3ClientMessageGetDataProvider",
            "supports_request": True,
        };
    }

    private {
        #! Message ID
        string id;
    }

    #! Creates the object from an POP3 connection
    constructor(Pop3Client pop3, string id) : Pop3ClientDataProviderBase(pop3) {
        self.id = id;
    }

    #! Returns the data provider name
    string getName() {
        return "get";
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Get message data provider for POP3 message %y", id);
    }

    #! Makes a request and returns the response
    /** @param req the request info
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        MailMessage::Message msg = pop3.getMessage(id);
        return msg.getInfo();
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *AbstractDataProviderType getRequestTypeImpl() {
        return AbstractDataProviderTypeMap."nothing";
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new HashDataType(TypedHash::forName("MailMessage::MailMessageInfo"));
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}