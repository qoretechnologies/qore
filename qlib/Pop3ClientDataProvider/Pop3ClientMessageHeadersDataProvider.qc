# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Pop3ClientMessageHeadersDataProvider class definition

/** Pop3ClientMessageHeadersDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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
#! The POP3 message headers data provider - retrieves only message headers using the TOP command
/** This is more efficient than retrieving the full message when only headers are needed.

    @note This requires server support for the TOP command (RFC 2449)
*/
public class Pop3ClientMessageHeadersDataProvider inherits Pop3ClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "Pop3ClientMessageHeadersDataProvider",
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        } + {
            "name": "headers",
            "desc": "Retrieves only the headers of a POP3 message (more efficient than full retrieval)",
        });
    }

    private {
        #! Message ID
        string id;
    }

    #! Creates the object from a POP3 connection
    constructor(Pop3Client pop3, string id) : Pop3ClientDataProviderBase(pop3) {
        self.id = id;
    }

    #! Returns the data provider name
    string getName() {
        return "headers";
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Headers-only data provider for POP3 message %y (uses TOP command)", id);
    }

    #! Makes a request and returns the response
    /** @param req the request info
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request (message headers as MailMessageInfo)
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        MailMessage::Message msg = pop3.getMessageHeaders(id);
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
